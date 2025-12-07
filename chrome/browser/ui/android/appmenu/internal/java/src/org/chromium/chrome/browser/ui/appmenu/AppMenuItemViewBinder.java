// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.annotation.SuppressLint;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.widget.ImageViewCompat;

import com.google.android.material.button.MaterialButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.theme.ThemeModuleUtils;
import org.chromium.chrome.browser.ui.appmenu.internal.R;
import org.chromium.components.browser_ui.util.motion.OnPeripheralClickListener;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.ChromeImageView;

/** The binder to bind the app menu {@link PropertyModel} with the view. */
@NullMarked
class AppMenuItemViewBinder {
    /** IDs of all of the buttons in icon_row_menu_item.xml. */
    private static final int[] BUTTON_IDS = {
        R.id.button_one, R.id.button_two, R.id.button_three, R.id.button_four, R.id.button_five
    };

    /** IDs of all of the buttons wrappers in icon_row_menu_item.xml. */
    private static final int[] BUTTON_WRAPPER_IDS = {
        R.id.button_wrapper_one,
        R.id.button_wrapper_two,
        R.id.button_wrapper_three,
        R.id.button_wrapper_four,
        R.id.button_wrapper_five
    };

    public static void bindStandardItem(PropertyModel model, View view, PropertyKey key) {
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
            setIcon(view, model);
        } else if (key == AppMenuItemProperties.CLICK_HANDLER) {
            view.setOnTouchListener(
                    new OnPeripheralClickListener(
                            view,
                            triggeringMotion ->
                                    model.get(AppMenuItemProperties.CLICK_HANDLER)
                                            .onItemClick(model, triggeringMotion)));
            view.setOnClickListener(
                    v -> model.get(AppMenuItemProperties.CLICK_HANDLER).onItemClick(model));
        } else if (key == AppMenuItemProperties.HOVER_LISTENER) {
            view.setOnHoverListener(model.get(AppMenuItemProperties.HOVER_LISTENER));
        } else if (key == AppMenuItemProperties.HAS_HOVER_BACKGROUND) {
            view.setHovered(model.get(AppMenuItemProperties.HAS_HOVER_BACKGROUND));
        } else if (key == AppMenuItemProperties.KEY_LISTENER) {
            view.setOnKeyListener(model.get(AppMenuItemProperties.KEY_LISTENER));
        }
    }

    public static void bindTitleButtonItem(PropertyModel model, View view, PropertyKey key) {
        bindStandardItem(model, view, key);

        if (key == AppMenuItemProperties.ADDITIONAL_ICONS) {
            ModelList subList = model.get(AppMenuItemProperties.ADDITIONAL_ICONS);
            assert subList.size() == 1;

            AppMenuClickHandler appMenuClickHandler =
                    model.get(AppMenuItemProperties.CLICK_HANDLER);

            View titleContainer = view.findViewById(R.id.menu_item_container);
            View actionIconContainer = view.findViewById(R.id.action_icon_container);
            AppMenuItemIcon checkbox = view.findViewById(R.id.checkbox);
            ChromeImageButton button = view.findViewById(R.id.button);
            PropertyModel buttonModel = subList.get(0).model;
            boolean hasAction = true;

            if (buttonModel.get(AppMenuItemProperties.CHECKABLE)) {
                // Display a checkbox for the MenuItem.
                button.setVisibility(View.GONE);
                checkbox.setVisibility(View.VISIBLE);
                checkbox.setChecked(buttonModel.get(AppMenuItemProperties.CHECKED));
                ImageViewCompat.setImageTintList(
                        checkbox,
                        AppCompatResources.getColorStateList(
                                checkbox.getContext(), R.color.selection_control_button_tint_list));
                setupMenuButton(checkbox, buttonModel, appMenuClickHandler);
            } else if (buttonModel.get(AppMenuItemProperties.ICON) != null) {
                // Display an icon alongside the MenuItem.
                checkbox.setVisibility(View.GONE);
                button.setVisibility(View.VISIBLE);
                if (!buttonModel.get(AppMenuItemProperties.ENABLED)) {
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
                hasAction = false;
            }

            if (hasAction) {
                actionIconContainer.setVisibility(View.VISIBLE);
                titleContainer.setPaddingRelative(
                        titleContainer.getPaddingStart(),
                        titleContainer.getPaddingTop(),
                        0,
                        titleContainer.getPaddingBottom());
            } else {
                // Display just the label of the MenuItem.
                actionIconContainer.setVisibility(View.GONE);

                int[] attrs = {android.R.attr.paddingEnd};
                @SuppressLint("ResourceType")
                TypedArray ta =
                        view.getContext().obtainStyledAttributes(R.style.AppMenuItem, attrs);
                int paddingEnd = ta.getDimensionPixelSize(0, 0);
                titleContainer.setPaddingRelative(
                        titleContainer.getPaddingStart(),
                        titleContainer.getPaddingTop(),
                        paddingEnd,
                        titleContainer.getPaddingBottom());
                ta.recycle();
            }
        }
    }

    public static void bindIconRowItem(PropertyModel model, View view, PropertyKey key) {
        if (key == AppMenuItemProperties.ADDITIONAL_ICONS) {
            // Obtain from the current theme a typed array containing all the attributes.
            TypedArray typedArray =
                    view.getContext().getTheme().obtainStyledAttributes(R.styleable.AppMenuIconRow);
            int drawableResId =
                    typedArray.getResourceId(
                            R.styleable.AppMenuIconRow_overflowMenuActionBarBgDrawable, 0);

            typedArray.recycle();

            // Set the background by resolving it from the current theme.
            if (drawableResId != 0) {
                view.setBackgroundResource(drawableResId);
            }

            ModelList iconList = model.get(AppMenuItemProperties.ADDITIONAL_ICONS);

            AppMenuClickHandler appMenuClickHandler =
                    model.get(AppMenuItemProperties.CLICK_HANDLER);

            int numItems = iconList.size();
            MaterialButton[] buttons = new MaterialButton[numItems];
            for (int i = 0; i < 5; i++) {
                View buttonWrapper = view.findViewById(BUTTON_WRAPPER_IDS[i]);
                MaterialButton button = buttonWrapper.findViewById(BUTTON_IDS[i]);
                if (i < numItems) {
                    buttons[i] = button;
                    buttonWrapper.setVisibility(View.VISIBLE);
                    button.setVisibility(View.VISIBLE);
                    Drawable icon = iconList.get(i).model.get(AppMenuItemProperties.ICON);
                    icon = DrawableCompat.wrap(icon.mutate());
                    button.setIcon(icon);

                    boolean isChecked = iconList.get(i).model.get(AppMenuItemProperties.CHECKED);

                    if (!ThemeModuleUtils.isEnabled()) {
                        @ColorRes
                        int resId =
                                isChecked
                                        ? R.color.default_icon_color_accent1_tint_list
                                        : R.color.default_icon_color_tint_list;
                        button.setIconTint(
                                AppCompatResources.getColorStateList(button.getContext(), resId));
                    } else {
                        button.setCheckable(true);
                        button.setChecked(isChecked);
                    }
                    setupMenuButton(button, iconList.get(i).model, appMenuClickHandler);
                } else {
                    buttonWrapper.setVisibility(View.GONE);
                    button.setVisibility(View.GONE);
                    button.setIcon(null);
                }
            }

            boolean isMenuIconAtStart = model.get(AppMenuItemProperties.MENU_ICON_AT_START);
            view.setTag(
                    R.id.menu_item_enter_anim_id,
                    AppMenuUtil.buildIconItemEnterAnimator(buttons, isMenuIconAtStart));

            view.setEnabled(false);
        }
    }

    public static void bindItemWithSubmenu(PropertyModel model, View view, PropertyKey key) {
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
            setIcon(view, model);
        } else if (key == AppMenuItemWithSubmenuProperties.CLICK_LISTENER) {
            view.setOnClickListener(model.get(AppMenuItemWithSubmenuProperties.CLICK_LISTENER));
        } else if (key == AppMenuItemProperties.HOVER_LISTENER) {
            view.setOnHoverListener(model.get(AppMenuItemProperties.HOVER_LISTENER));
        } else if (key == AppMenuItemProperties.HAS_HOVER_BACKGROUND) {
            view.setHovered(model.get(AppMenuItemProperties.HAS_HOVER_BACKGROUND));
        } else if (key == AppMenuItemProperties.KEY_LISTENER) {
            view.setOnKeyListener(model.get(AppMenuItemProperties.KEY_LISTENER));
        }
    }

    public static void bindSubmenuHeader(PropertyModel model, View view, PropertyKey key) {
        if (key == AppMenuItemProperties.MENU_ITEM_ID) {
            int id = model.get(AppMenuItemProperties.MENU_ITEM_ID);
            view.setId(id);
        } else if (key == AppMenuItemProperties.TITLE) {
            ((TextView) view.findViewById(R.id.menu_item_text))
                    .setText(model.get(AppMenuItemProperties.TITLE));
        } else if (key == AppMenuItemProperties.ENABLED) {
            boolean enabled = model.get(AppMenuItemProperties.ENABLED);
            view.setEnabled(enabled);
        } else if (key == AppMenuItemWithSubmenuProperties.CLICK_LISTENER) {
            view.setOnClickListener(model.get(AppMenuItemWithSubmenuProperties.CLICK_LISTENER));
        } else if (key == AppMenuItemProperties.KEY_LISTENER) {
            view.setOnKeyListener(model.get(AppMenuItemProperties.KEY_LISTENER));
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

    private static void setIcon(View view, final PropertyModel model) {
        Drawable icon = model.get(AppMenuItemProperties.ICON);
        ChromeImageView imageView = view.findViewById(R.id.menu_item_icon);

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

        View buttonWrapper = (View) button.getParent();
        if (model.get(AppMenuItemProperties.HIGHLIGHTED)) {
            ViewHighlighter.turnOnHighlight(
                    buttonWrapper, new HighlightParams(HighlightShape.CIRCLE));
        } else {
            ViewHighlighter.turnOffHighlight(buttonWrapper);
        }

        // Menu items may be hidden by command line flags before they get to this point.
        button.setVisibility(View.VISIBLE);
    }
}
