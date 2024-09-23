// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.chrome.browser.ui.appmenu.internal.R;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.ChromeImageView;

/** The binder to bind the app menu {@link PropertyModel} with the view. */
class AppMenuItemViewBinder {
    /** IDs of all of the buttons in icon_row_menu_item.xml. */
    private static final int[] BUTTON_IDS = {
        R.id.button_one, R.id.button_two, R.id.button_three, R.id.button_four, R.id.button_five
    };

    public static void bindStandardItem(PropertyModel model, View view, PropertyKey key) {
        AppMenuUtil.bindStandardItemEnterAnimation(model, view, key);

        if (key == AppMenuItemProperties.MENU_ITEM_ID) {
            int id = model.get(AppMenuItemProperties.MENU_ITEM_ID);
            view.setId(id);
        } else if (key == AppMenuItemProperties.TITLE) {
            ((TextView) view.findViewById(R.id.menu_item_text))
                    .setText(model.get(AppMenuItemProperties.TITLE));
        } else if (key == AppMenuItemProperties.TITLE_CONDENSED) {
            setContentDescription(view.findViewById(R.id.menu_item_text), model);
        } else if (key == AppMenuItemProperties.ENABLED) {
            boolean enabled = model.get(AppMenuItemProperties.ENABLED);
            view.setEnabled(enabled);
        } else if (key == AppMenuItemProperties.HIGHLIGHTED) {
            if (model.get(AppMenuItemProperties.HIGHLIGHTED)) {
                ViewHighlighter.turnOnHighlight(
                        view, new HighlightParams(HighlightShape.RECTANGLE));
            } else {
                ViewHighlighter.turnOffHighlight(view);
            }
        } else if (key == AppMenuItemProperties.ICON) {
            Drawable icon = model.get(AppMenuItemProperties.ICON);
            ChromeImageView imageView = (ChromeImageView) view.findViewById(R.id.menu_item_icon);

            @ColorRes int colorResId = model.get(AppMenuItemProperties.ICON_COLOR_RES);
            if (colorResId == 0) {
                // If there is no color assigned to the icon, use the default color.
                colorResId = R.color.default_icon_color_secondary_tint_list;
            }
            ColorStateList tintList =
                    AppCompatResources.getColorStateList(imageView.getContext(), colorResId);

            if (model.get(AppMenuItemProperties.ICON_SHOW_BADGE)) {
                // Draw the icon with a red badge on top.
                icon =
                        UiUtils.drawIconWithBadge(
                                imageView.getContext(),
                                icon,
                                colorResId,
                                R.dimen.menu_item_icon_badge_size,
                                R.dimen.menu_item_icon_badge_border_size,
                                R.color.default_red);
                // `colorResId` has already been applied by `drawIconWithBadge` and thus, passing
                // `tintList` is not required.
                // Note that tint is set to null to clear any tint previously set via XML.
                tintList = null;
            }

            imageView.setImageDrawable(icon);
            imageView.setVisibility(icon == null ? View.GONE : View.VISIBLE);

            // tint the icon
            ImageViewCompat.setImageTintList(imageView, tintList);
        } else if (key == AppMenuItemProperties.CLICK_HANDLER) {
            view.setOnClickListener(
                    v -> model.get(AppMenuItemProperties.CLICK_HANDLER).onItemClick(model));
        }
    }

    public static void bindTitleButtonItem(PropertyModel model, View view, PropertyKey key) {
        AppMenuUtil.bindStandardItemEnterAnimation(model, view, key);

        if (key == AppMenuItemProperties.SUBMENU) {
            ModelList subList = model.get(AppMenuItemProperties.SUBMENU);
            PropertyModel titleModel = subList.get(0).model;

            view.setId(titleModel.get(AppMenuItemProperties.MENU_ITEM_ID));

            TextViewWithCompoundDrawables title =
                    (TextViewWithCompoundDrawables) view.findViewById(R.id.title);
            title.setText(titleModel.get(AppMenuItemProperties.TITLE));
            title.setEnabled(titleModel.get(AppMenuItemProperties.ENABLED));
            title.setFocusable(titleModel.get(AppMenuItemProperties.ENABLED));
            title.setCompoundDrawablesRelative(
                    titleModel.get(AppMenuItemProperties.ICON), null, null, null);
            setContentDescription(title, titleModel);

            AppMenuClickHandler appMenuClickHandler =
                    model.get(AppMenuItemProperties.CLICK_HANDLER);
            title.setOnClickListener(v -> appMenuClickHandler.onItemClick(titleModel));
            if (titleModel.get(AppMenuItemProperties.HIGHLIGHTED)) {
                ViewHighlighter.turnOnHighlight(
                        view, new HighlightParams(HighlightShape.RECTANGLE));
            } else {
                ViewHighlighter.turnOffHighlight(view);
            }

            AppMenuItemIcon checkbox = (AppMenuItemIcon) view.findViewById(R.id.checkbox);
            ChromeImageButton button = (ChromeImageButton) view.findViewById(R.id.button);
            PropertyModel buttonModel = null;
            boolean checkable = false;
            boolean checked = false;
            boolean buttonEnabled = true;
            Drawable subIcon = null;

            if (subList.size() == 2) {
                buttonModel = subList.get(1).model;
                checkable = buttonModel.get(AppMenuItemProperties.CHECKABLE);
                checked = buttonModel.get(AppMenuItemProperties.CHECKED);
                buttonEnabled = buttonModel.get(AppMenuItemProperties.ENABLED);
                subIcon = buttonModel.get(AppMenuItemProperties.ICON);
            }

            if (checkable) {
                // Display a checkbox for the MenuItem.
                button.setVisibility(View.GONE);
                checkbox.setVisibility(View.VISIBLE);
                checkbox.setChecked(checked);
                ImageViewCompat.setImageTintList(
                        checkbox,
                        AppCompatResources.getColorStateList(
                                checkbox.getContext(), R.color.selection_control_button_tint_list));
                setupMenuButton(checkbox, buttonModel, appMenuClickHandler);
            } else if (subIcon != null) {
                // Display an icon alongside the MenuItem.
                checkbox.setVisibility(View.GONE);
                button.setVisibility(View.VISIBLE);
                if (!buttonEnabled) {
                    // Only grey out the icon when disabled. When the menu is enabled, use the
                    // icon's original color.
                    Drawable icon = buttonModel.get(AppMenuItemProperties.ICON);
                    DrawableCompat.setTintList(
                            icon,
                            AppCompatResources.getColorStateList(
                                    button.getContext(),
                                    R.color.default_icon_color_secondary_tint_list));
                    buttonModel.set(AppMenuItemProperties.ICON, icon);
                }
                setupImageButton(button, buttonModel, appMenuClickHandler);
            } else {
                // Display just the label of the MenuItem.
                checkbox.setVisibility(View.GONE);
                button.setVisibility(View.GONE);
            }
        } else if (key == AppMenuItemProperties.HIGHLIGHTED) {
            if (model.get(AppMenuItemProperties.HIGHLIGHTED)) {
                ViewHighlighter.turnOnHighlight(
                        view, new HighlightParams(HighlightShape.RECTANGLE));
            } else {
                ViewHighlighter.turnOffHighlight(view);
            }
        }
    }

