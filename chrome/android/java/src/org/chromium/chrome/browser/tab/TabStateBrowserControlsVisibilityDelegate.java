// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.build.NullUtil.assertNonNull;

import android.annotation.SuppressLint;
import android.os.Handler;
import android.os.Message;
import android.util.ArraySet;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.DeviceInfo;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProviderSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.web_distiller.core.WebDistillerUrlUtils;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Set;

/**
 * Determines the desired visibility of the browser controls based on the current state of a given
 * tab.
 */
@NullMarked
public class TabStateBrowserControlsVisibilityDelegate extends BrowserControlsVisibilityDelegate
        implements ImeEventObserver {
    private static final String TAG = "BrowserControls";
    protected static final int MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD = 1;
    private static final int MSG_ID_SHOW_CONTROLS_ON_SLOW_LOAD = 2;

    /** The maximum amount of time to wait for a page to load before entering fullscreen. */
    private static final long MAX_FULLSCREEN_LOAD_DELAY_MS = 3000;

    private static final long SECURITY_MIN_LOCKOUT_DURATION_MS = 1000;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        LockReason.CHROME_URL,
        LockReason.TAB_CONTENT_DANGEROUS,
        LockReason.EDITABLE_NODE_FOCUS,
        LockReason.TAB_ERROR,
        LockReason.TAB_HIDDEN,
        LockReason.FULLSCREEN_LOADING,
        LockReason.A11Y_ENABLED,
        LockReason.FULLSCREEN_DISABLED,
        LockReason.NUM_TOTAL
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface LockReason {
        int CHROME_URL = 0;
        int TAB_CONTENT_DANGEROUS = 1;
        int EDITABLE_NODE_FOCUS = 2;
        int TAB_ERROR = 3;
        int TAB_HIDDEN = 4;
        int FULLSCREEN_LOADING = 5;
        int A11Y_ENABLED = 6;
        int FULLSCREEN_DISABLED = 7;

        int NUM_TOTAL = 8;
    }

    private static boolean sDisableLoadingCheck;

    protected final TabImpl mTab;
    private @Nullable WebContents mWebContents;

    private boolean mIsFullscreenWaitingForLoad;
    private boolean mIsFocusedNodeEditable;

    // There shouldn't be very many outstanding navigations at once so an ArraySet should be fine.
    private final Set<Long> mOutstandingNavigations = new ArraySet<>();

    // Outstanding navigations that skipped the load lock (ClankOmniboxMinimization). They take the
    // lock if they do not finish within the slow-load fallback delay, or if the tab is hidden.
    // Always a subset of mOutstandingNavigations.
    private final Set<Long> mKeepHiddenNavigationIds = new ArraySet<>();

    @SuppressLint("HandlerLeak")
    private final Handler mHandler =
            new Handler() {
                @Override
                public void handleMessage(Message msg) {
                    if (msg == null) return;
                    if (msg.what == MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD) {
                        if (!mIsFullscreenWaitingForLoad) return;
                        updateWaitingForLoad(false);
                    } else if (msg.what == MSG_ID_SHOW_CONTROLS_ON_SLOW_LOAD) {
                        lockKeptHiddenNavigations();
                    }
                }
            };

    /**
     * Basic constructor.
     *
     * @param tab The associated {@link Tab}.
     */
    public TabStateBrowserControlsVisibilityDelegate(Tab tab) {
        mTab = (TabImpl) tab;
        mTab.addObserver(
                new TabObserver() {
                    @Override
                    public void onContentChanged(Tab tab) {
                        onWebContentsUpdated(tab.getWebContents());
                    }

                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        if (navigation.isSameDocument()) return;

                        boolean changed = mOutstandingNavigations.add(navigation.getNavigationId());
                        RecordHistogram.recordBooleanHistogram(
                                "Android.BrowserControls.OutstandingChangedOnStart", changed);

                        mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);

                        int fallbackShowAfterMs =
                                BrowserControlsUtils.getSameOriginOmniboxFallbackShowAfterMs();
                        if (shouldKeepControlsHiddenOnStart(navigation, fallbackShowAfterMs)) {
                            // Keep any existing lock: another navigation may commit cross-origin.
                            mKeepHiddenNavigationIds.add(navigation.getNavigationId());
                            // Keep the deadline of the first kept-hidden navigation while any
                            // kept-hidden navigation is in flight.
                            if (!mHandler.hasMessages(MSG_ID_SHOW_CONTROLS_ON_SLOW_LOAD)) {
                                mHandler.sendEmptyMessageDelayed(
                                        MSG_ID_SHOW_CONTROLS_ON_SLOW_LOAD, fallbackShowAfterMs);
                            }
                            return;
                        }
                        boolean safe = WebDistillerUrlUtils.isDistilledPage(navigation.getUrl());
                        updateWaitingForLoad(!safe);
                    }

                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        if (navigation.isSameDocument()) return;

                        boolean changed =
                                mOutstandingNavigations.remove(navigation.getNavigationId());
                        RecordHistogram.recordBooleanHistogram(
                                "Android.BrowserControls.OutstandingChangedOnFinish", changed);

                        onKeptHiddenNavigationFinished(navigation);

                        if (mOutstandingNavigations.isEmpty()) {
                            mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
                            mHandler.sendEmptyMessageDelayed(
                                    MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD, getLoadDelayMs());
                        }
                        RecordHistogram.recordCount100Histogram(
                                "Android.BrowserControls.OutstandingNavigationsOnFinish",
                                mOutstandingNavigations.size());
                    }

                    @Override
                    public void onHidden(Tab tab, @TabHidingType int type) {
                        mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
                        // While a main frame navigation is still in flight the controls must stay
                        // locked when the tab is shown again, otherwise hiding and showing the tab
                        // lets a page scroll the real URL away before the navigation commits. The
                        // lock is released as usual once the outstanding navigations finish.
                        // Navigations that skipped the lock take it now.
                        lockKeptHiddenNavigations();
                        if (!mOutstandingNavigations.isEmpty()) return;
                        updateWaitingForLoad(false);
                    }

                    @Override
                    public void onSSLStateUpdated(Tab tab) {
                        updateVisibilityConstraints();
                    }

                    @Override
                    public void onRendererResponsiveStateChanged(Tab tab, boolean isResponsive) {
                        updateVisibilityConstraints();
                    }

                    @Override
                    public void onShown(Tab tab, int type) {
                        updateVisibilityConstraints();
                    }

                    @Override
                    public void onActivityAttachmentChanged(
                            Tab tab, @Nullable WindowAndroid window) {
                        if (window != null) updateVisibilityConstraints();
                    }

                    @Override
                    public void onCrash(Tab tab) {
                        mIsFocusedNodeEditable = false;
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        // Remove pending handler actions to prevent memory leaks.
                        mHandler.removeCallbacksAndMessages(null);
                    }
                });
        onWebContentsUpdated(mTab.getWebContents());
        updateVisibilityConstraints();
    }

    private void onWebContentsUpdated(@Nullable WebContents contents) {
        if (mWebContents == contents) return;
        mWebContents = contents;

        if (mWebContents == null) return;

        ImeAdapter adapter = assertNonNull(ImeAdapter.fromWebContents(mWebContents));

        // Gracefully handle a null adapter in non-debug builds.
        if (adapter != null) adapter.addEventObserver(this);
    }

    private void updateWaitingForLoad(boolean waiting) {
        if (mIsFullscreenWaitingForLoad == waiting) return;
        mIsFullscreenWaitingForLoad = waiting;
        updateVisibilityConstraints();
    }

    private boolean enableHidingBrowserControls() {
        WebContents webContents = mTab.getWebContents();
        if (webContents == null || webContents.isDestroyed()) return false;

        GURL url = mTab.getUrl();
        boolean enableHidingBrowserControls = true;
        int flags = 0;
        if (UrlUtilities.isChromeScheme(url)) {
            enableHidingBrowserControls = false;
            flags |= (1 << (int) LockReason.CHROME_URL);
        }
        if (SecurityStateModel.isContentDangerous(mTab.getWebContents())) {
            enableHidingBrowserControls = false;
            flags |= (1 << (int) LockReason.TAB_CONTENT_DANGEROUS);
        }
        if (mIsFocusedNodeEditable) {
            enableHidingBrowserControls = false;
            flags |= (1 << (int) LockReason.EDITABLE_NODE_FOCUS);
        }
        if (mTab.isShowingErrorPage() || mTab.isRendererUnresponsive()) {
            enableHidingBrowserControls = false;
            flags |= (1 << (int) LockReason.TAB_ERROR);
        }
        if (mTab.isHidden()) {
            enableHidingBrowserControls = false;
            flags |= (1 << (int) LockReason.TAB_HIDDEN);
        }
        if (mIsFullscreenWaitingForLoad) {
            enableHidingBrowserControls = false;
            flags |= (1 << (int) LockReason.FULLSCREEN_LOADING);
        }
        // TODO(tedchoc): AccessibilityUtil and DeviceClassManager checks do not belong in Tab
        //                logic.  They should be moved to application level checks.
        if (ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
            enableHidingBrowserControls = false;
            flags |= (1 << (int) LockReason.A11Y_ENABLED);
        }
        if (!DeviceClassManager.enableFullscreen()) {
            enableHidingBrowserControls = false;
            flags |= (1 << (int) LockReason.FULLSCREEN_DISABLED);
        }

        RecordHistogram.recordBooleanHistogram(
                "Android.BrowserControls.LockedByTabState", !enableHidingBrowserControls);

        if (ChromeFeatureList.sBrowserControlsDebugging.isEnabled()) {
            Log.i(TAG, "Browser controls hiding reason flags: " + Integer.toBinaryString(flags));
        }

        return enableHidingBrowserControls;
    }

    /**
     * Returns whether a navigation can skip the load lock (ClankOmniboxMinimization Exp A). A
     * fallback delay of 0 ms or less shows the controls at once, so the navigation takes the lock.
     *
     * @param fallbackShowAfterMs The slow-load fallback delay, which is 0 if Exp A is off.
     */
    private boolean shouldKeepControlsHiddenOnStart(
            NavigationHandle navigation, int fallbackShowAfterMs) {
        return fallbackShowAfterMs > 0
                && canKeepOmniboxHiddenInTab()
                && areBrowserControlsOffScreen()
                && isSameOriginHttpOrHttpsNavigation(navigation)
                && isEligibleNavigationType(navigation);
    }

    /**
     * Returns whether the type of a navigation lets the Omnibox stay hidden. These navigations are
     * eligible: back and forward from the browser UI, and navigations that the page itself starts
     * in its own origin. All other navigations keep the load lock.
     *
     * <p>A same-origin iframe or another same-origin window also passes, because Java has no
     * initiator-frame signal.
     */
    private static boolean isEligibleNavigationType(NavigationHandle navigation) {
        // Session restore also sets isHistory().
        if (navigation.isRestore()) return false;
        // Browser-initiated: only UI back/forward, which includes BFCache restores. Browser
        // reloads (toolbar, pull-to-refresh) are not history, so they keep the lock.
        if (!navigation.isRendererInitiated()) return navigation.isHistory();
        // Renderer-initiated history: the initiator origin comes from the history entry, so the
        // frame that called history.back() (main frame or iframe) is unknown. Keep the lock.
        if (navigation.isHistory()) return false;
        // Defense in depth: renderer-initiated FROM_API loads today have a null, opaque, or
        // extension initiator, so isInitiatedBySameOrigin() also locks them.
        if ((navigation.pageTransition() & PageTransition.FROM_API) != 0) return false;
        // Only the page's own origin. Cross-origin, sandboxed, and data: iframes, and loads with an
        // opaque initiator (Tab Group Sync, web sign-in redirect) keep the lock. A
        // location.reload() call from the page passes, because a cross-origin frame cannot reload
        // the top frame.
        return isInitiatedBySameOrigin(navigation);
    }

    /** Returns whether the initiator origin of a navigation is the origin of its URL. */
    private static boolean isInitiatedBySameOrigin(NavigationHandle navigation) {
        Origin initiator = navigation.getInitiatorOrigin();
        if (initiator == null) return false;
        return initiator.equals(Origin.create(navigation.getUrl()));
    }

    private boolean isSameOriginHttpOrHttpsNavigation(NavigationHandle navigation) {
        WebContents webContents = mTab.getWebContents();
        if (webContents == null) return false;
        GURL targetUrl = navigation.getUrl();
        GURL currentUrl = webContents.getLastCommittedUrl();
        // Check both schemes: the origin of a filesystem: URL is its inner origin.
        return UrlUtilities.isHttpOrHttps(targetUrl)
                && UrlUtilities.isHttpOrHttps(currentUrl)
                && targetUrl.getOrigin().equals(currentUrl.getOrigin());
    }

    /** Returns whether the Omnibox can stay hidden across navigations in this tab. */
    private boolean canKeepOmniboxHiddenInTab() {
        // A hidden tab reads the controls offset of the foreground tab, which does not show
        // whether the user saw the URL of this tab. Keep the load lock for hidden tabs.
        if (!mTab.isTabInBrowser() || mTab.isHidden()) return false;
        if (DeviceInfo.isAutomotive() || DeviceInfo.isTV() || DeviceInfo.isXr()) return false;
        BrowserControlsStateProvider provider = getBrowserControlsStateProvider();
        return provider != null && provider.getControlsPosition() == ControlsPosition.TOP;
    }

    private boolean areBrowserControlsOffScreen() {
        BrowserControlsStateProvider provider = getBrowserControlsStateProvider();
        return provider != null && BrowserControlsUtils.areBrowserControlsOffScreen(provider);
    }

    private @Nullable BrowserControlsStateProvider getBrowserControlsStateProvider() {
        // Not cached: the provider belongs to the activity, and the tab can change windows.
        return BrowserControlsStateProviderSupplier.getValueOrNullFrom(mTab.getWindowAndroid());
    }

    private void onKeptHiddenNavigationFinished(NavigationHandle navigation) {
        if (!mKeepHiddenNavigationIds.remove(navigation.getNavigationId())) return;
        if (mKeepHiddenNavigationIds.isEmpty()) {
            mHandler.removeMessages(MSG_ID_SHOW_CONTROLS_ON_SLOW_LOAD);
        }
        // TODO(crbug.com/579544758): Re-check the origin and error state at commit.
    }

    /** Takes the load lock that the kept-hidden navigations skipped. */
    private void lockKeptHiddenNavigations() {
        if (mKeepHiddenNavigationIds.isEmpty()) return;
        mHandler.removeMessages(MSG_ID_SHOW_CONTROLS_ON_SLOW_LOAD);
        mKeepHiddenNavigationIds.clear();
        // Released getLoadDelayMs() after the last outstanding navigation finishes.
        updateWaitingForLoad(true);
    }

    /** Returns the constraints that determine the visibility of the browser controls. */
    protected @BrowserControlsState int calculateVisibilityConstraints() {
        return enableHidingBrowserControls()
                ? BrowserControlsState.BOTH
                : BrowserControlsState.SHOWN;
    }

    /** Updates the browser controls visibility constraints based on the current configuration. */
    protected void updateVisibilityConstraints() {
        set(calculateVisibilityConstraints());
    }

    @VisibleForTesting
    static long getLoadDelayMs() {
        if (sDisableLoadingCheck) {
            return 0;
        }
        if (ChromeFeatureList.sBrowserControlsLockoutDuration.isEnabled()) {
            return MathUtils.clamp(
                    ChromeFeatureList.sBrowserControlsLockoutFullscreenLoadDelayMs.getValue(),
                    SECURITY_MIN_LOCKOUT_DURATION_MS,
                    MAX_FULLSCREEN_LOAD_DELAY_MS);
        }
        return MAX_FULLSCREEN_LOAD_DELAY_MS;
    }

    /** Disables the logic that prevents hiding the top controls during page load for testing. */
    public static void disablePageLoadDelayForTests() {
        sDisableLoadingCheck = true;
        ResettersForTesting.register(() -> sDisableLoadingCheck = false);
    }

    // ImeEventObserver

    @Override
    public void onNodeAttributeUpdated(boolean editable, boolean password) {
        mIsFocusedNodeEditable = editable;
        updateVisibilityConstraints();
    }
}
