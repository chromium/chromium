// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.content.Context;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.incognito.R;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A delegate for the menu button present inside the Incognito re-auth view full page. */
class IncognitoReauthMenuDelegate implements ListMenu.Delegate {
    /**
     * An enum interface denoting the various options (in-order) present in the
     * three dots menu in the incognito re-auth full page view.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MenuItemType.CLOSE_INCOGNITO_TABS, MenuItemType.SETTINGS})
    public @interface MenuItemType {
        int CLOSE_INCOGNITO_TABS = 1;
        int SETTINGS = 2;
    }

    private final Context mContext;
    private final Runnable mCloseAllIncognitoTabsRunnable;
    private final BasicListMenu mIncognitoReauthMenu;

    /**
     * @param context The {@link Context} from where the android resources would be fetched.
     * @param closeAllIncognitoTabRunnable The {@link Runnable} which would be used to close the
     *     Incognito tabs when the user clicks on "Close Incognito tabs" option.
     */
    IncognitoReauthMenuDelegate(
            @NonNull Context context, @NonNull Runnable closeAllIncognitoTabRunnable) {
        mContext = context;
        mCloseAllIncognitoTabsRunnable = closeAllIncognitoTabRunnable;
        mIncognitoReauthMenu = buildIncognitoReauthMenu();
    }

    /**
     * An implementation for the {@link ListMenu.Delegate} that holds the functionality associated
     * with the menu items.
     */
    @Override
    public void onItemSelected(PropertyModel item) {
        int textId = item.get(ListMenuItemProperties.TITLE_ID);
        if (textId == R.string.menu_close_all_incognito_tabs) {
            onCloseAllIncognitoTabsMenuItemClicked();
        } else if (textId == R.string.menu_settings) {
            onSettingsMenuItemClicked();
        } else {
            assert false : "No action defined for " + mContext.getString(textId);
        }
    }

    /**
     * @return The underlying {@link BasicListMenu} instance.
     */
    BasicListMenu getBasicListMenu() {
        return mIncognitoReauthMenu;
    }

    /**
     * @return {@link ListMenuButtonDelegate} which returns the underlying menu delegate.
     */
    ListMenuButtonDelegate getListMenuButtonDelegate() {
        return () -> mIncognitoReauthMenu;
    }

    private BasicListMenu buildIncognitoReauthMenu() {
        MVCListAdapter.ModelList itemList = buildMenuItems();
        return BrowserUiListMenuUtils.getBasicListMenu(
                mContext, itemList, this, R.color.menu_item_bg_color_dark_baseline);
    }

    private MVCListAdapter.ModelList buildMenuItems() {
        MVCListAdapter.ModelList itemList = new MVCListAdapter.ModelList();
        itemList.add(buildListItemByMenuItemType(MenuItemType.CLOSE_INCOGNITO_TABS));
        itemList.add(buildListItemByMenuItemType(MenuItemType.SETTINGS));
        return itemList;
    }

    private MVCListAdapter.ListItem buildListItemByMenuItemType(@MenuItemType int type) {
        switch (type) {
            case MenuItemType.CLOSE_INCOGNITO_TABS:
                return buildMenuListItemWithCustomApperance(
                        /* titleId= */ R.string.menu_close_all_incognito_tabs,
                        /* menuId= */ 0,
                        /* startIconId= */ R.drawable.btn_close,
                        /* enabled= */ true,
                        /* colorTint= */ R.color.default_icon_color_secondary_light_tint_list,
                        /* textAppearanceStyle= */ R.style
                                .TextAppearance_TextLarge_Primary_Baseline_Light,
                        /* textEllipsizedAtEnd= */ true);
            case MenuItemType.SETTINGS:
                return buildMenuListItemWithCustomApperance(
                        /* titleId= */ R.string.menu_settings,
                        /* menuId= */ 0,
                        /* startIconId= */ R.drawable.settings_cog,
                        /* enabled= */ true,
                        /* colorTint= */ R.color.default_icon_color_secondary_light_tint_list,
                        /* textAppearanceStyle= */ R.style
                                .TextAppearance_TextLarge_Primary_Baseline_Light,
                        /* textEllipsizedAtEnd= */ true);
            default:
                assert false : "Not implemented yet.";
                return null;
        }
    }

    private void onCloseAllIncognitoTabsMenuItemClicked() {
        mCloseAllIncognitoTabsRunnable.run();
    }

    private void onSettingsMenuItemClicked() {
        SettingsNavigationFactory.createSettingsNavigation().startSettings(mContext);
    }

    /**
     * Helper function to build a list menu item. Pass 0 for attributes that aren't applicable to
     * the menu item (e.g. if there is no icon or text).
     *
     * @param titleId The text on the menu item.
     * @param menuId Id of the menu item.
     * @param startIconId The icon on the start of the menu item.
     * @param enabled Whether or not this menu item should be enabled.
     * @param colorTint The color tinr to apply on the menu item icons.
     * @param textAppearanceStyle The style to apply on the text.
     * @param textEllipsizedAtEnd Whether to ellipsize the text at the end when it doesn't fit the
     *     view width.
     * @return ListItem Representing an item with text or icon.
     */
    private static MVCListAdapter.ListItem buildMenuListItemWithCustomApperance(
            @StringRes int titleId,
            @IdRes int menuId,
            @DrawableRes int startIconId,
            boolean enabled,
            int colorTint,
            int textAppearanceStyle,
            boolean textEllipsizedAtEnd) {
        return new MVCListAdapter.ListItem(
                BasicListMenu.ListMenuItemType.MENU_ITEM,
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE_ID, titleId)
                        .with(ListMenuItemProperties.MENU_ITEM_ID, menuId)
                        .with(ListMenuItemProperties.START_ICON_ID, startIconId)
                        .with(ListMenuItemProperties.ENABLED, enabled)
                        .with(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID, colorTint)
                        .with(ListMenuItemProperties.TEXT_APPEARANCE_ID, textAppearanceStyle)
                        .with(ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END, textEllipsizedAtEnd)
                        .build());
    }
}
