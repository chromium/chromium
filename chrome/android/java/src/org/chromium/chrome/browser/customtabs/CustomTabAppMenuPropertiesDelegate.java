// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.content.res.Resources;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.DefaultBrowserInfo;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.readaloud.ReadAloudFeatures;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Supplier;

/** App menu properties delegate for {@link CustomTabActivity}. */
@NullMarked
public class CustomTabAppMenuPropertiesDelegate extends AppMenuPropertiesDelegateImpl {
    private static final String CUSTOM_MENU_ITEM_ID_KEY = "CustomMenuItemId";
    private static final String SHOW_OPEN_IN_BROWSER_MENU_TOP_PARAM =
            "show_open_in_browser_menu_top";
    private static final String REMOVE_FIND_IN_PAGE_MENU_ITEM_PARAM =
            "remove_find_in_page_menu_item";
    private static final String REMOVE_DESKTOP_SITE_MENU_ITEM_PARAM =
            "remove_desktop_site_menu_item";
    private final Verifier mVerifier;
    private final @CustomTabsUiType int mUiType;
    private boolean mShowShare;
    private final boolean mShowStar;
    private final boolean mShowDownload;
    private final boolean mIsOpenedByChrome;
    private final boolean mIsIncognitoBranded;
    private final boolean mIsOffTheRecord;
    private final boolean mIsStartIconMenu;

    private final List<String> mMenuEntries;
    private final Map<Integer, Integer> mItemIdToIndexMap = new HashMap<>();
    private final Supplier<ContextualPageActionController> mContextualPageActionControllerSupplier;

    private boolean mHasClientPackage;

    /** Creates an {@link CustomTabAppMenuPropertiesDelegate} instance. */
    public CustomTabAppMenuPropertiesDelegate(
            Context context,
            ActivityTabProvider activityTabProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            TabModelSelector tabModelSelector,
            ToolbarManager toolbarManager,
            View decorView,
            ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            Verifier verifier,
            @CustomTabsUiType final int uiType,
            List<String> menuEntries,
            boolean isOpenedByChrome,
            boolean showShare,
            boolean showStar,
            boolean showDownload,
            boolean isIncognitoBranded,
            boolean isOffTheRecord,
            boolean isStartIconMenu,
            Supplier<ReadAloudController> readAloudControllerSupplier,
            Supplier<ContextualPageActionController> contextualPageActionControllerSupplier,
            boolean hasClientPackage) {
        super(
                context,
                activityTabProvider,
                multiWindowModeStateDispatcher,
                tabModelSelector,
                toolbarManager,
                decorView,
                null,
                bookmarkModelSupplier,
                readAloudControllerSupplier);
        mVerifier = verifier;
        mUiType = uiType;
        mMenuEntries = menuEntries;
        mIsOpenedByChrome = isOpenedByChrome;
        mShowShare = showShare && mUiType != CustomTabsUiType.AUTH_TAB;
        mShowStar = showStar;
        mShowDownload = showDownload;
        mIsIncognitoBranded = isIncognitoBranded;
        mIsOffTheRecord = isOffTheRecord;
        mIsStartIconMenu = isStartIconMenu;
        mContextualPageActionControllerSupplier = contextualPageActionControllerSupplier;
        mHasClientPackage = hasClientPackage;
    }

