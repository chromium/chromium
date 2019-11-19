// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.datareduction.DataReductionSavingsMilestonePromo;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.widget.highlight.ViewHighlighter;
import org.chromium.chrome.browser.ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * A helper class for IPH shown on the toolbar.
 */
public class ToolbarButtonInProductHelpController {
    private final ActivityTabTabObserver mPageLoadObserver;
    private final ChromeActivity mActivity;

    private AppMenuHandler mAppMenuHandler;

    public ToolbarButtonInProductHelpController(
            final ChromeActivity activity, AppMenuCoordinator appMenuCoordinator) {
        mActivity = activity;
        mPageLoadObserver = new ActivityTabTabObserver(activity.getActivityTabProvider()) {
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
                long dataSaved = DataReductionProxySettings.getInstance()
                                         .getContentLengthSavedInHistorySummary()
                        - mDataSavedOnStartPageLoad;
                Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
                if (dataSaved > 0L) tracker.notifyEvent(EventConstants.DATA_SAVED_ON_PAGE_LOAD);
                if (tab.isPreview()) tracker.notifyEvent(EventConstants.PREVIEWS_PAGE_LOADED);
                if (tab.isUserInteractable()) {
                    maybeShowDataSaverDetail();
                    if (dataSaved > 0L) maybeShowDataSaverMilestonePromo();
                    if (tab.isPreview()) maybeShowPreviewVerboseStatus();
                }
            }
        };

