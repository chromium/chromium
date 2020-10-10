// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import android.os.Handler;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.datareduction.DataReductionSavingsMilestonePromo;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.feature_engagement.ScreenshotMonitor;
import org.chromium.chrome.browser.feature_engagement.ScreenshotMonitorDelegate;
import org.chromium.chrome.browser.feature_engagement.ScreenshotTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.previews.Previews;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * A helper class for IPH shown on the toolbar.
 * TODO(https://crbug.com/865801): Remove feature-specific IPH from here.
 */
public class ToolbarButtonInProductHelpController
        implements ScreenshotMonitorDelegate, PauseResumeWithNativeObserver {
    private final ActivityTabTabObserver mPageLoadObserver;
    private final ChromeActivity mActivity;
    private final AppMenuPropertiesDelegate mAppMenuPropertiesDelegate;
    private final ScreenshotMonitor mScreenshotMonitor;
    private AppMenuHandler mAppMenuHandler;
    private UserEducationHelper mUserEducationHelper;
    private final Handler mHandler = new Handler();

    public ToolbarButtonInProductHelpController(final ChromeActivity activity,
            AppMenuCoordinator appMenuCoordinator, ActivityLifecycleDispatcher lifecycleDispatcher,
            ActivityTabProvider tabProvider) {
        mActivity = activity;
        mUserEducationHelper =
                new UserEducationHelper(mActivity, mHandler, TrackerFactory::getTrackerForProfile);
        mScreenshotMonitor = new ScreenshotMonitor(this);
        lifecycleDispatcher.register(this);
        mPageLoadObserver = new ActivityTabTabObserver(tabProvider) {
            /**
             * Stores total data saved at the start of a page load. Used to calculate delta at the
             * end of page load, which is just an estimate of the data saved for the current page
             * load since there may be multiple pages loading at the same time. This estimate is
             * used to get an idea of how widely used the data saver feature is for a particular
             * user at a time (i.e. not since the user started using Chrome).
             */
            private long mDataSavedOnStartPageLoad;

            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                mDataSavedOnStartPageLoad = DataReductionProxySettings.getInstance()
                                                    .getContentLengthSavedInHistorySummary();
            }

            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                if (tab.isShowingErrorPage()) {
                    handleIPHForErrorPageShown(tab);
                    return;
                }

                handleIPHForSuccessfulPageLoad(tab);
            }

            private void handleIPHForSuccessfulPageLoad(final Tab tab) {
                long dataSaved = DataReductionProxySettings.getInstance()
                                         .getContentLengthSavedInHistorySummary()
                        - mDataSavedOnStartPageLoad;
                Tracker tracker = TrackerFactory.getTrackerForProfile(
                        Profile.fromWebContents(tab.getWebContents()));
                if (dataSaved > 0L) tracker.notifyEvent(EventConstants.DATA_SAVED_ON_PAGE_LOAD);
                if (Previews.isPreview(tab)) {
                    tracker.notifyEvent(EventConstants.PREVIEWS_PAGE_LOADED);
                }

                if (tab.isUserInteractable()) {
                    showDataSaverDetail();
                    if (dataSaved > 0L) showDataSaverMilestonePromo();
                    if (Previews.isPreview(tab)) showPreviewVerboseStatus();
                }

                showDownloadPageTextBubble(tab, FeatureConstants.DOWNLOAD_PAGE_FEATURE);
                showTranslateMenuButtonTextBubble(tab);
            }

            private void handleIPHForErrorPageShown(Tab tab) {
                if (!(mActivity instanceof ChromeTabbedActivity) || mActivity.isTablet()) {
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
        };

        mAppMenuHandler = appMenuCoordinator.getAppMenuHandler();
        mAppMenuPropertiesDelegate = appMenuCoordinator.getAppMenuPropertiesDelegate();
    }

    public void destroy() {
        if (mPageLoadObserver != null) {
            mPageLoadObserver.destroy();
        }
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
                        .setAnchorView(mActivity.getToolbarManager().getMenuButtonView())
                        .setOnShowCallback(
                                () -> turnOnHighlightForMenuItem(R.id.downloads_menu_id, true))
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
        if (mActivity.isTablet()) return;
        mScreenshotMonitor.startMonitoring();
    }

    @Override
    public void onPauseWithNative() {
        mScreenshotMonitor.stopMonitoring();
    }

    @Override
    public void onScreenshotTaken() {
        // TODO (https://crbug.com/1048632): Use the current profile (i.e., regular profile or
        // incognito profile) instead of always using regular profile. It works correctly now, but
        // it is not safe.
        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile());
        tracker.notifyEvent(EventConstants.SCREENSHOT_TAKEN_CHROME_IN_FOREGROUND);

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            showDownloadPageTextBubble(
                    mActivity.getActivityTab(), FeatureConstants.DOWNLOAD_PAGE_SCREENSHOT_FEATURE);
            ScreenshotTabObserver tabObserver =
                    ScreenshotTabObserver.from(mActivity.getActivityTab());
            if (tabObserver != null) tabObserver.onScreenshotTaken();
        });
    }

    // Private methods.
    private static int getDataReductionMenuItemHighlight() {
        return R.id.app_menu_footer;
    }

    // Attempts to show an IPH text bubble for data saver detail.
    private void showDataSaverDetail() {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(),
                        FeatureConstants.DATA_SAVER_DETAIL_FEATURE,
                        R.string.iph_data_saver_detail_text,
                        R.string.iph_data_saver_detail_accessibility_text)
                        .setAnchorView(mActivity.getToolbarManager().getMenuButtonView())
                        .setOnShowCallback(
                                ()
                                        -> turnOnHighlightForMenuItem(
                                                getDataReductionMenuItemHighlight(), false))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    // Attempts to show an IPH text bubble for data saver milestone promo.
    private void showDataSaverMilestonePromo() {
        final DataReductionSavingsMilestonePromo promo =
                new DataReductionSavingsMilestonePromo(mActivity,
                        DataReductionProxySettings.getInstance().getTotalHttpContentLengthSaved());
        if (!promo.shouldShowPromo()) return;

        final Runnable dismissCallback = () -> {
            promo.onPromoTextSeen();
            turnOffHighlightForMenuItem();
        };

        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(),
                        FeatureConstants.DATA_SAVER_MILESTONE_PROMO_FEATURE, promo.getPromoText(),
                        promo.getPromoText())
                        .setAnchorView(mActivity.getToolbarManager().getMenuButtonView())
                        .setOnShowCallback(
                                ()
                                        -> turnOnHighlightForMenuItem(
                                                getDataReductionMenuItemHighlight(), false))
                        .setOnDismissCallback(dismissCallback)
                        .build());
    }

    // Attempts to show an IPH text bubble for page in preview mode.
    private void showPreviewVerboseStatus() {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(),
                        FeatureConstants.PREVIEWS_OMNIBOX_UI_FEATURE,
                        R.string.iph_previews_omnibox_ui_text,
                        R.string.iph_previews_omnibox_ui_accessibility_text)
                        .setAnchorView(mActivity.getToolbarManager().getSecurityIconView())
                        .setShouldHighlight(false)
                        .build());
    }

    private void showDownloadHomeIPH() {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(),
                        FeatureConstants.DOWNLOAD_HOME_FEATURE, R.string.iph_download_home_text,
                        R.string.iph_download_home_accessibility_text)
                        .setAnchorView(mActivity.getToolbarManager().getMenuButtonView())
                        .setOnShowCallback(
                                () -> turnOnHighlightForMenuItem(R.id.downloads_menu_id, true))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    /**
     * Show the download page in-product-help bubble. Also used by download page screenshot IPH.
     * @param tab The current tab.
     */
    private void showDownloadPageTextBubble(final Tab tab, String featureName) {
        if (tab == null) return;
        if (!(mActivity instanceof ChromeTabbedActivity) || mActivity.isTablet()
                || mActivity.isInOverviewMode() || !DownloadUtils.isAllowedToDownloadPage(tab)) {
            return;
        }

        final Integer offlinePageId =
                CachedFeatureFlags.isEnabled(
                        ChromeFeatureList.TABBED_APP_OVERFLOW_MENU_THREE_BUTTON_ACTIONBAR)
                ? R.id.offline_page_chip_id
                : R.id.offline_page_id;

        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mActivity.getResources(), featureName,
                        R.string.iph_download_page_for_offline_usage_text,
                        R.string.iph_download_page_for_offline_usage_accessibility_text)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(offlinePageId, true))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .setAnchorView(mActivity.getToolbarManager().getMenuButtonView())
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
                        .setOnShowCallback(
                                () -> turnOnHighlightForMenuItem(R.id.translate_id, false))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .setAnchorView(mActivity.getToolbarManager().getMenuButtonView())
                        .build());
    }

    private void turnOnHighlightForMenuItem(Integer highlightMenuItemId, boolean circleHighlight) {
        if (mAppMenuHandler != null) {
            mAppMenuHandler.setMenuHighlight(highlightMenuItemId, circleHighlight);
        }
    }

    private void turnOffHighlightForMenuItem() {
        if (mAppMenuHandler != null) {
            mAppMenuHandler.clearMenuHighlight();
        }
    }
}