    @Override
    @VisibleForTesting
    public MVCListAdapter.ModelList buildMenuModelList() {
        MVCListAdapter.ModelList modelList = new MVCListAdapter.ModelList();

        Tab currentTab = mActivityTabProvider.get();
        if (currentTab == null) return modelList;

        GURL url = currentTab.getUrl();

        boolean iconRowVisible = true;
        boolean findInPageVisible = true;
        boolean openInChromeItemVisible = true;
        boolean bookmarkItemVisible = mShowStar;
        boolean downloadItemVisible = mShowDownload;
        boolean addToHomeScreenVisible = true;
        boolean requestDesktopSiteVisible = true;
        boolean tryAddingReadAloud = ReadAloudFeatures.isEnabledForOverflowMenuInCct();
        boolean readerModePrefsVisible = false;
        boolean translateVisible = true;
        // When the icon row is visible, site info is a button in that row.
        // This is a separate menu item row for the site info shown within the icon row.
        boolean siteSettingsItemVisible = false;
        boolean zoomVisible = false;

        if (ChromeFeatureList.sCctAdaptiveButton.isEnabled()) {
            if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CCT_ADAPTIVE_BUTTON,
                    REMOVE_FIND_IN_PAGE_MENU_ITEM_PARAM,
                    false)) {
                findInPageVisible = false;
            }
            if (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                    ChromeFeatureList.CCT_ADAPTIVE_BUTTON,
                    REMOVE_DESKTOP_SITE_MENU_ITEM_PARAM,
                    false)) {
                requestDesktopSiteVisible = false;
            }
        }
        if (mUiType == CustomTabsUiType.MEDIA_VIEWER) {
            // Most of the menu items don't make sense when viewing media.
            iconRowVisible = false;
            findInPageVisible = false;
            bookmarkItemVisible = false; // Set to skip initialization.
            downloadItemVisible = false; // Set to skip initialization.
            requestDesktopSiteVisible = false;
            addToHomeScreenVisible = false;
            tryAddingReadAloud = false;
        } else if (mUiType == CustomTabsUiType.READER_MODE) {
            // Only 'find in page' and the reader mode preference are shown for Reader Mode UI.
            iconRowVisible = false;
            bookmarkItemVisible = false; // Set to skip initialization.
            downloadItemVisible = false; // Set to skip initialization.
            requestDesktopSiteVisible = false;
            addToHomeScreenVisible = false;
            tryAddingReadAloud = false;
            readerModePrefsVisible = true;
        } else if (mUiType == CustomTabsUiType.MINIMAL_UI_WEBAPP) {
            requestDesktopSiteVisible = false;
            // For Webapps & WebAPKs Verifier#wasPreviouslyVerified() performs verification
            // (instead of looking up cached value).
            addToHomeScreenVisible = !mVerifier.wasPreviouslyVerified(url.getSpec());
            downloadItemVisible = false;
            bookmarkItemVisible = false;
        } else if (mUiType == CustomTabsUiType.TRUSTED_WEB_ACTIVITY) {
            // The CCT menu button was removed for TWAs. This would only affect the
            // app header's menu button.
            addToHomeScreenVisible = !mVerifier.wasPreviouslyVerified(url.getSpec());
            downloadItemVisible = false;
            bookmarkItemVisible = false;
            if (ChromeFeatureList.sAndroidWebAppMenuButton.isEnabled()) {
                requestDesktopSiteVisible = false;

                translateVisible = false;
                // Remove icons.
                iconRowVisible = false;
                // Site settings menu item row.
                siteSettingsItemVisible = true;
                zoomVisible = true;
                findInPageVisible = true;
                mShowShare = true;
            }
        } else if (mUiType == CustomTabsUiType.OFFLINE_PAGE) {
            bookmarkItemVisible = true;
            downloadItemVisible = false;
            addToHomeScreenVisible = false;
            requestDesktopSiteVisible = true;
            tryAddingReadAloud = false;
        } else if (mUiType == CustomTabsUiType.AUTH_TAB) {
            bookmarkItemVisible = false;
            downloadItemVisible = false;
            addToHomeScreenVisible = false;
            tryAddingReadAloud = false;
        } else if (mUiType == CustomTabsUiType.NETWORK_BOUND_TAB) {
            addToHomeScreenVisible = false;
            requestDesktopSiteVisible = true;
        } else if (mUiType == CustomTabsUiType.POPUP) {
            bookmarkItemVisible = false;
            downloadItemVisible = false;
            addToHomeScreenVisible = false;
            requestDesktopSiteVisible = false;
            tryAddingReadAloud = false;
        }

        if (!FirstRunStatus.getFirstRunFlowComplete()) {
            bookmarkItemVisible = false;
            downloadItemVisible = false;
            addToHomeScreenVisible = false;
        }

        if (mIsIncognitoBranded) {
            addToHomeScreenVisible = false;
            downloadItemVisible = false;
            tryAddingReadAloud = false;
        }

        if (CustomTabIntentDataProvider.isOpenInBrowserDisallowed(mUiType, mIsIncognitoBranded)) {
            openInChromeItemVisible = false;
        }
        boolean isNativePage =
                url.getScheme().equals(UrlConstants.CHROME_SCHEME)
                        || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME)
                        || currentTab.isNativePage();
        boolean isFileScheme = url.getScheme().equals(UrlConstants.FILE_SCHEME);
        boolean isContentScheme = url.getScheme().equals(UrlConstants.CONTENT_SCHEME);
        // TODO(crbug.com/384992232): Hide open in Chrome for blob and data url until such view
        //  intent can be handled.
        if ((ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_OPEN_PDF_INLINE)
                        || ChromeFeatureList.isEnabled(
                                ChromeFeatureList.ANDROID_OPEN_PDF_INLINE_BACKPORT))
                && (url.getScheme().equals(UrlConstants.BLOB_SCHEME)
                        || url.getScheme().equals(UrlConstants.DATA_SCHEME))) {
            openInChromeItemVisible = false;
        }
        addToHomeScreenVisible &=
                shouldShowHomeScreenMenuItem(
                        isNativePage, isFileScheme, isContentScheme, mIsOffTheRecord, url);

        // --- Icon Row ---
        if (iconRowVisible) {
            List<PropertyModel> iconModels = new ArrayList<>();
            iconModels.add(buildForwardActionModel(currentTab));

            if (bookmarkItemVisible) {
                iconModels.add(buildBookmarkActionModel(currentTab));
            }

            if (downloadItemVisible) {
                iconModels.add(buildDownloadActionModel(currentTab));
            }

            iconModels.add(buildPageInfoModel(currentTab));
            iconModels.add(buildReloadModel(currentTab));

            modelList.add(
                    new MVCListAdapter.ListItem(
                            AppMenuHandler.AppMenuItemType.BUTTON_ROW,
                            buildModelForIconRow(R.id.icon_row_menu_id, iconModels)));
        }

        // --- App Specific Items / Divider ---
        for (int i = 0; i < mMenuEntries.size(); i++) {
            @IdRes
            int id =
                    switch (i) {
                        case 0 -> R.id.custom_tabs_app_menu_item_id_0;
                        case 1 -> R.id.custom_tabs_app_menu_item_id_1;
                        case 2 -> R.id.custom_tabs_app_menu_item_id_2;
                        case 3 -> R.id.custom_tabs_app_menu_item_id_3;
                        case 4 -> R.id.custom_tabs_app_menu_item_id_4;
                        case 5 -> R.id.custom_tabs_app_menu_item_id_5;
                        case 6 -> R.id.custom_tabs_app_menu_item_id_6;
                        default -> {
                            assert false : "Only 7 custom menu items are currently allowed.";
                            yield 0;
                        }
                    };
            modelList.add(
                    new MVCListAdapter.ListItem(
                            AppMenuHandler.AppMenuItemType.STANDARD,
                            buildBaseModelForTextItem(id)
                                    .with(AppMenuItemProperties.TITLE, mMenuEntries.get(i))
                                    .build()));
            mItemIdToIndexMap.put(id, i);
        }

        if (!mMenuEntries.isEmpty()) {
            modelList.add(
                    new MVCListAdapter.ListItem(
                            AppMenuHandler.AppMenuItemType.DIVIDER,
                            buildModelForDivider(R.id.divider_line_id)));
        }

        // --- App info row ---
        if (siteSettingsItemVisible) {
            modelList.add(
                    new MVCListAdapter.ListItem(
                            AppMenuHandler.AppMenuItemType.STANDARD,
                            buildModelForStandardMenuItem(
                                    R.id.info_menu_id, R.string.menu_app_info, 0)));
        }

        // --- Open in browser ---
        boolean showOpenInBrowserAtTop =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CCT_ADAPTIVE_BUTTON,
                        SHOW_OPEN_IN_BROWSER_MENU_TOP_PARAM,
                        false);
        if (openInChromeItemVisible && showOpenInBrowserAtTop) {
            addOpenInChrome(modelList, /* showIcon= */ true);
        }

        // --- Read Aloud ---
        if (tryAddingReadAloud) {
            // Set visibility of Read Aloud menu item. The entrypoint will be visible iff the tab
            // can be synthesized.
            observeAndMaybeAddReadAloud(modelList, currentTab);
        }

        // --- Reader Mode ---
        if (shouldShowReaderModeItem()) {
            modelList.add(buildReaderModeItem(currentTab));
        }

        // --- Share ---
        if (mShowShare) {
            modelList.add(buildShareListItem(false));
        }

        // --- History ---
        if (CustomTabAppMenuHelper.showHistoryItem(mHasClientPackage, mUiType)) {
            modelList.add(
                    new MVCListAdapter.ListItem(
                            AppMenuHandler.AppMenuItemType.STANDARD,
                            buildModelForStandardMenuItem(
                                    R.id.open_history_menu_id, R.string.chrome_history, 0)));
        }

        // --- Find in Page ---
        if (findInPageVisible) {
            modelList.add(
                    new MVCListAdapter.ListItem(
                            AppMenuHandler.AppMenuItemType.STANDARD,
                            buildModelForStandardMenuItem(
                                    R.id.find_in_page_id, R.string.menu_find_in_page, 0)));
        }

        // --- Reader Mode Prefs ---
        if (readerModePrefsVisible) {
            modelList.add(buildReaderModePrefsItem());
        }

        // --- Price Tracking / Price Insights ---
        if (ChromeFeatureList.sCctAdaptiveButton.isEnabled()) {
            // TODO(crbug.com/391931899): Also check the dev-controlled flag
            MVCListAdapter.ListItem priceTrackingItem =
                    maybeBuildPriceTrackingListItem(currentTab, false);
            if (priceTrackingItem != null) {
                modelList.add(priceTrackingItem);
            }
            var cpaController = mContextualPageActionControllerSupplier.get();
            if (cpaController != null && cpaController.hasPriceInsights()) {
                modelList.add(
                        new MVCListAdapter.ListItem(
                                AppMenuHandler.AppMenuItemType.STANDARD,
                                buildModelForStandardMenuItem(
                                        R.id.price_insights_menu_id,
                                        R.string.price_insights_title,
                                        R.drawable.ic_trending_down_24dp)));
            }
        }

        // --- Add to Homescreen / Open WebAPK ---
        if (addToHomeScreenVisible) {
            modelList.add(buildAddToHomescreenListItem(currentTab, false));
        }

        // --- Request Desktop Site ---
        if (requestDesktopSiteVisible) {
            MVCListAdapter.ListItem rdsListItem =
                    maybeBuildRequestDesktopSiteListItem(currentTab, isNativePage, false);
            if (rdsListItem != null) modelList.add(rdsListItem);
        }

        // --- Translate ---
        if (translateVisible && shouldShowTranslateMenuItem(currentTab)) {
            modelList.add(buildTranslateMenuItem(currentTab, false));
        }

        // --- Open with ---
        if (shouldShowOpenWithItem(currentTab)) {
            modelList.add(buildOpenWithItem(currentTab, false));
        }

        // --- Open in Browser ---
        if (openInChromeItemVisible && !showOpenInBrowserAtTop) {
            addOpenInChrome(modelList, /* showIcon= */ false);
        }

        // --- Zoom ---
        if (zoomVisible) {
            modelList.add(
                    new MVCListAdapter.ListItem(
                            AppMenuHandler.AppMenuItemType.STANDARD,
                            buildModelForStandardMenuItem(
                                    R.id.page_zoom_id, R.string.page_zoom_menu_title, 0)));
        }
        return modelList;
    }

    private void addOpenInChrome(MVCListAdapter.ModelList modelList, boolean showIcon) {
        String title =
                mIsOffTheRecord
                        ? ContextUtils.getApplicationContext()
                                .getString(R.string.menu_open_in_incognito_chrome)
                        : DefaultBrowserInfo.getTitleOpenInDefaultBrowser(mIsOpenedByChrome);
        PropertyModel model =
                buildBaseModelForTextItem(R.id.open_in_browser_id)
                        .with(AppMenuItemProperties.TITLE, title)
                        .build();
        if (showIcon) {
            model.set(
                    AppMenuItemProperties.ICON,
                    AppCompatResources.getDrawable(mContext, R.drawable.ic_open_in_new_white_24dp));
        }
        modelList.add(new MVCListAdapter.ListItem(AppMenuHandler.AppMenuItemType.STANDARD, model));
    }

    private boolean shouldShowReaderModeItem() {
        if (!ChromeFeatureList.sCctAdaptiveButton.isEnabled()
                || !ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.CCT_ADAPTIVE_BUTTON,
                        ReaderModeManager.CPA_FALLBACK_MENU_PARAM,
                        false)) {
            return false;
        }
        var cpaController = mContextualPageActionControllerSupplier.get();
        return cpaController != null && cpaController.hasReaderMode();
    }

    /**
     * @return The index that the given menu item should appear in the result of {@link
     *     BrowserServicesIntentDataProvider#getMenuTitles()}. Returns -1 if item not found.
     */
    public static int getIndexOfMenuItemFromBundle(Bundle menuItemData) {
        if (menuItemData != null && menuItemData.containsKey(CUSTOM_MENU_ITEM_ID_KEY)) {
            return menuItemData.getInt(CUSTOM_MENU_ITEM_ID_KEY);
        }

        return -1;
    }

    @Override
    public @Nullable Bundle getBundleForMenuItem(int itemId) {
        if (!mItemIdToIndexMap.containsKey(itemId)) {
            return null;
        }

        Bundle itemBundle = new Bundle();
        itemBundle.putInt(CUSTOM_MENU_ITEM_ID_KEY, mItemIdToIndexMap.get(itemId).intValue());
        return itemBundle;
    }

    @Override
    public @Nullable View buildFooterView(AppMenuHandler appMenuHandler) {
        // Avoid showing the branded menu footer for media and offline pages.
        if (mUiType == CustomTabsUiType.MEDIA_VIEWER || mUiType == CustomTabsUiType.OFFLINE_PAGE) {
            return null;
        }

        View footer =
                LayoutInflater.from(mContext).inflate(R.layout.powered_by_chrome_footer, null);

        TextView footerTextView = footer.findViewById(R.id.running_in_chrome_footer_text);
        if (footerTextView != null) {
            Resources res = footer.getResources();
            String appName = res.getString(R.string.app_name);
            String footerText = res.getString(R.string.twa_running_in_chrome_template, appName);
            footerTextView.setText(footerText);
        }

        return footer;
    }

    @Override
    public boolean isMenuIconAtStart() {
        return mIsStartIconMenu;
    }

    void setHasClientPackageForTesting(boolean hasClientPackage) {
        mHasClientPackage = hasClientPackage;
    }
}