        mAppMenuHandler = appMenuCoordinator.getAppMenuHandler();
    }

    public void destroy() {
        if (mPageLoadObserver != null) {
            mPageLoadObserver.destroy();
        }
    }

    private static int getDataReductionMenuItemHighlight() {
        return FeatureUtilities.isBottomToolbarEnabled() ? R.id.data_reduction_menu_item
                                                         : R.id.app_menu_footer;
    }

    // Attempts to show an IPH text bubble for data saver detail.
    private void maybeShowDataSaverDetail() {
        View anchorView = mActivity.getToolbarManager().getMenuButtonView();
        if (anchorView == null) return;

        setupAndMaybeShowIPHForFeature(FeatureConstants.DATA_SAVER_DETAIL_FEATURE,
                getDataReductionMenuItemHighlight(), false, R.string.iph_data_saver_detail_text,
                R.string.iph_data_saver_detail_accessibility_text, anchorView,
                Profile.getLastUsedProfile(), null);
    }

    // Attempts to show an IPH text bubble for data saver milestone promo.
    private void maybeShowDataSaverMilestonePromo() {
        View anchorView = mActivity.getToolbarManager().getMenuButtonView();
        if (anchorView == null) return;

        final DataReductionSavingsMilestonePromo promo =
                new DataReductionSavingsMilestonePromo(mActivity,
                        DataReductionProxySettings.getInstance().getTotalHttpContentLengthSaved());
        if (!promo.shouldShowPromo()) return;

        final Runnable dismissCallback = () -> {
            promo.onPromoTextSeen();
        };
        setupAndMaybeShowIPHForFeature(FeatureConstants.DATA_SAVER_MILESTONE_PROMO_FEATURE,
                getDataReductionMenuItemHighlight(), false, promo.getPromoText(),
                promo.getPromoText(), anchorView, Profile.getLastUsedProfile(), dismissCallback);
    }

    // Attempts to show an IPH text bubble for page in preview mode.
    private void maybeShowPreviewVerboseStatus() {
        final View anchorView = mActivity.getToolbarManager().getSecurityIconView();
        if (anchorView == null) return;

        setupAndMaybeShowIPHForFeature(FeatureConstants.PREVIEWS_OMNIBOX_UI_FEATURE, null, true,
                R.string.iph_previews_omnibox_ui_text,
                R.string.iph_previews_omnibox_ui_accessibility_text, anchorView,
                Profile.getLastUsedProfile(), null);
    }

    /**
     * Attempts to show an IPH text bubble for those that trigger on a cold start.
     */
    public void maybeShowColdStartIPH() {
        maybeShowDownloadHomeIPH();
    }

    private void maybeShowDownloadHomeIPH() {
        setupAndMaybeShowIPHForFeature(FeatureConstants.DOWNLOAD_HOME_FEATURE,
                R.id.downloads_menu_id, true, R.string.iph_download_home_text,
                R.string.iph_download_home_accessibility_text,
                mActivity.getToolbarManager().getMenuButtonView(), Profile.getLastUsedProfile(),
                null);
    }

    /**
     * Attempts to show an IPH text bubble for download continuing.
     * @param window The window to use for the IPH.
     * @param profile The profile to use for the tracker.
     */
    public void maybeShowDownloadContinuingIPH(Profile profile) {
        setupAndMaybeShowIPHForFeature(
                FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOAD_CONTINUING_FEATURE,
                R.id.downloads_menu_id, true,
                R.string.iph_download_infobar_download_continuing_text,
                R.string.iph_download_infobar_download_continuing_text,
                mActivity.getToolbarManager().getMenuButtonView(), profile, null);
    }

    private void setupAndMaybeShowIPHForFeature(String featureName, Integer highlightMenuItemId,
            boolean circleHighlight, @StringRes int stringId, @StringRes int accessibilityStringId,
            View anchorView, Profile profile, @Nullable Runnable onDismissCallback) {
        final String contentString = mActivity.getString(stringId);
        final String accessibilityString = mActivity.getString(accessibilityStringId);
        final Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.addOnInitializedCallback((Callback<Boolean>) success
                -> maybeShowIPH(tracker, featureName, highlightMenuItemId, circleHighlight,
                        contentString, accessibilityString, anchorView, onDismissCallback));
    }

    private void setupAndMaybeShowIPHForFeature(String featureName, Integer highlightMenuItemId,
            boolean circleHighlight, String contentString, String accessibilityString,
            View anchorView, Profile profile, @Nullable Runnable onDismissCallback) {
        final Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.addOnInitializedCallback((Callback<Boolean>) success
                -> maybeShowIPH(tracker, featureName, highlightMenuItemId, circleHighlight,
                        contentString, accessibilityString, anchorView, onDismissCallback));
    }

    private static boolean shouldHighlightForIPH(String featureName) {
        switch (featureName) {
            case FeatureConstants.PREVIEWS_OMNIBOX_UI_FEATURE:
                return false;
            default:
                return true;
        }
    }

    private void maybeShowIPH(Tracker tracker, String featureName, Integer highlightMenuItemId,
            boolean circleHighlight, String contentString, String accessibilityString,
            View anchorView, @Nullable Runnable onDismissCallback) {
        // Activity was destroyed; don't show IPH.
        if (mActivity.isActivityFinishingOrDestroyed() || anchorView == null) return;

        assert (contentString.length() > 0);
        assert (accessibilityString.length() > 0);

        // Post a request to show the IPH bubble to allow time for a layout pass. Since the bubble
        // is shown on startup, the anchor view may not have a height initially see
        // https://crbug.com/871537.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (mActivity.isActivityFinishingOrDestroyed()) return;

            if (!tracker.shouldTriggerHelpUI(featureName)) return;
            ViewRectProvider rectProvider = new ViewRectProvider(anchorView);

            TextBubble textBubble = new TextBubble(
                    mActivity, anchorView, contentString, accessibilityString, true, rectProvider);
            textBubble.setDismissOnTouchInteraction(true);
            textBubble.addOnDismissListener(() -> anchorView.getHandler().postDelayed(() -> {
                tracker.dismissed(featureName);
                if (onDismissCallback != null) {
                    onDismissCallback.run();
                }
                if (shouldHighlightForIPH(featureName)) {
                    turnOffHighlightForTextBubble(anchorView);
                }
            }, ViewHighlighter.IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS));

            if (shouldHighlightForIPH(featureName)) {
                turnOnHighlightForTextBubble(highlightMenuItemId, circleHighlight, anchorView);
            }

            int yInsetPx = mActivity.getResources().getDimensionPixelOffset(
                    R.dimen.text_bubble_menu_anchor_y_inset);
            rectProvider.setInsetPx(0, 0, 0, yInsetPx);
            textBubble.show();
        });
    }

    private void turnOnHighlightForTextBubble(
            Integer highlightMenuItemId, boolean circleHighlight, View anchorView) {
        if (mAppMenuHandler != null) {
            mAppMenuHandler.setMenuHighlight(highlightMenuItemId, circleHighlight);
        } else {
            ViewHighlighter.turnOnHighlight(anchorView, circleHighlight);
        }
    }

    private void turnOffHighlightForTextBubble(View anchorView) {
        if (mAppMenuHandler != null) {
            mAppMenuHandler.clearMenuHighlight();
        } else {
            ViewHighlighter.turnOffHighlight(anchorView);
        }
    }
}
