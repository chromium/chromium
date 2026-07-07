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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;

/** Builds AppMenu items related to saving and sharing. */
@NullMarked
public class SaveAndShareItemBuilder {
    private final Context mContext;
    private final AppMenuItemTheme mAppMenuItemTheme;
    private final boolean mIsMenuIconAtStart;
    private final TabModelSelector mTabModelSelector;

    /**
     * Constructs a {@link SaveAndShareItemBuilder} which is responsible for building saving and
     * sharing related menu items for the app menu.
     *
     * @param context The Android Context used to get resources.
     * @param appMenuItemTheme The theme used to style the app menu items.
     * @param isMenuIconAtStart Whether the menu icon is displayed at the start.
     */
    public SaveAndShareItemBuilder(
            Context context,
            AppMenuItemTheme appMenuItemTheme,
            boolean isMenuIconAtStart,
            TabModelSelector tabModelSelector) {
        mContext = context;
        mAppMenuItemTheme = appMenuItemTheme;
        mIsMenuIconAtStart = isMenuIconAtStart;
        mTabModelSelector = tabModelSelector;
    }

    private boolean isIncognitoShowing() {
        return mTabModelSelector.getCurrentModel().isIncognito();
    }

    /** Builds the "Copy link" menu item. */
    public ListItem buildCopyLinkItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.copy_link_menu_id,
                        R.string.menu_copy_link,
                        Resources.ID_NULL,
                        mIsMenuIconAtStart),
                /* showIcon= */ false);
    }

    /** Builds the "Send to devices" menu item. */
    public ListItem buildSendToDevicesItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.send_to_devices_menu_id,
                        R.string.menu_send_to_devices,
                        Resources.ID_NULL,
                        mIsMenuIconAtStart),
                /* showIcon= */ false);
    }

    /** Builds the "QR Code" menu item. */
    public ListItem buildShareQrCodeItem() {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.qr_code_menu_id,
                        R.string.menu_qr_code,
                        Resources.ID_NULL,
                        mIsMenuIconAtStart),
                /* showIcon= */ false);
    }

    /**
     * Builds the "Download page" menu item.
     *
     * @param showIcon Whether to display an icon next to the item.
     * @return A {@link ListItem} representing the Download page menu item.
     */
    public ListItem buildDownloadPageItem(boolean showIcon) {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.download_page_id,
                        R.string.menu_download_page,
                        showIcon ? R.drawable.ic_file_download_white_24dp : Resources.ID_NULL,
                        mIsMenuIconAtStart),
                showIcon);
    }

    /**
     * Returns whether the paint preview menu item should be displayed.
     *
     * @param isNativePage Whether the current tab is a native page.
     * @param currentTab The currentTab for which the app menu is showing.
     * @return Whether the paint preview menu item should be displayed.
     */
    @Contract("_, null -> false")
    public boolean shouldShowPaintPreview(boolean isNativePage, @Nullable Tab currentTab) {
        return currentTab != null
                && ChromeFeatureList.sPaintPreviewDemo.isEnabled()
                && !isNativePage
                && !isIncognitoShowing();
    }

    /**
     * Builds the "Paint preview" menu item.
     *
     * @param showIcon Whether to display an icon next to the item.
     * @return A {@link ListItem} representing the Paint preview menu item.
     */
    public ListItem buildPaintPreviewItem(boolean showIcon) {
        return AppMenuItemUtils.createStandardListItem(
                AppMenuItemUtils.buildModelForStandardMenuItem(
                        mContext,
                        mAppMenuItemTheme,
                        R.id.paint_preview_show_id,
                        R.string.menu_paint_preview_show,
                        showIcon ? R.drawable.ic_photo_camera : Resources.ID_NULL,
                        mIsMenuIconAtStart),
                showIcon);
    }
}
