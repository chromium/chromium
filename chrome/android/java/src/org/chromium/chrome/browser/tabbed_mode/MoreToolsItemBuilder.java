// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.content.Context;
import android.content.res.Resources;

import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemTheme;
import org.chromium.chrome.browser.app.appmenu.AppMenuItemUtils;
import org.chromium.chrome.browser.devtools.DevToolsWindowAndroid;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/** Builds AppMenu items for the "More tools" submenu. */
@NullMarked
public class MoreToolsItemBuilder {
    private final Context mContext;
    private final AppMenuItemTheme mAppMenuItemTheme;
    private final boolean mIsMenuIconAtStart;
    private final TabModelSelector mTabModelSelector;
    private final Supplier<@Nullable ReadAloudController> mReadAloudControllerSupplier;
    private final Supplier<Boolean> mPageInfoVisibilitySupplier;

    /**
     * Constructs a {@link MoreToolsItemBuilder} which is responsible for building more tools
     * related menu items for the app menu.
     *
     * @param context The Android Context used to get resources.
     * @param appMenuItemTheme The theme used to style the app menu items.
     * @param isMenuIconAtStart Whether the menu icon is displayed at the start.
     */
    public MoreToolsItemBuilder(
            Context context,
            AppMenuItemTheme mAppMenuItemTheme,
            boolean isMenuIconAtStart,
            TabModelSelector tabModelSelector,
            Supplier<@Nullable ReadAloudController> readAloudControllerSupplier,
            Supplier<Boolean> pageInfoVisibilitySupplier) {
        mContext = context;
        this.mAppMenuItemTheme = mAppMenuItemTheme;
        mIsMenuIconAtStart = isMenuIconAtStart;
        mTabModelSelector = tabModelSelector;
        mReadAloudControllerSupplier = readAloudControllerSupplier;
        mPageInfoVisibilitySupplier = pageInfoVisibilitySupplier;
    }

    private boolean isIncognitoShowing() {
        return mTabModelSelector.getCurrentModel().isIncognito();
    }

    /**
     * Returns whether the "Reader mode" menu item should be displayed.
     *
     * @param currentTab The current tab.
     */
    @Contract("null -> false")
    public boolean shouldShowReaderModeItem(@Nullable Tab currentTab) {
        if (currentTab == null) {
            return false;
        }

        GURL url = currentTab.getUrl();
        boolean isChromeOrNativePage =
                url.getScheme().equals(UrlConstants.CHROME_SCHEME)
                        || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME)
                        || currentTab.isNativePage();

        if (isChromeOrNativePage) {
            return false;
        }

        return true;
    }

    /**
     * Returns whether the "More tools" parent menu item should be displayed.
     *
     * @param currentTab The current tab.
     */
    public boolean shouldShowMoreToolsItem(@Nullable Tab currentTab) {
        if (!TabbedAppMenuPropertiesDelegate.isSubmenusEnabled()) {
            return false;
        }

        ReadAloudController readAloudController = mReadAloudControllerSupplier.get();
        if ((readAloudController != null && readAloudController.isReadable(currentTab))
                || shouldShowReaderModeItem(currentTab)
                || shouldShowNameWindowItem()
                || shouldShowTabLayoutToggleItem()
                || shouldShowNtpCustomizations(currentTab)
                || mPageInfoVisibilitySupplier.get()
                || shouldShowTaskManagerItem()
                || shouldShowDevToolsItem(currentTab)) {
            return true;
        }

        return false;
    }

    /** Builds the "Listen to this page" submenu item. */
    public ListItem buildReadAloudSubmenuItem(boolean showIcon) {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.readaloud_menu_id,
                        R.string.menu_listen_to_this_page,
                        showIcon ? R.drawable.ic_play_circle : Resources.ID_NULL,
                        mIsMenuIconAtStart),
                showIcon);
    }

    /** Returns whether the "Name window" menu item should be displayed. */
    public boolean shouldShowNameWindowItem() {
        return MultiWindowUtils.isMultiInstanceApi31Enabled();
    }

    /** Builds the "Name window" menu item. */
    public ListItem buildNameWindowItem(boolean showIcon) {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.name_window_menu_id,
                        R.string.menu_name_window,
                        showIcon ? R.drawable.ic_window_24dp : Resources.ID_NULL,
                        mIsMenuIconAtStart),
                showIcon);
    }

    /** Returns whether the "Toggle tab layout" menu item should be displayed. */
    public boolean shouldShowTabLayoutToggleItem() {
        return VerticalTabUtils.isVerticalTabsEligible(mContext);
    }

    /** Builds the "Toggle tab layout" menu item. */
    public ListItem buildTabLayoutToggleItem(boolean showIcon) {
        boolean isVerticalActive = VerticalTabUtils.isVerticalTabsEnabled(mContext);
        int stringRes =
                isVerticalActive
                        ? org.chromium.chrome.tab_ui.R.string.show_tabs_horizontally
                        : org.chromium.chrome.tab_ui.R.string.show_tabs_vertically;

        int iconRes = Resources.ID_NULL;
        if (showIcon) {
            iconRes =
                    isVerticalActive
                            ? R.drawable.ic_toolbar_24dp
                            : R.drawable.ic_dock_to_right_24dp;
        }

        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.toggle_tab_layout_menu_id,
                        stringRes,
                        iconRes,
                        mIsMenuIconAtStart),
                showIcon);
    }

    /**
     * Returns whether the "NTP customization" menu item should be displayed.
     *
     * @param currentTab The current tab.
     */
    public boolean shouldShowNtpCustomizations(@Nullable Tab currentTab) {
        return !isIncognitoShowing()
                && currentTab != null
                && UrlUtilities.isNtpUrl(currentTab.getUrl());
    }

    /** Builds the "NTP customization" menu item. */
    public ListItem buildNtpCustomizationsItem(boolean showIcon) {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.ntp_customization_id,
                        R.string.menu_ntp_customization,
                        showIcon ? R.drawable.ic_edit_24dp : Resources.ID_NULL,
                        mIsMenuIconAtStart),
                showIcon);
    }

    /** Returns whether the "Task manager" menu item should be displayed. */
    public boolean shouldShowTaskManagerItem() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.TASK_MANAGER_CLANK);
    }

    /** Builds the "Task manager" menu item. */
    public ListItem buildTaskManagerItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.task_manager,
                        R.string.menu_task_manager,
                        Resources.ID_NULL,
                        mIsMenuIconAtStart),
                /* showIcon= */ false);
    }

    /**
     * Returns whether the "Dev tools" menu item should be displayed.
     *
     * @param currentTab The current tab.
     */
    public boolean shouldShowDevToolsItem(@Nullable Tab currentTab) {
        if (!ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_DEV_TOOLS_FRONTEND)
                || !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                || currentTab == null
                || currentTab.isNativePage()) {
            return false;
        }

        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) {
            return false;
        }

        return DevToolsWindowAndroid.isDevToolsAllowedFor(currentTab.getProfile(), webContents);
    }

    /** Builds the "Dev tools" menu item. */
    public ListItem buildDevToolsItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.dev_tools,
                        R.string.menu_dev_tools,
                        Resources.ID_NULL,
                        mIsMenuIconAtStart),
                /* showIcon= */ false);
    }
}
