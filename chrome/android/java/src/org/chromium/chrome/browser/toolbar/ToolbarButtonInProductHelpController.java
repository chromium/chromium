// Copyright 2018 The Chromium Authors
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
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.download.DownloadUtils;
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
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialServiceFactory;
import org.chromium.chrome.browser.video_tutorials.iph.VideoTutorialTryNowTracker;
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
    private final AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private final ScreenshotMonitor mScreenshotMonitor;
    private final Handler mHandler = new Handler();
    private final View mMenuButtonAnchorView;
    private final View mSecurityIconAnchorView;
    private final AppMenuHandler mAppMenuHandler;
    private final UserEducationHelper mUserEducationHelper;
    private final Supplier<Tab> mCurrentTabSupplier;
    private final Supplier<Boolean> mIsInOverviewModeSupplier;

    /**
     * @param activity {@link Activity} on which this class runs.
     * @param windowAndroid {@link WindowAndroid} for the current Activity.
     * @param appMenuCoordinator {@link AppMenuCoordinator} whose visual state is to be updated
     *        accordingly.
     * @param lifecycleDispatcher {@link LifecycleDispatcher} that helps observe activity lifecycle.
     * @param tabSupplier An observable supplier of the current {@link Tab}.
     * @param isInOverviewModeSupplier Supplies whether the app is in overview mode.
     * @param menuButtonAnchorView The menu button view to serve as an anchor.
     * @param securityIconAnchorView The security icon to serve as an anchor.
     */
    public ToolbarButtonInProductHelpController(@NonNull Activity activity,
            @NonNull WindowAndroid windowAndroid, @NonNull AppMenuCoordinator appMenuCoordinator,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull ObservableSupplier<Tab> tabSupplier,
            @NonNull Supplier<Boolean> isInOverviewModeSupplier, @NonNull View menuButtonAnchorView,
            @NonNull View securityIconAnchorView) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mAppMenuHandler = appMenuCoordinator.getAppMenuHandler();
        mAppMenuPropertiesDelegate = appMenuCoordinator.getAppMenuPropertiesDelegate();
        mMenuButtonAnchorView = menuButtonAnchorView;
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
        if (!ShoppingFeatures.isShoppingListEnabled()
                || !PowerBookmarkUtils.isPriceTrackingEligible(tab)
                || AdaptiveToolbarFeatures.isContextualPageActionUiEnabled()) {
            return;
        }

        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(),
                        FeatureConstants.SHOPPING_LIST_MENU_ITEM_FEATURE,
                        R.string.iph_price_tracking_menu_item,
                        R.string.iph_price_tracking_menu_item_accessibility)
                        .setAnchorView(mMenuButtonAnchorView)
                        .setOnShowCallback(()
                                                   -> turnOnHighlightForMenuItem(
                                                           R.id.enable_price_tracking_menu_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    /**
     * Attempts to show an IPH text bubble for download continuing.
     */
    public void showDownloadContinuingIPH() {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(),
                        FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOAD_CONTINUING_FEATURE,
                        R.string.iph_download_infobar_download_continuing_text,
                        R.string.iph_download_infobar_download_continuing_text)
                        .setAnchorView(mMenuButtonAnchorView)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(R.id.downloads_menu_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
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
        mHandler.post(this::showVideoTutorialTryNowUIForDownload);
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
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(),
                        FeatureConstants.DOWNLOAD_HOME_FEATURE, R.string.iph_download_home_text,
                        R.string.iph_download_home_accessibility_text)
                        .setAnchorView(mMenuButtonAnchorView)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(R.id.downloads_menu_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    private void showIncognitoTabIPH() {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(),
                        FeatureConstants
                                .FEATURE_NOTIFICATION_GUIDE_INCOGNITO_TAB_HELP_BUBBLE_FEATURE,
                        R.string.feature_notification_guide_tooltip_message_incognito_tab,
                        R.string.feature_notification_guide_tooltip_message_incognito_tab)
                        .setAnchorView(mMenuButtonAnchorView)
                        .setOnShowCallback(
                                () -> turnOnHighlightForMenuItem(R.id.new_incognito_tab_menu_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    /**
     * Show the download page in-product-help bubble. Also used by download page screenshot IPH.
     * @param tab The current tab.
     */
    private void showDownloadPageTextBubble(final Tab tab, String featureName) {
        if (tab == null) return;
        if (DeviceFormFactor.isWindowOnTablet(mWindowAndroid)
                || (mIsInOverviewModeSupplier.get() != null && mIsInOverviewModeSupplier.get())
                || !DownloadUtils.isAllowedToDownloadPage(tab)) {
            return;
        }

        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(), featureName,
                        R.string.iph_download_page_for_offline_usage_text,
                        R.string.iph_download_page_for_offline_usage_accessibility_text)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(R.id.offline_page_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .setAnchorView(mMenuButtonAnchorView)
                        .build());
        // Record metrics if we show Download IPH after a screenshot of the page.
        ScreenshotTabObserver tabObserver = ScreenshotTabObserver.from(tab);
        if (tabObserver != null) {
            tabObserver.onActionPerformedAfterScreenshot(
                    ScreenshotTabObserver.SCREENSHOT_ACTION_DOWNLOAD_IPH);
        }
    }

    /**
     * Show the translate manual trigger in-product-help bubble.
     * @param tab The current tab.
     */
    private void showTranslateMenuButtonTextBubble(final Tab tab) {
        if (tab == null) return;
        if (!TranslateUtils.canTranslateCurrentTab(tab)
                || !TranslateBridge.shouldShowManualTranslateIPH(tab)) {
            return;
        }

        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(),
                        FeatureConstants.TRANSLATE_MENU_BUTTON_FEATURE,
                        R.string.iph_translate_menu_button_text,
                        R.string.iph_translate_menu_button_accessibility_text)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(R.id.translate_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .setAnchorView(mMenuButtonAnchorView)
                        .build());
    }

    /** Show the Try Now UI for video tutorial download feature. */
    private void showVideoTutorialTryNowUIForDownload() {
        VideoTutorialTryNowTracker tryNowTracker = VideoTutorialServiceFactory.getTryNowTracker();
        if (!tryNowTracker.didClickTryNowButton(
                    org.chromium.chrome.browser.video_tutorials.FeatureType.DOWNLOAD)) {
            return;
        }

        Integer menuItemId = DownloadUtils.isAllowedToDownloadPage(mCurrentTabSupplier.get())
                ? R.id.offline_page_id
                : R.id.downloads_menu_id;

        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(), null,
                        R.string.video_tutorials_iph_tap_here_to_start,
                        R.string.video_tutorials_iph_tap_here_to_start)
                        .setAnchorView(mMenuButtonAnchorView)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(menuItemId))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
        tryNowTracker.tryNowUIShown(
                org.chromium.chrome.browser.video_tutorials.FeatureType.DOWNLOAD);
    }

    private void turnOnHighlightForMenuItem(Integer highlightMenuItemId) {
        if (mAppMenuHandler != null) {
            mAppMenuHandler.setMenuHighlight(highlightMenuItemId);
        }
    }

    private void turnOffHighlightForMenuItem() {
        if (mAppMenuHandler != null) {
            mAppMenuHandler.clearMenuHighlight();
        }
    }
}