    public static void bindIconRowItem(PropertyModel model, View view, PropertyKey key) {
        if (key == AppMenuItemProperties.SUBMENU) {
            ModelList iconList = model.get(AppMenuItemProperties.SUBMENU);

            int numItems = iconList.size();
            ImageButton[] buttons = new ImageButton[numItems];
            // Save references to all the buttons.
            for (int i = 0; i < numItems; i++) {
                buttons[i] = (ImageButton) view.findViewById(BUTTON_IDS[i]);
            }

            // Remove unused menu items.
            for (int i = numItems; i < 5; i++) {
                ((ViewGroup) view).removeView(view.findViewById(BUTTON_IDS[i]));
            }

            AppMenuClickHandler appMenuClickHandler =
                    model.get(AppMenuItemProperties.CLICK_HANDLER);
            for (int i = 0; i < numItems; i++) {
                setupImageButton(buttons[i], iconList.get(i).model, appMenuClickHandler);
            }

            boolean isMenuIconAtStart = model.get(AppMenuItemProperties.MENU_ICON_AT_START);
            view.setTag(
                    R.id.menu_item_enter_anim_id,
                    AppMenuUtil.buildIconItemEnterAnimator(buttons, isMenuIconAtStart));

            // Tint action bar's background.
            view.setBackgroundResource(R.drawable.menu_action_bar_bg);

            view.setEnabled(false);
        }
    }

    public static void setContentDescription(View view, final PropertyModel model) {
        CharSequence titleCondensed = model.get(AppMenuItemProperties.TITLE_CONDENSED);
        if (TextUtils.isEmpty(titleCondensed)) {
            view.setContentDescription(null);
        } else {
            view.setContentDescription(titleCondensed);
        }
    }

    private static void setupImageButton(
            ImageButton button,
            final PropertyModel model,
            AppMenuClickHandler appMenuClickHandler) {
        // Store and recover the level of image as button.setimageDrawable
        // resets drawable to default level.
        Drawable icon = model.get(AppMenuItemProperties.ICON);
        int currentLevel = icon.getLevel();
        button.setImageDrawable(icon);
        icon.setLevel(currentLevel);

        // TODO(gangwu): Resetting this tint if we go from checked -> not checked while the menu is
        // visible.
        if (model.get(AppMenuItemProperties.CHECKED)) {
            ImageViewCompat.setImageTintList(
                    button,
                    AppCompatResources.getColorStateList(
                            button.getContext(), R.color.default_icon_color_accent1_tint_list));
        }

        setupMenuButton(button, model, appMenuClickHandler);
    }

    private static void setupMenuButton(
            View button, final PropertyModel model, AppMenuClickHandler appMenuClickHandler) {
        // On Android M, even setEnabled(false), the button still focusable.
        button.setEnabled(model.get(AppMenuItemProperties.ENABLED));
        button.setFocusable(model.get(AppMenuItemProperties.ENABLED));

        CharSequence titleCondensed = model.get(AppMenuItemProperties.TITLE_CONDENSED);
        if (TextUtils.isEmpty(titleCondensed)) {
            button.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        } else {
            button.setContentDescription(titleCondensed);
            button.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);
        }

        button.setOnClickListener(v -> appMenuClickHandler.onItemClick(model));
        button.setOnLongClickListener(v -> appMenuClickHandler.onItemLongClick(model, button));

        if (model.get(AppMenuItemProperties.HIGHLIGHTED)) {
            ViewHighlighter.turnOnHighlight(button, new HighlightParams(HighlightShape.CIRCLE));
        } else {
            ViewHighlighter.turnOffHighlight(button);
        }

        // Menu items may be hidden by command line flags before they get to this point.
        button.setVisibility(View.VISIBLE);
    }
}
