// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.app.Activity;
import android.os.Handler;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.ScreenshotMonitor;
import org.chromium.chrome.browser.feature_engagement.ScreenshotMonitorDelegate;
import org.chromium.chrome.browser.feature_engagement.ScreenshotTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feature_guide.notifications.FeatureNotificationUtils;
import org.chromium.chrome.browser.feature_guide.notifications.FeatureType;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * A helper class for IPH shown on the toolbar.
 * TODO(https://crbug.com/865801): Remove feature-specific IPH from here.
 */
public class ToolbarButtonInProductHelpController
        implements ScreenshotMonitorDelegate, PauseResumeWithNativeObserver {
    private final CurrentTabObserver mPageLoadObserver;
    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final ScreenshotMonitor mScreenshotMonitor;
    private final Handler mHandler = new Handler();
    private final View mSecurityIconAnchorView;
    private final UserEducationHelper mUserEducationHelper;
    private final Supplier<Tab> mCurrentTabSupplier;
    private final Supplier<Boolean> mIsInOverviewModeSupplier;

    /**
     * @param activity {@link Activity} on which this class runs.
     * @param windowAndroid {@link WindowAndroid} for the current Activity.
     * @param lifecycleDispatcher {@link LifecycleDispatcher} that helps observe activity lifecycle.
     * @param tabSupplier An observable supplier of the current {@link Tab}.
     * @param isInOverviewModeSupplier Supplies whether the app is in overview mode.
     * @param securityIconAnchorView The security icon to serve as an anchor.
     */
    public ToolbarButtonInProductHelpController(@NonNull Activity activity,
            @NonNull WindowAndroid windowAndroid,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull ObservableSupplier<Tab> tabSupplier,
            @NonNull Supplier<Boolean> isInOverviewModeSupplier,
            @NonNull View securityIconAnchorView) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mSecurityIconAnchorView = securityIconAnchorView;
        mIsInOverviewModeSupplier = isInOverviewModeSupplier;
        mUserEducationHelper = new UserEducationHelper(mActivity, mHandler);
        mScreenshotMonitor = new ScreenshotMonitor(this);
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mCurrentTabSupplier = tabSupplier;
        mPageLoadObserver = new CurrentTabObserver(tabSupplier, new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                // Part of scroll jank investigation http://crbug.com/1311003. Will remove
                // TraceEvent after the investigation is complete.
                try (TraceEvent te = TraceEvent.scoped(
                             "ToolbarButtonInProductHelpController::onPageLoadFinished")) {
                    if (tab.isShowingErrorPage()) {
                        handleIPHForErrorPageShown(tab);
                        return;
                    }

                    handleIPHForSuccessfulPageLoad(tab);
                }
            }

            private void handleIPHForSuccessfulPageLoad(final Tab tab) {
                showDownloadPageTextBubble(tab, FeatureConstants.DOWNLOAD_PAGE_FEATURE);
                showTranslateMenuButtonTextBubble(tab);
                showPriceTrackingIPH(tab);
            }

            private void handleIPHForErrorPageShown(Tab tab) {
                if (DeviceFormFactor.isWindowOnTablet(mWindowAndroid)) {
                    return;
                }

                OfflinePageBridge bridge = OfflinePageBridge.getForProfile(
                        Profile.fromWebContents(tab.getWebContents()));
                if (bridge == null
                        || !bridge.isShowingDownloadButtonInErrorPage(tab.getWebContents())) {
                    return;
                }

                Tracker tracker = TrackerFactory.getTrackerForProfile(
                        Profile.fromWebContents(tab.getWebContents()));
                tracker.notifyEvent(EventConstants.USER_HAS_SEEN_DINO);
            }
        }, /*swapCallback=*/null);

        FeatureNotificationUtils.registerIPHCallback(
                FeatureType.INCOGNITO_TAB, this::showIncognitoTabIPH);
    }

    public void destroy() {
        FeatureNotificationUtils.unregisterIPHCallback(FeatureType.INCOGNITO_TAB);
        mPageLoadObserver.destroy();
        mLifecycleDispatcher.unregister(this);
    }

    /**
     * Attempt to show the IPH for price tracking.
     * @param tab The tab currently being displayed to the user.
     */
    private void showPriceTrackingIPH(Tab tab) {
    }

    /**
     * Attempts to show an IPH text bubble for download continuing.
     */
    public void showDownloadContinuingIPH() {
    }

    /**
     * Attempts to show an IPH text bubble for those that trigger on a cold start.
     */
    public void showColdStartIPH() {
        showDownloadHomeIPH();
    }

    // Overridden public methods.
    @Override
    public void onResumeWithNative() {
        // Part of the (more runtime-related) check to determine whether to trigger help UI is
        // left until onScreenshotTaken() since it is less expensive to keep monitoring on and
        // check when the help UI is accessed than it is to start/stop monitoring per tab change
        // (e.g. tab switch or in overview mode).
        if (DeviceFormFactor.isWindowOnTablet(mWindowAndroid)) return;
        mScreenshotMonitor.startMonitoring();
    }

    @Override
    public void onPauseWithNative() {
        mScreenshotMonitor.stopMonitoring();
    }

    @Override
    public void onScreenshotTaken() {
        boolean isIncognito =
                mCurrentTabSupplier.get() != null && mCurrentTabSupplier.get().isIncognito();
        Profile profile = IncognitoUtils.getProfileFromWindowAndroid(mWindowAndroid, isIncognito);
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.SCREENSHOT_TAKEN_CHROME_IN_FOREGROUND);

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            showDownloadPageTextBubble(
                    mCurrentTabSupplier.get(), FeatureConstants.DOWNLOAD_PAGE_SCREENSHOT_FEATURE);
            ScreenshotTabObserver tabObserver =
                    ScreenshotTabObserver.from(mCurrentTabSupplier.get());
            if (tabObserver != null) tabObserver.onScreenshotTaken();
        });
    }

    // Private methods.
    private static int getDataReductionMenuItemHighlight() {
        return R.id.app_menu_footer;
    }

    private void showDownloadHomeIPH() {
    }

    private void showIncognitoTabIPH() {
    }

    /**
     * Show the download page in-product-help bubble. Also used by download page screenshot IPH.
     * @param tab The current tab.
     */
    private void showDownloadPageTextBubble(final Tab tab, String featureName) {
    }

    /**
     * Show the translate manual trigger in-product-help bubble.
     * @param tab The current tab.
     */
    private void showTranslateMenuButtonTextBubble(final Tab tab) {
    }

    private void turnOnHighlightForMenuItem(Integer highlightMenuItemId) {
    }

    private void turnOffHighlightForMenuItem() {
    }
}
