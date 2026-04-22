// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.app.Activity;
import android.os.Handler;
import android.view.View;

import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/**
 * A helper class for IPH shown on the toolbar. TODO(crbug.com/40585866): Remove feature-specific
 * IPH from here.
 */
@NullMarked
public class ToolbarButtonInProductHelpController {
    public static final int PAGE_HISTORY_MIN_OFFSET = -2;

    private final CurrentTabObserver mPageLoadObserver;
    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final View mMenuButtonAnchorView;
    private final AppMenuHandler mAppMenuHandler;
    private final UserEducationHelper mUserEducationHelper;
    private final Supplier<Boolean> mIsInOverviewModeSupplier;

    /**
     * @param activity {@link Activity} on which this class runs.
     * @param windowAndroid {@link WindowAndroid} for the current Activity.
     * @param appMenuCoordinator {@link AppMenuCoordinator} whose visual state is to be updated
     *     accordingly.
     * @param profile The current Profile.
     * @param tabSupplier An observable supplier of the current {@link Tab}.
     * @param isInOverviewModeSupplier Supplies whether the app is in overview mode.
     * @param menuButtonAnchorView The menu button view to serve as an anchor.
     */
    public ToolbarButtonInProductHelpController(
            Activity activity,
            WindowAndroid windowAndroid,
            AppMenuCoordinator appMenuCoordinator,
            Profile profile,
            NullableObservableSupplier<Tab> tabSupplier,
            Supplier<Boolean> isInOverviewModeSupplier,
            View menuButtonAnchorView) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mAppMenuHandler = appMenuCoordinator.getAppMenuHandler();
        mMenuButtonAnchorView = menuButtonAnchorView;
        mIsInOverviewModeSupplier = isInOverviewModeSupplier;
        mUserEducationHelper = new UserEducationHelper(mActivity, profile, new Handler());
        mPageLoadObserver =
                new CurrentTabObserver(
                        tabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onPageLoadFinished(Tab tab, GURL url) {
                                // Part of scroll jank investigation http://crbug.com/40830793. Will
                                // remove TraceEvent after the investigation is complete.
                                try (TraceEvent te =
                                        TraceEvent.scoped(
                                                "ToolbarButtonInProductHelpController::onPageLoadFinished")) {
                                    if (tab.isShowingErrorPage()) {
                                        handleIphForErrorPageShown(tab);
                                        return;
                                    }

                                    handleIphForSuccessfulPageLoad(tab);
                                }
                            }

                            private void handleIphForSuccessfulPageLoad(final Tab tab) {
                                showDownloadPageTextBubble(
                                        tab, FeatureConstants.DOWNLOAD_PAGE_FEATURE);
                                showTranslateMenuButtonTextBubble(tab);
                                showPriceTrackingIph(tab);
                                maybeShowNewTabPageThemeCustomizationIph(tab);
                                if (appMenuCoordinator
                                        .getAppMenuPropertiesDelegate()
                                        .shouldShowIconRow()) {
                                    maybeShowBackButtonIph(tab);
                                }
                            }

                            private void handleIphForErrorPageShown(Tab tab) {
                                if (DeviceFormFactor.isWindowOnTablet(mWindowAndroid)) {
                                    return;
                                }

                                OfflinePageBridge bridge =
                                        OfflinePageBridge.getForProfile(tab.getProfile());
                                if (bridge == null
                                        || !bridge.isShowingDownloadButtonInErrorPage(
                                                tab.getWebContents())) {
                                    return;
                                }

                                Profile profile = Profile.fromWebContents(tab.getWebContents());
                                assert profile != null;
                                Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
                                tracker.notifyEvent(EventConstants.USER_HAS_SEEN_DINO);
                            }
                        },
                        /* swapCallback= */ null);
    }

    public void destroy() {
        mPageLoadObserver.destroy();
    }

    /**
     * Attempt to show the IPH for price tracking.
     *
     * @param tab The tab currently being displayed to the user.
     */
    private void showPriceTrackingIph(Tab tab) {
        if (tab == null || tab.getWebContents() == null) return;

        if (!CommerceFeatureUtils.isShoppingListEligible(
                        ShoppingServiceFactory.getForProfile(tab.getProfile()))
                || !PowerBookmarkUtils.isPriceTrackingEligible(tab)) {
            return;
        }

        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.SHOPPING_LIST_MENU_ITEM_FEATURE,
                                R.string.iph_price_tracking_menu_item,
                                R.string.iph_price_tracking_menu_item_accessibility)
                        .setAnchorView(mMenuButtonAnchorView)
                        .setOnShowCallback(
                                () ->
                                        turnOnHighlightForMenuItem(
                                                R.id.enable_price_tracking_menu_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    /** Attempts to show an IPH text bubble for download continuing. */
    public void showDownloadContinuingIph() {
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.DOWNLOAD_INFOBAR_DOWNLOAD_CONTINUING_FEATURE,
                                R.string.iph_download_infobar_download_continuing_text,
                                R.string.iph_download_infobar_download_continuing_text)
                        .setAnchorView(mMenuButtonAnchorView)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(R.id.downloads_menu_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    /** Attempts to show an IPH for New Tab Page theme customization. */
    private void maybeShowNewTabPageThemeCustomizationIph(Tab tab) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)) {
            return;
        }
        if (tab.isIncognitoBranded() || !UrlUtilities.isNtpUrl(tab.getUrl())) return;

        showNewTabPageThemeCustomizationIph();
    }

    private void showNewTabPageThemeCustomizationIph() {
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.NEW_TAB_PAGE_THEME_CUSTOMIZATION_FEATURE,
                                R.string.new_tab_page_theme_customization_iph,
                                R.string.new_tab_page_theme_customization_iph)
                        .setAnchorView(mMenuButtonAnchorView)
                        .setOnShowCallback(
                                () -> turnOnHighlightForMenuItem(R.id.ntp_customization_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    /** Attempts to show an IPH text bubble for those that trigger on a cold start. */
    public void showColdStartIph() {
        showAddToGroupIph();
        showDownloadHomeIph();
    }

    private void showDownloadHomeIph() {
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.DOWNLOAD_HOME_FEATURE,
                                R.string.iph_download_home_text,
                                R.string.iph_download_home_accessibility_text)
                        .setAnchorView(mMenuButtonAnchorView)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(R.id.downloads_menu_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    private void showAddToGroupIph() {
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.MENU_ADD_TO_GROUP,
                                R.string.tab_switcher_add_to_group_iph,
                                R.string.tab_switcher_add_to_group_iph)
                        .setAnchorView(mMenuButtonAnchorView)
                        .setOnShowCallback(
                                () -> turnOnHighlightForMenuItem(R.id.add_to_group_menu_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .build());
    }

    /**
     * Show the download page in-product-help bubble. Also used by download page screenshot IPH.
     *
     * @param tab The current tab.
     */
    private void showDownloadPageTextBubble(final Tab tab, String featureName) {
        if (tab == null) return;
        if (DeviceFormFactor.isWindowOnTablet(mWindowAndroid)
                || (mIsInOverviewModeSupplier.get() != null && mIsInOverviewModeSupplier.get())
                || !DownloadUtils.isAllowedToDownloadPage(tab)) {
            return;
        }

        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mActivity.getResources(),
                                featureName,
                                R.string.iph_download_page_for_offline_usage_text,
                                R.string.iph_download_page_for_offline_usage_accessibility_text)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(R.id.offline_page_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .setAnchorView(mMenuButtonAnchorView)
                        .build());
    }

    /**
     * Show the translate manual trigger in-product-help bubble.
     * @param tab The current tab.
     */
    private void showTranslateMenuButtonTextBubble(final Tab tab) {
        if (tab == null) return;
        if (!TranslateUtils.canTranslateCurrentTab(tab)
                || !TranslateBridge.shouldShowManualTranslateIph(tab)) {
            return;
        }

        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mActivity.getResources(),
                                FeatureConstants.TRANSLATE_MENU_BUTTON_FEATURE,
                                R.string.iph_translate_menu_button_text,
                                R.string.iph_translate_menu_button_accessibility_text)
                        .setOnShowCallback(() -> turnOnHighlightForMenuItem(R.id.translate_id))
                        .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                        .setAnchorView(mMenuButtonAnchorView)
                        .build());
    }

    private void maybeShowBackButtonIph(Tab tab) {
        if (!ChromeFeatureList.sThreeDotMenuBackButton.isEnabled()) {
            return;
        }

        // Ensure that the tab history has at least two web pages to navigate back to.
        boolean validPageHistory =
                tab.getWebContents() != null
                        && tab.getWebContents()
                                .getNavigationController()
                                .canGoToOffset(PAGE_HISTORY_MIN_OFFSET);
        if (validPageHistory) {
            mUserEducationHelper.requestShowIph(
                    new IphCommandBuilder(
                                    mActivity.getResources(),
                                    FeatureConstants.THREE_DOT_MENU_BACK_BUTTON,
                                    R.string.menu_back_button_iph_text,
                                    R.string.menu_back_button_iph_text)
                            .setOnShowCallback(() -> turnOnHighlightForMenuItem(R.id.back_menu_id))
                            .setOnDismissCallback(this::turnOffHighlightForMenuItem)
                            .setAnchorView(mMenuButtonAnchorView)
                            .setShowTextBubble(true)
                            .setDismissOnTouch(true)
                            .build());
        }
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
