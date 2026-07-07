// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemWithSubmenuProperties;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.function.Supplier;

/** Utility class for building AppMenu property models and list items. */
@NullMarked
public class AppMenuItemUtils {
    /**
     * Builds a property model for a divider item type.
     *
     * @param id The id of the divider.
     * @return The property model for this item.
     */
    public static PropertyModel buildModelForDivider(@IdRes int id) {
        return new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                .with(AppMenuItemProperties.MENU_ITEM_ID, id)
                .build();
    }

    /**
     * Constructs the basis for text menu items models.
     *
     * @param theme The AppMenuItemTheme handling theming aspects.
     * @param id The id of the text menu item.
     * @param isMenuIconAtStart Whether the menu icon should be placed at the start.
     * @return A Builder object that forms the basis for text menu item models.
     */
    public static PropertyModel.Builder buildBaseModelForTextItem(
            AppMenuItemTheme theme, @IdRes int id, boolean isMenuIconAtStart) {
        return populateBaseModelForTextItem(
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS),
                theme,
                id,
                isMenuIconAtStart);
    }

    /**
     * Populates the PropertyModel.Builder with the common properties for a text menu item.
     *
     * @param builder The builder to populate with data.
     * @param theme The AppMenuItemTheme handling theming aspects.
     * @param id The id of the text menu item.
     * @param isMenuIconAtStart Whether the menu icon should be placed at the start.
     * @return A Builder object that forms the basis for text menu item models.
     */
    public static PropertyModel.Builder populateBaseModelForTextItem(
            PropertyModel.Builder builder,
            AppMenuItemTheme theme,
            @IdRes int id,
            boolean isMenuIconAtStart) {
        return builder.with(AppMenuItemProperties.MENU_ITEM_ID, id)
                .with(AppMenuItemProperties.ENABLED, true)
                .with(AppMenuItemProperties.ICON_COLOR_RES, theme.getMenuItemIconColorRes(id))
                .with(
                        AppMenuItemProperties.ICON_SHOW_BADGE,
                        theme.shouldShowBadgeOnMenuItemIcon(id))
                .with(AppMenuItemProperties.MENU_ICON_AT_START, isMenuIconAtStart)
                .with(AppMenuItemProperties.TITLE_CONDENSED, theme.getContentDescription(id))
                .with(AppMenuItemProperties.MANAGED, theme.isMenuItemManaged(id));
    }

    /**
     * Build a property model for a standard text menu item.
     *
     * @param context The Context used to resolve resources.
     * @param theme The AppMenuItemTheme handling theming aspects.
     * @param id The id of the menu item.
     * @param titleId The resource id of the title to be displayed.
     * @param iconResId The resource id of the icon to be displayed (or 0 for no icon).
     * @param isMenuIconAtStart Whether the menu icon should be placed at the start.
     * @return The property model for this item.
     */
    public static PropertyModel buildModelForStandardMenuItem(
            Context context,
            AppMenuItemTheme theme,
            @IdRes int id,
            @StringRes int titleId,
            @DrawableRes int iconResId,
            boolean isMenuIconAtStart) {
        PropertyModel model =
                buildBaseModelForTextItem(theme, id, isMenuIconAtStart)
                        .with(AppMenuItemProperties.TITLE, context.getString(titleId))
                        .build();
        if (iconResId != 0) {
            model.set(
                    AppMenuItemProperties.ICON, AppCompatResources.getDrawable(context, iconResId));
        }
        return model;
    }

    /**
     * Build a property model for a text menu item w/ checkbox.
     *
     * @param context The Context used to resolve resources.
     * @param theme The AppMenuItemTheme handling theming aspects.
     * @param id The id of the menu item.
     * @param titleId The resource id of the title to be displayed.
     * @param iconResId The resource id of the icon to be displayed (or 0 for no icon).
     * @param checkBoxId The id of the checkbox item.
     * @param isChecked Whether the checkbox is currently checked.
     * @param isMenuIconAtStart Whether the menu icon should be placed at the start.
     * @return The property model for this item.
     */
    public static PropertyModel buildModelForMenuItemWithCheckbox(
            Context context,
            AppMenuItemTheme theme,
            @IdRes int id,
            @StringRes int titleId,
            @DrawableRes int iconResId,
            @IdRes int checkBoxId,
            boolean isChecked,
            boolean isMenuIconAtStart) {
        PropertyModel checkBoxModel =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_ICON_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, checkBoxId)
                        .with(AppMenuItemProperties.CHECKABLE, true)
                        .with(AppMenuItemProperties.CHECKED, isChecked)
                        .with(AppMenuItemProperties.ENABLED, true)
                        .build();
        ModelList subList = new ModelList();
        subList.add(new ListItem(0, checkBoxModel));

        PropertyModel model =
                buildBaseModelForTextItem(theme, id, isMenuIconAtStart)
                        .with(AppMenuItemProperties.TITLE, context.getString(titleId))
                        .with(AppMenuItemProperties.ADDITIONAL_ICONS, subList)
                        .build();
        if (iconResId != 0) {
            model.set(
                    AppMenuItemProperties.ICON, AppCompatResources.getDrawable(context, iconResId));
        }
        return model;
    }

    /**
     * Build a property model for a text menu item w/ secondary action button.
     *
     * @param context The Context used to resolve resources.
     * @param theme The AppMenuItemTheme handling theming aspects.
     * @param id The id of the menu item.
     * @param titleId The resource id of the title to be displayed.
     * @param iconResId The resource id of the icon to be displayed (or 0 for no icon).
     * @param secondaryActionId The id of the secondary action.
     * @param secondaryActionTitle The title for the secondary action.
     * @param secondaryActionIcon The icon for the secondary action.
     * @param isMenuIconAtStart Whether the menu icon should be placed at the start.
     * @return The property model for this item.
     */
    public static PropertyModel buildModelForMenuItemWithSecondaryButton(
            Context context,
            AppMenuItemTheme theme,
            @IdRes int id,
            @StringRes int titleId,
            @DrawableRes int iconResId,
            @IdRes int secondaryActionId,
            CharSequence secondaryActionTitle,
            Drawable secondaryActionIcon,
            boolean isMenuIconAtStart) {
        PropertyModel secondaryActionModel =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, secondaryActionId)
                        .with(AppMenuItemProperties.TITLE, secondaryActionTitle)
                        .with(AppMenuItemProperties.ICON, secondaryActionIcon)
                        .with(AppMenuItemProperties.ENABLED, true)
                        .build();

        ModelList subList = new ModelList();
        subList.add(new MVCListAdapter.ListItem(0, secondaryActionModel));

        PropertyModel model =
                buildBaseModelForTextItem(theme, id, isMenuIconAtStart)
                        .with(AppMenuItemProperties.TITLE, context.getString(titleId))
                        .with(AppMenuItemProperties.ADDITIONAL_ICONS, subList)
                        .build();
        if (iconResId != 0) {
            model.set(
                    AppMenuItemProperties.ICON, AppCompatResources.getDrawable(context, iconResId));
        }
        return model;
    }

    /**
     * Build a property model for a menu item with submenu.
     *
     * @param context The Context used to resolve resources.
     * @param theme The AppMenuItemTheme handling theming aspects.
     * @param id The id of the menu item.
     * @param titleId The resource id of the title to be displayed.
     * @param iconResId The resource id of the icon to be displayed (or 0 for no icon).
     * @param submenuItemProvider The provider of {@code ListItem}s in the submenu.
     * @param isMenuIconAtStart Whether the menu icon should be placed at the start.
     * @return The property model for this item.
     */
    public static PropertyModel buildModelForMenuItemWithSubmenu(
            Context context,
            AppMenuItemTheme theme,
            @IdRes int id,
            @StringRes int titleId,
            @DrawableRes int iconResId,
            Supplier<List<ListItem>> submenuItemProvider,
            boolean isMenuIconAtStart) {
        return buildModelForMenuItemWithSubmenu(
                context,
                theme,
                id,
                context.getString(titleId),
                iconResId,
                submenuItemProvider,
                isMenuIconAtStart);
    }

    /**
     * Build a property model for a menu item with submenu.
     *
     * @param context The Context used to resolve resources.
     * @param theme The AppMenuItemTheme handling theming aspects.
     * @param id The id of the menu item.
     * @param title The title to be displayed.
     * @param iconResId The resource id of the icon to be displayed (or 0 for no icon).
     * @param submenuItemProvider The provider of {@code ListItem}s in the submenu.
     * @param isMenuIconAtStart Whether the menu icon should be placed at the start.
     * @return The property model for this item.
     */
    public static PropertyModel buildModelForMenuItemWithSubmenu(
            Context context,
            AppMenuItemTheme theme,
            @IdRes int id,
            String title,
            @DrawableRes int iconResId,
            Supplier<List<ListItem>> submenuItemProvider,
            boolean isMenuIconAtStart) {
        Drawable icon = iconResId != 0 ? AppCompatResources.getDrawable(context, iconResId) : null;
        return buildModelForMenuItemWithSubmenu(
                context, theme, id, title, icon, submenuItemProvider, isMenuIconAtStart);
    }

    /**
     * Build a property model for a menu item with submenu.
     *
     * @param context The Context used to resolve resources.
     * @param theme The AppMenuItemTheme handling theming aspects.
     * @param id The id of the menu item.
     * @param titleId The resource id of the title to be displayed.
     * @param icon The icon to be displayed (or null for no icon).
     * @param submenuItemProvider The provider of {@code ListItem}s in the submenu.
     * @param isMenuIconAtStart Whether the menu icon should be placed at the start.
     * @return The property model for this item.
     */
    public static PropertyModel buildModelForMenuItemWithSubmenu(
            Context context,
            AppMenuItemTheme theme,
            @IdRes int id,
            @StringRes int titleId,
            @Nullable Drawable icon,
            Supplier<List<ListItem>> submenuItemProvider,
            boolean isMenuIconAtStart) {
        return buildModelForMenuItemWithSubmenu(
                context,
                theme,
                id,
                context.getString(titleId),
                icon,
                submenuItemProvider,
                isMenuIconAtStart);
    }

    /**
     * Build a property model for a menu item with submenu.
     *
     * @param context The Context used to resolve resources.
     * @param theme The AppMenuItemTheme handling theming aspects.
     * @param id The id of the menu item.
     * @param title The title to be displayed.
     * @param icon The icon to be displayed (or null for no icon).
     * @param submenuItemProvider The provider of {@code ListItem}s in the submenu.
     * @param isMenuIconAtStart Whether the menu icon should be placed at the start.
     * @return The property model for this item.
     */
    public static PropertyModel buildModelForMenuItemWithSubmenu(
            Context context,
            AppMenuItemTheme theme,
            @IdRes int id,
            String title,
            @Nullable Drawable icon,
            Supplier<List<ListItem>> submenuItemProvider,
            boolean isMenuIconAtStart) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemWithSubmenuProperties.ALL_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, id)
                        .with(AppMenuItemProperties.TITLE, title)
                        .with(AppMenuItemProperties.ENABLED, true)
                        .with(
                                AppMenuItemProperties.ICON_COLOR_RES,
                                theme.getMenuItemIconColorRes(id))
                        .with(AppMenuItemProperties.MENU_ICON_AT_START, isMenuIconAtStart)
                        .with(AppMenuItemProperties.MANAGED, theme.isMenuItemManaged(id))
                        .with(
                                AppMenuItemWithSubmenuProperties.SUBMENU_PROVIDER,
                                submenuItemProvider)
                        .with(
                                AppMenuItemProperties.ICON_SHOW_BADGE,
                                theme.shouldShowBadgeOnMenuItemIcon(id))
                        .build();
        if (icon != null) {
            model.set(AppMenuItemProperties.ICON, icon);
        }
        return model;
    }

    /**
     * Build a property model for an icon row button.
     *
     * @param context The Context used to resolve resources.
     * @param id The id of the menu item.
     * @param titleId The resource id of the title for this icon.
     * @param titleCondensedId The resource id of the condensed title for this icon, which is used
     *     for accessibility.
     * @param iconResId The resource id of the icon to be displayed.
     * @return The property model for this item.
     */
    public static PropertyModel buildModelForIcon(
            Context context,
            @IdRes int id,
            @StringRes int titleId,
            @StringRes int titleCondensedId,
            @DrawableRes int iconResId) {
        PropertyModel model =
                new PropertyModel.Builder(AppMenuItemProperties.ALL_ICON_KEYS)
                        .with(AppMenuItemProperties.MENU_ITEM_ID, id)
                        .with(AppMenuItemProperties.TITLE, context.getString(titleId))
                        .with(
                                AppMenuItemProperties.TITLE_CONDENSED,
                                context.getString(titleCondensedId))
                        .with(AppMenuItemProperties.ENABLED, true)
                        .build();
        if (iconResId != 0) {
            model.set(
                    AppMenuItemProperties.ICON, AppCompatResources.getDrawable(context, iconResId));
        }
        return model;
    }

    /**
     * Build a property model for an icon row.
     *
     * @param id The id of the menu item.
     * @param iconModels The list of models representing the icons in the row.
     * @param isMenuIconAtStart Whether the menu icon should be placed at the start.
     * @return The property model for this item.
     */
    public static PropertyModel buildModelForIconRow(
            @IdRes int id, List<PropertyModel> iconModels, boolean isMenuIconAtStart) {
        ModelList subList = new ModelList();
        for (PropertyModel iconModel : iconModels) {
            subList.add(new MVCListAdapter.ListItem(0, iconModel));
        }

        return new PropertyModel.Builder(AppMenuItemProperties.ALL_KEYS)
                .with(AppMenuItemProperties.MENU_ITEM_ID, id)
                .with(AppMenuItemProperties.ADDITIONAL_ICONS, subList)
                .with(AppMenuItemProperties.MENU_ICON_AT_START, isMenuIconAtStart)
                .build();
    }

    /**
     * Build a list item for a standard menu item.
     *
     * @param model The property model for this item.
     * @param showIcon Whether the icon should be displayed.
     * @return The list item for this menu item.
     */
    public static MVCListAdapter.ListItem createStandardListItem(
            PropertyModel model, boolean showIcon) {
        return new MVCListAdapter.ListItem(
                showIcon
                        ? AppMenuHandler.AppMenuItemType.STANDARD
                        : AppMenuHandler.AppMenuItemType.STANDARD_NO_ICON,
                model);
    }

    /**
     * Build a list item for a menu item with submenu.
     *
     * @param model The property model for this item.
     * @param showIcon Whether the icon should be displayed.
     * @return The list item for this menu item.
     */
    public static MVCListAdapter.ListItem createMenuItemWithSubmenuListItem(
            PropertyModel model, boolean showIcon) {
        return new MVCListAdapter.ListItem(
                showIcon
                        ? AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU
                        : AppMenuHandler.AppMenuItemType.MENU_ITEM_WITH_SUBMENU_NO_ICON,
                model);
    }

    /**
     * Build a list item for a header menu item.
     *
     * @param context The Context used to resolve resources.
     * @param theme The AppMenuItemTheme handling theming aspects.
     * @param id The id of the menu item.
     * @param titleRes The resource id of the title to be displayed.
     * @param isMenuIconAtStart Whether the menu icon should be placed at the start.
     * @return The list item for this menu item.
     */
    public static MVCListAdapter.ListItem buildHeaderItem(
            Context context,
            AppMenuItemTheme theme,
            @IdRes int id,
            @StringRes int titleRes,
            boolean isMenuIconAtStart) {
        return new MVCListAdapter.ListItem(
                AppMenuHandler.AppMenuItemType.HEADER,
                buildBaseModelForTextItem(theme, id, isMenuIconAtStart)
                        .with(AppMenuItemProperties.TITLE, context.getString(titleRes))
                        .build());
    }
}
