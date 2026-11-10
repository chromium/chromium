// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.handoff;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.HandoffActivityData;
import android.app.HandoffActivityDataRequestInfo;
import android.app.HandoffActivityParams;
import android.content.ComponentName;
import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.PersistableBundle;
import android.os.SystemClock;
import android.os.UserManager;
import android.provider.Browser;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.ExternalIntentUrlChecker;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/**
 * Orchestrates Handoff integration for a {@link Activity}. This class manages the opt-in state for
 * the Activity and handles data requests from the Android platform.
 */
@NullMarked
@SuppressLint("NewApi")
public class HandoffController implements Destroyable {
    @VisibleForTesting static final int FETCH_TEXT_DEBOUNCE_MS = 300;

    @IntDef({
        HandoffEnableTrigger.TAB_SWITCH,
        HandoffEnableTrigger.URL_NAVIGATION,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface HandoffEnableTrigger {
        int TAB_SWITCH = 0;
        int URL_NAVIGATION = 1;
    }

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(HandoffScrollAttachmentStatus)
    @IntDef({
        HandoffScrollAttachmentStatus.TEXT_ATTACHED,
        HandoffScrollAttachmentStatus.MISSED_DUE_TO_DEBOUNCE,
        HandoffScrollAttachmentStatus.NO_SCROLL,
        HandoffScrollAttachmentStatus.NO_TEXT,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface HandoffScrollAttachmentStatus {
        // Non-empty text fragment selector was attached to the outgoing Handoff URL.
        int TEXT_ATTACHED = 0;

        // User scrolled, but Handoff occurred during the 300ms debounce or while IPC was in flight.
        int MISSED_DUE_TO_DEBOUNCE = 1;

        // Page was unscrolled at the top (scrollOffsetY == 0).
        int NO_SCROLL = 2;

        // Page was scrolled (scrollOffsetY > 0), but Blink found no text at the reading position.
        int NO_TEXT = 3;

        int NUM_ENTRIES = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:HandoffScrollAttachmentStatus)

    private final Activity mActivity;
    private final TabModelSelector mTabModelSelector;
    private final ActivityTabProvider mActivityTabProvider;
    private final Delegate mDelegate;
    private final ActivityTabTabObserver mActivityTabTabObserver;
    private final Callback<@Nullable Tab> mCurrentTabObserver =
            _ -> updateHandoffState(HandoffEnableTrigger.TAB_SWITCH);

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Runnable mFetchRunnable = () -> fetchActiveTabTextFragment();
    private @Nullable WebContents mActiveWebContents;
    private String mActiveTabTextFragment = "";

    // True while scrolling or while a debounced Mojo selector fetch is pending/in-flight, so
    // onShown and onPageLoadFinished know whether to re-trigger extraction.
    private boolean mHasPendingScrollFetch;

    // Monotonically increasing token incremented when starting or canceling a fetch to ignore
    // stale Mojo callbacks from earlier in-flight requests.
    private int mFetchGeneration;

    private @Nullable GURL mTabLastUrlSeen;

    private final GestureStateListener mGestureStateListener =
            new GestureStateListener() {
                @Override
                public void onScrollStarted(
                        int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                    cancelPendingFetch();
                    mHasPendingScrollFetch = true;
                }

                @Override
                public void onFlingStartGesture(
                        int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                    cancelPendingFetch();
                    mHasPendingScrollFetch = true;
                }

                @Override
                public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                    scheduleTextFragmentFetch(scrollOffsetY);
                }

                @Override
                public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
                    scheduleTextFragmentFetch(scrollOffsetY);
                }

                @Override
                public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {
                    // Handles #anchor/JS scrolls, pinch-zoom, and scroll-anchoring layout shifts.
                    scheduleTextFragmentFetch(scrollOffsetY);
                }
            };

    /** Delegate interface for Android Handoff system APIs. */
    interface Delegate {
        void setHandoffEnabled(Activity activity, boolean enabled);

        boolean isHandoffEnabled(Activity activity);

        @Nullable Object buildHandoffActivityData(Activity activity, String url);
    }

    private static class DelegateImpl implements Delegate {
        @Override
        public void setHandoffEnabled(Activity activity, boolean enabled) {
            HandoffActivityParams params =
                    new HandoffActivityParams.Builder()
                            .setAllowHandoffWithoutPackageInstalled(true)
                            .build();
            activity.setHandoffEnabled(enabled, params);
        }

        @Override
        public boolean isHandoffEnabled(Activity activity) {
            return activity.isHandoffEnabled();
        }

        @Override
        public Object buildHandoffActivityData(Activity activity, String url) {
            PersistableBundle extras = new PersistableBundle();
            extras.putBoolean(IntentHandler.EXTRA_INVOKED_FROM_HANDOFF, true);
            extras.putString(IntentHandler.EXTRA_HANDOFF_URL, url);
            extras.putString(Browser.EXTRA_APPLICATION_ID, activity.getPackageName());
            extras.putBoolean(Browser.EXTRA_CREATE_NEW_TAB, true);

            return new HandoffActivityData.Builder(
                            new ComponentName(
                                    activity,
                                    BrowserIntentUtils.CHROME_LAUNCHER_ACTIVITY_CLASS_NAME))
                    .setFallbackUri(Uri.parse(url))
                    .setExtras(extras)
                    .build();
        }
    }

    public HandoffController(
            Activity activity,
            TabModelSelector tabModelSelector,
            ActivityTabProvider activityTabProvider) {
        this(activity, tabModelSelector, activityTabProvider, new DelegateImpl());
    }

    @VisibleForTesting
    HandoffController(
            Activity activity,
            TabModelSelector tabModelSelector,
            ActivityTabProvider activityTabProvider,
            Delegate delegate) {
        mActivity = activity;
        mTabModelSelector = tabModelSelector;
        mActivityTabProvider = activityTabProvider;
        mDelegate = delegate;
        mTabModelSelector.getCurrentTabSupplier().addSyncObserver(mCurrentTabObserver);
        Tab currentTab = activityTabProvider.get();
        mTabLastUrlSeen =
                (currentTab != null && !currentTab.isIncognitoBranded())
                        ? currentTab.getUrl()
                        : null;

        mActivityTabTabObserver =
                new ActivityTabTabObserver(activityTabProvider) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        cancelAndClearTextFragment();
                        updateGestureListener(tab);
                        if (tab != null && !tab.isIncognitoBranded()) {
                            mTabLastUrlSeen = tab.getUrl();
                            scheduleTextFragmentFetch();
                        } else {
                            mTabLastUrlSeen = null;
                        }
                        updateHandoffState(HandoffEnableTrigger.TAB_SWITCH);
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        // Rebind gesture listener when WebContents swaps (e.g. prerender or tab
                        // restore).
                        cancelAndClearTextFragment();
                        updateGestureListener(tab);
                        if (!tab.isIncognitoBranded()) {
                            scheduleTextFragmentFetch();
                        }
                    }

                    @Override
                    public void onShown(Tab tab, @TabSelectionType int type) {
                        // If the user scrolled and immediately backgrounded Chrome before we
                        // finished extracting the new text, fetch it now that the tab is visible.
                        if (!tab.isIncognitoBranded()
                                && (mActiveTabTextFragment.isEmpty() || mHasPendingScrollFetch)) {
                            scheduleTextFragmentFetch();
                        }
                    }

                    @Override
                    public void onUrlUpdated(Tab tab) {
                        // Ignore duplicate calls to #onUrlUpdate within the same navigation.
                        GURL currentUrl = tab.getUrl();
                        if (Objects.equals(currentUrl, mTabLastUrlSeen)) return;
                        if (tab.isIncognitoBranded()) return;
                        mTabLastUrlSeen = currentUrl;

                        cancelAndClearTextFragment();
                        scheduleTextFragmentFetch();
                        updateHandoffState(HandoffEnableTrigger.URL_NAVIGATION);
                    }

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        // When navigating back or restoring a tab, Chrome may restore the scroll
                        // position before the page finishes loading. Fetch the text once loaded.
                        if (!tab.isIncognitoBranded()
                                && (mActiveTabTextFragment.isEmpty() || mHasPendingScrollFetch)) {
                            scheduleTextFragmentFetch();
                        }
                    }
                };
    }

    private void cancelPendingFetch() {
        mHandler.removeCallbacks(mFetchRunnable);
        // Invalidate any in-flight Mojo request.
        mFetchGeneration++;
    }

    private void cancelAndClearTextFragment() {
        cancelPendingFetch();
        mActiveTabTextFragment = "";
        mHasPendingScrollFetch = false;
    }

    private void scheduleTextFragmentFetch() {
        scheduleTextFragmentFetch(getScrollOffsetY(mActivityTabProvider.get()));
    }

    private int getScrollOffsetY(@Nullable Tab tab) {
        WebContents webContents = (tab != null) ? tab.getWebContents() : null;
        RenderCoordinates coords =
                (webContents != null && !webContents.isDestroyed())
                        ? RenderCoordinates.fromWebContents(webContents)
                        : null;
        return (coords != null) ? coords.getScrollYPixInt() : 0;
    }

    private void scheduleTextFragmentFetch(int scrollOffsetY) {
        cancelPendingFetch();
        mHasPendingScrollFetch = scrollOffsetY > 0;
        if (mHasPendingScrollFetch) {
            // Wait for scrolling to settle before asking Blink to extract viewport text.
            mHandler.postDelayed(mFetchRunnable, FETCH_TEXT_DEBOUNCE_MS);
        } else {
            // At the top of the page, no scroll selector is needed.
            mActiveTabTextFragment = "";
        }
    }

    private void updateGestureListener(@Nullable Tab tab) {
        WebContents newWebContents =
                (tab != null && !tab.isIncognitoBranded()) ? tab.getWebContents() : null;
        if (mActiveWebContents == newWebContents) return;

        // 1. Detach the scroll listener from the previous WebContents.
        if (mActiveWebContents != null && !mActiveWebContents.isDestroyed()) {
            GestureListenerManager glm = GestureListenerManager.fromWebContents(mActiveWebContents);
            if (glm != null) {
                glm.removeListener(mGestureStateListener);
            }
        }
        // 2. Attach the scroll listener to the new active WebContents.
        mActiveWebContents = newWebContents;
        if (mActiveWebContents != null && !mActiveWebContents.isDestroyed()) {
            GestureListenerManager glm = GestureListenerManager.fromWebContents(mActiveWebContents);
            if (glm != null) {
                glm.addListener(
                        mGestureStateListener, RootScrollOffsetUpdateFrequency.ON_SCROLL_END);
            }
        }
    }

    private void fetchActiveTabTextFragment() {
        Tab tab = mActivityTabProvider.get();
        if (tab == null || !isHandoffEnabled(tab)) {
            mActiveTabTextFragment = "";
            mHasPendingScrollFetch = false;
            return;
        }

        // Skip extraction if the tab is backgrounded or its native view/renderer is not ready yet;
        // mHasPendingScrollFetch stays true so onShown or onPageLoadFinished can retry later.
        if (tab.isHidden()
                || tab.getWebContents() == null
                || tab.getWebContents().isDestroyed()
                || tab.getWebContents().getTopLevelNativeWindow() == null) {
            return;
        }

        RenderFrameHost mainFrame = tab.getWebContents().getMainFrame();
        if (mainFrame == null || !mainFrame.isRenderFrameLive()) {
            return;
        }

        // Request the text fragment for the center of the current viewport from Blink.
        final int requestGeneration = ++mFetchGeneration;
        final GURL requestUrl = tab.getUrl();
        final long fetchStartTimeMs = SystemClock.elapsedRealtime();
        LinkToTextHelper.requestSelectorForViewportCenter(
                tab.getWebContents(),
                (selector) -> {
                    // Drop responses invalidated by a scroll, tab switch, or navigation.
                    if (requestGeneration != mFetchGeneration) return;
                    Tab currentTab = mActivityTabProvider.get();
                    if (currentTab != tab || !Objects.equals(currentTab.getUrl(), requestUrl)) {
                        return;
                    }
                    RecordHistogram.recordTimesHistogram(
                            "Android.Handoff.ScrollPosition.FetchDuration",
                            SystemClock.elapsedRealtime() - fetchStartTimeMs);
                    mHasPendingScrollFetch = false;
                    mActiveTabTextFragment = (selector != null) ? selector : "";
                });
    }

    @Override
    public void destroy() {
        cancelAndClearTextFragment();
        updateGestureListener(null);
        mActivityTabTabObserver.destroy();
        mTabModelSelector.getCurrentTabSupplier().removeObserver(mCurrentTabObserver);
    }

    ActivityTabTabObserver getActiveTabObserverForTesting() {
        return mActivityTabTabObserver;
    }

    GestureStateListener getGestureStateListenerForTesting() {
        return mGestureStateListener;
    }

    /**
     * Updates the handoff enablement state for the activity. Handoff is disabled if the user is in
     * Incognito mode, if there is no active tab (e.g. in the tab switcher), or if the current URL
     * is an internal/unsafe scheme.
     */
    private void updateHandoffState(@HandoffEnableTrigger int updateType) {
        if (mActivityTabProvider == null) return;

        Tab tab = mActivityTabProvider.get();
        boolean handoffEnabled = isHandoffEnabled(tab);
        boolean wasHandoffEnabled = mDelegate.isHandoffEnabled(mActivity);

        if (handoffEnabled && !wasHandoffEnabled) {
            String histogramName =
                    switch (updateType) {
                        case HandoffEnableTrigger.TAB_SWITCH -> "Android.Handoff.Enabled.TabSwitch";
                        case HandoffEnableTrigger.URL_NAVIGATION ->
                                "Android.Handoff.Enabled.UrlNavigation";
                        default -> null;
                    };
            if (histogramName != null) {
                RecordHistogram.recordBooleanHistogram(histogramName, true);
            }
        } else if (!handoffEnabled && wasHandoffEnabled) {
            RecordUserAction.record("HandoffDisabled");
        }

        // 3. Resets the handoff state to allow OS to refresh and resurface the handoff icon.
        if (handoffEnabled && wasHandoffEnabled) {
            mDelegate.setHandoffEnabled(mActivity, false);
        }

        // 4. Update handoff state via delegate.
        mDelegate.setHandoffEnabled(mActivity, handoffEnabled);
    }

    @Contract("null -> false")
    private boolean isHandoffEnabled(@Nullable Tab tab) {
        if (tab == null || tab.isIncognitoBranded() || isDisallowedByPolicy()) {
            return false;
        }
        return !ExternalIntentUrlChecker.isUnsafeExternalIntentUrl(
                tab.getUrl(), /* allowLocalFiles= */ false);
    }

    private boolean isDisallowedByPolicy() {
        UserManager userManager = (UserManager) mActivity.getSystemService(Context.USER_SERVICE);
        if (userManager != null) {
            Bundle restrictions = userManager.getUserRestrictions();
            // TODO(crbug.com/444503472): Change "disallow_handoff" to UserManager#DISALLOW_HANDOFF,
            // once it is integrated into the Chrome build.
            return restrictions.getBoolean("disallow_handoff", false);
        }
        return false;
    }

    public @Nullable HandoffActivityData onHandoffActivityDataRequested(
            HandoffActivityDataRequestInfo requestInfo) {
        // 1. Get the active tab.
        Tab tab = mActivityTabProvider.get();
        if (!isHandoffEnabled(tab)) {
            return null;
        }

        RecordUserAction.record("HandoffDataRequested");

        @HandoffScrollAttachmentStatus int attachmentStatus;
        if (!mActiveTabTextFragment.isEmpty()) {
            attachmentStatus = HandoffScrollAttachmentStatus.TEXT_ATTACHED;
        } else if (mHasPendingScrollFetch) {
            attachmentStatus = HandoffScrollAttachmentStatus.MISSED_DUE_TO_DEBOUNCE;
        } else if (getScrollOffsetY(tab) > 0) {
            attachmentStatus = HandoffScrollAttachmentStatus.NO_TEXT;
        } else {
            attachmentStatus = HandoffScrollAttachmentStatus.NO_SCROLL;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Handoff.ScrollPosition.AttachmentStatus",
                attachmentStatus,
                HandoffScrollAttachmentStatus.NUM_ENTRIES);

        // 2. Build the handoff data via delegate.
        String url = appendTextFragment(tab.getUrl().getSpec(), mActiveTabTextFragment);
        return (HandoffActivityData) mDelegate.buildHandoffActivityData(mActivity, url);
    }

    private String appendTextFragment(String url, String selector) {
        Uri uri = Uri.parse(url);
        String existingFragment = uri.getEncodedFragment();
        if (existingFragment == null) existingFragment = "";

        // 1. Keep any fragment text before ":~:text=" (e.g. "intro" in "#intro:~:text=old").
        int directiveIndex = existingFragment.indexOf(LinkToTextHelper.TEXT_FRAGMENT_PREFIX);
        String baseFragment =
                directiveIndex >= 0
                        ? existingFragment.substring(0, directiveIndex)
                        : existingFragment;

        // 2. When selector is empty, remove any existing ":~:text=" from the URL.
        if (selector == null || selector.isEmpty()) {
            if (directiveIndex < 0) {
                return url;
            }
            return uri.buildUpon()
                    .encodedFragment(baseFragment.isEmpty() ? null : baseFragment)
                    .toString();
        }

        // 3. Otherwise, append ":~:text=<selector>" to baseFragment.
        return uri.buildUpon()
                .encodedFragment(baseFragment + LinkToTextHelper.TEXT_FRAGMENT_PREFIX + selector)
                .toString();
    }
}
