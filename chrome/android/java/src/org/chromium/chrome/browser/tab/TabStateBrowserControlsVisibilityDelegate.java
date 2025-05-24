// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.annotation.SuppressLint;
import android.os.Handler;
import android.os.Message;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;
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

    /** The maximum amount of time to wait for a page to load before entering fullscreen. */
    private static final long MAX_FULLSCREEN_LOAD_DELAY_MS = 3000;

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

    private final Set<Long> mOutstandingNavigations = new HashSet<>();

    /**
     * Basic constructor.
     *
     * @param tab The associated {@link Tab}.
     */
    public TabStateBrowserControlsVisibilityDelegate(Tab tab) {
        super(BrowserControlsState.BOTH);

        mTab = (TabImpl) tab;

        mTab.addObserver(
                new EmptyTabObserver() {
                    @SuppressLint("HandlerLeak")
                    private final Handler mHandler =
                            new Handler() {
                                @Override
                                public void handleMessage(Message msg) {
                                    if (msg == null) return;
                                    if (msg.what == MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD) {
                                        if (!mIsFullscreenWaitingForLoad) return;
                                        updateWaitingForLoad(false);
                                    }
                                }
                            };

                    private long getLoadDelayMs() {
                        return sDisableLoadingCheck ? 0 : MAX_FULLSCREEN_LOAD_DELAY_MS;
                    }

                    private void cancelEnableFullscreenLoadDelay() {
                        mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
                        updateWaitingForLoad(false);
                    }

                    private void scheduleEnableFullscreenLoadDelayIfNecessary() {
                        if (mIsFullscreenWaitingForLoad
                                && !mHandler.hasMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD)) {
                            mHandler.sendEmptyMessageDelayed(
                                    MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD, getLoadDelayMs());
                        }
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        onWebContentsUpdated(tab.getWebContents());
                    }

                    @Override
                    public void onWebContentsSwapped(
                            Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                        if (!didStartLoad) return;

                        // As we may have missed the main frame commit notification for the
                        // swapped web contents, schedule the enabling of fullscreen now.
                        scheduleEnableFullscreenLoadDelayIfNecessary();
                    }

                    @Override
                    public void onDidStartNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        if (!ChromeFeatureList.sControlsVisibilityFromNavigations.isEnabled()) {
                            return;
                        }

                        if (navigation.isSameDocument()) return;

                        boolean changed = mOutstandingNavigations.add(navigation.getNavigationId());
                        RecordHistogram.recordBooleanHistogram(
                                "Android.BrowserControls.OutstandingChangedOnStart", changed);

                        mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
                        boolean safe = DomDistillerUrlUtils.isDistilledPage(navigation.getUrl());
                        updateWaitingForLoad(!safe);
                    }

                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        if (ChromeFeatureList.sControlsVisibilityFromNavigations.isEnabled()) {
                            if (navigation.isSameDocument()) return;

                            boolean changed =
                                    mOutstandingNavigations.remove(navigation.getNavigationId());
                            RecordHistogram.recordBooleanHistogram(
                                    "Android.BrowserControls.OutstandingChangedOnFinish", changed);

                            if (mOutstandingNavigations.isEmpty()) {
                                mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
                                mHandler.sendEmptyMessageDelayed(
                                        MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD, getLoadDelayMs());
                            }
                            RecordHistogram.recordCount100Histogram(
                                    "Android.BrowserControls.OutstandingNavigationsOnFinish",
                                    mOutstandingNavigations.size());
                        } else {
                            if (!navigation.hasCommitted()) return;
                            mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
                            mHandler.sendEmptyMessageDelayed(
                                    MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD, getLoadDelayMs());
                        }
                    }

                    @Override
                    public void onPageLoadStarted(Tab tab, GURL url) {
                        if (!ChromeFeatureList.sControlsVisibilityFromNavigations.isEnabled()) {
                            mHandler.removeMessages(MSG_ID_ENABLE_FULLSCREEN_AFTER_LOAD);
                            updateWaitingForLoad(!DomDistillerUrlUtils.isDistilledPage(url));
                        }
                    }

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        // Handle the case where a commit or prerender swap notification failed to
                        // arrive and the enable fullscreen message was never enqueued.
                        if (!ChromeFeatureList.sControlsVisibilityFromNavigations.isEnabled()) {
                            scheduleEnableFullscreenLoadDelayIfNecessary();
                        }
                    }

                    @Override
                    public void onPageLoadFailed(Tab tab, int errorCode) {
                        // TODO(crbug.com/40926082): Associate events with navigation ids or
                        // urls, so that we can fully unlock controls here possible here.
                        // May have already received the start of a different navigation. Do not
                        // cancel the outstanding delay. See https://crbug.com/1447237.
                        if (!ChromeFeatureList.sControlsVisibilityFromNavigations.isEnabled()) {
                            scheduleEnableFullscreenLoadDelayIfNecessary();
                        }
                    }

                    @Override
                    public void onHidden(Tab tab, @TabHidingType int type) {
                        cancelEnableFullscreenLoadDelay();
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
                        super.onDestroyed(tab);

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
        ImeAdapter.fromWebContents(mWebContents).addEventObserver(this);
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
        if (url.getScheme().equals(UrlConstants.CHROME_SCHEME)
                || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME)) {
            enableHidingBrowserControls = false;
            recordBrowserControlsLockReason(LockReason.CHROME_URL);
            flags |= (1 << (int) LockReason.CHROME_URL);
        }
        if (SecurityStateModel.isContentDangerous(mTab.getWebContents())) {
            enableHidingBrowserControls = false;
            recordBrowserControlsLockReason(LockReason.TAB_CONTENT_DANGEROUS);
            flags |= (1 << (int) LockReason.TAB_CONTENT_DANGEROUS);
        }
        if (mIsFocusedNodeEditable) {
            enableHidingBrowserControls = false;
            recordBrowserControlsLockReason(LockReason.EDITABLE_NODE_FOCUS);
            flags |= (1 << (int) LockReason.EDITABLE_NODE_FOCUS);
        }
        if (mTab.isShowingErrorPage() || mTab.isRendererUnresponsive()) {
            enableHidingBrowserControls = false;
            recordBrowserControlsLockReason(LockReason.TAB_ERROR);
            flags |= (1 << (int) LockReason.TAB_ERROR);
        }
        if (mTab.isHidden()) {
            enableHidingBrowserControls = false;
            recordBrowserControlsLockReason(LockReason.TAB_HIDDEN);
            flags |= (1 << (int) LockReason.TAB_HIDDEN);
        }
        if (mIsFullscreenWaitingForLoad) {
            enableHidingBrowserControls = false;
            recordBrowserControlsLockReason(LockReason.FULLSCREEN_LOADING);
            flags |= (1 << (int) LockReason.FULLSCREEN_LOADING);
        }
        // TODO(tedchoc): AccessibilityUtil and DeviceClassManager checks do not belong in Tab
        //                logic.  They should be moved to application level checks.
        if (ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
            enableHidingBrowserControls = false;
            recordBrowserControlsLockReason(LockReason.A11Y_ENABLED);
            flags |= (1 << (int) LockReason.A11Y_ENABLED);
        }
        if (!DeviceClassManager.enableFullscreen()) {
            enableHidingBrowserControls = false;
            recordBrowserControlsLockReason(LockReason.FULLSCREEN_DISABLED);
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
     * @return The constraints that determine the visibility of the browser controls.
     */
    protected @BrowserControlsState int calculateVisibilityConstraints() {
        return enableHidingBrowserControls()
                ? BrowserControlsState.BOTH
                : BrowserControlsState.SHOWN;
    }

    /** Updates the browser controls visibility constraints based on the current configuration. */
    protected void updateVisibilityConstraints() {
        set(calculateVisibilityConstraints());
    }

    /** Disables the logic that prevents hiding the top controls during page load for testing. */
    public static void disablePageLoadDelayForTests() {
        sDisableLoadingCheck = true;
    }

    // ImeEventObserver

    @Override
    public void onNodeAttributeUpdated(boolean editable, boolean password) {
        mIsFocusedNodeEditable = editable;
        updateVisibilityConstraints();
    }

    private static void recordBrowserControlsLockReason(@LockReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.BrowserControls.LockedByTabState.Reason", reason, LockReason.NUM_TOTAL);
    }
}
