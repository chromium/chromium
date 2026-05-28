// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabListViewBinderUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the Vertical Tab List item rows. */
@NullMarked
class TabVerticalViewBinder {
    private static final float ROTATION_COLLAPSED = 0f;
    private static final float ROTATION_EXPANDED = 180f;

    // Public Entry-Point Binders

    /**
     * Binds PropertyModel properties of a standard tab item to the row's ViewGroup elements.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the standard tab row item.
     * @param propertyKey the specific property key to bind, or null to bind all properties.
     */
    public static void bindTab(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {

        bindCommonProperties(model, view, propertyKey);

        if (TabProperties.TITLE == propertyKey) {
            TextView titleView = view.findViewById(R.id.tab_title);
            titleView.setText(model.get(TabProperties.TITLE));
        } else if (TabProperties.IS_SELECTED == propertyKey
                || TabProperties.IS_INCOGNITO == propertyKey) {
            updateRegularColors(model, view);
        } else if (TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey) {
            @Nullable TabActionButtonData data = model.get(TabProperties.TAB_ACTION_BUTTON_DATA);
            @Nullable View actionButton = view.findViewById(R.id.action_button);
            if (actionButton != null) {
                TabListViewBinderUtils.bindActionButton(model, actionButton, data);
            }
        } else if (TabProperties.TAB_GROUP_ID == propertyKey) {
            updateChildRowPadding(model, view);
        } else if (TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER == propertyKey) {
            TabListViewBinderUtils.updateContentDescription(model, view);
        }
        // TODO(crbug.com/509226293): Add MEDIA_INDICATOR to TabProperties.ALL_KEYS_VERTICAL_TAB
        // and implement binder logic to display/update playing/muted audio icons.
    }

    /**
     * Binds PropertyModel properties of a compact, icon-only pinned tab row to the view elements.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the pinned tab row item.
     * @param propertyKey the specific property key to bind, or null to bind all properties.
     */
    public static void bindPinnedTab(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        bindCommonProperties(model, view, propertyKey);

        if (TabProperties.TITLE == propertyKey) {
            view.setContentDescription(model.get(TabProperties.TITLE));
        } else if (TabProperties.IS_SELECTED == propertyKey
                || TabProperties.IS_INCOGNITO == propertyKey) {
            updatePinnedColors(model, view);
        }
    }

    /**
     * Binds properties of a tab group header row item to its ViewGroup elements.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the tab group header row item.
     * @param propertyKey the specific property key to bind.
     */
    public static void bindTabGroupHeader(
            PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        bindCommonProperties(model, view, propertyKey);

        if (TabProperties.TITLE == propertyKey) {
            TextView titleView = view.findViewById(R.id.group_title);
            titleView.setText(model.get(TabProperties.TITLE));
        } else if (TabProperties.TAB_GROUP_CARD_COLOR == propertyKey
                || TabProperties.IS_INCOGNITO == propertyKey) {
            updateGroupHeaderColors(model, view);
        } else if (TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER == propertyKey) {
            TabListViewBinderUtils.updateContentDescription(model, view);
            // TODO(crbug.com/509226293): Override the default ACTION_CLICK action label
            // to announce "expand" or "collapse" dynamically based on the
            // TabProperties.IS_COLLAPSED
            // state once child rows are implemented. Also confirm and resolve the conflicting
            // "Expand" prefix in the main content description string for active/expanded groups.
        } else if (TabProperties.IS_COLLAPSED == propertyKey) {
            updateChevronRotation(model, view);
        }
    }

    // Common Property Binding Helpers

    /**
     * Binds common property keys shared by all tab row views, preventing duplicate routing logic.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the tab row item.
     * @param propertyKey the specific property key to bind.
     */
    private static void bindCommonProperties(
            PropertyModel model, ViewGroup view, PropertyKey propertyKey) {
        if (TabProperties.FAVICON_FETCHER == propertyKey) {
            updateFavicon(model, view);
        } else if (TabProperties.TAB_CLICK_LISTENER == propertyKey) {
            TabListViewBinderUtils.setNullableClickListener(
                    model.get(TabProperties.TAB_CLICK_LISTENER), view, model);
        } else if (TabProperties.TAB_LONG_CLICK_LISTENER == propertyKey) {
            TabListViewBinderUtils.setNullableLongClickListener(
                    model.get(TabProperties.TAB_LONG_CLICK_LISTENER), view, model);
        } else if (TabProperties.TAB_CONTEXT_CLICK_LISTENER == propertyKey) {
            TabListViewBinderUtils.setNullableContextClickListener(
                    model.get(TabProperties.TAB_CONTEXT_CLICK_LISTENER), view, model);
        }
    }

    private static void updateFavicon(PropertyModel model, ViewGroup view) {
        @Nullable ImageView faviconView = view.findViewById(R.id.tab_favicon);
        if (faviconView != null) {
            TabListViewBinderUtils.updateFavicon(model, faviconView);
        }
    }

    // Row-Specific Layout Color Binder Helpers

    private static void updateRegularColors(PropertyModel model, ViewGroup view) {
        boolean isSelected = model.get(TabProperties.IS_SELECTED);
        boolean isIncognito = model.get(TabProperties.IS_INCOGNITO);
        Context context = view.getContext();
        view.setSelected(isSelected);

        @Nullable Drawable bg = view.getBackground();
        if (bg != null) {
            bg.mutate();
            ViewCompat.setBackgroundTintList(
                    view, getBackgroundTintList(context, isSelected, isIncognito));
        }

        TextView titleView = view.findViewById(R.id.tab_title);
        titleView.setTextColor(getTextColor(context, isSelected, isIncognito));

        @Nullable ImageView actionButton = view.findViewById(R.id.action_button);
        if (actionButton != null) {
            ImageViewCompat.setImageTintList(
                    actionButton, getActionButtonTintList(context, isSelected, isIncognito));
            actionButton.setVisibility(isSelected ? View.VISIBLE : View.INVISIBLE);
        }

        updateFavicon(model, view);
        setupCloseButtonHoverListener(model, view);
    }

    /**
     * Updates the background tint and website favicon specifically for a pinned tab row view.
     * Clears background tints when unselected, to allow the solid XML container drawable to render.
     *
     * @param model the model containing the tab properties.
     * @param view the root ViewGroup representing the pinned tab row item.
     */
    private static void updatePinnedColors(PropertyModel model, ViewGroup view) {
        boolean isSelected = model.get(TabProperties.IS_SELECTED);
        boolean isIncognito = model.get(TabProperties.IS_INCOGNITO);
        Context context = view.getContext();
        view.setSelected(isSelected);

        @Nullable Drawable bg = view.getBackground();
        if (bg != null) {
            bg.mutate();
            ColorStateList tintList =
                    isSelected
                            ? getBackgroundTintList(context, /* isSelected= */ true, isIncognito)
                            : null;
            ViewCompat.setBackgroundTintList(view, tintList);
        }
        updateFavicon(model, view);
    }

    /**
     * Updates the background tint color specifically for the tab group header row view, dynamically
     * resolving the group color ID using TabGroupColorPickerUtils.
     *
     * @param model the model containing the tab group properties.
     * @param view the root ViewGroup representing the tab group header row item.
     */
    private static void updateGroupHeaderColors(PropertyModel model, ViewGroup view) {
        @Nullable Integer colorId = model.get(TabProperties.TAB_GROUP_CARD_COLOR);
        boolean isIncognito = model.get(TabProperties.IS_INCOGNITO);
        Context context = view.getContext();

        @Nullable Drawable bg = view.getBackground();
        if (bg != null && colorId != null) {
            bg.mutate();
            int color =
                    TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                            context, colorId, isIncognito);
            ViewCompat.setBackgroundTintList(view, ColorStateList.valueOf(color));

            @ColorInt
            int textColor = ContextCompat.getColor(context, R.color.default_text_color_light);
            @ColorInt
            int iconColor = ContextCompat.getColor(context, R.color.default_icon_color_light);

            TextView titleView = view.findViewById(R.id.group_title);
            if (titleView != null) {
                titleView.setTextColor(textColor);
            }

            @Nullable ImageView expandChevron = view.findViewById(R.id.expand_chevron);
            if (expandChevron != null) {
                ImageViewCompat.setImageTintList(expandChevron, ColorStateList.valueOf(iconColor));
            }
        }
    }

    // Row-Specific Layout Geometry & Rotation Helpers

    private static void updateChevronRotation(PropertyModel model, ViewGroup view) {
        boolean isCollapsed = model.get(TabProperties.IS_COLLAPSED);
        @Nullable ImageView expandChevron = view.findViewById(R.id.expand_chevron);
        if (expandChevron != null) {
            // TODO(crbug.com/509226293): Animate the rotation once child tab
            // expansion transitions are implemented.
            expandChevron.setRotation(isCollapsed ? ROTATION_COLLAPSED : ROTATION_EXPANDED);
        }
    }

    private static void updateChildRowPadding(PropertyModel model, View view) {
        boolean isInGroup = model.get(TabProperties.TAB_GROUP_ID) != null;
        int marginStart =
                isInGroup
                        ? view.getResources()
                                .getDimensionPixelSize(R.dimen.vertical_tab_child_nesting_margin)
                        : 0;
        if (view.getLayoutParams() instanceof ViewGroup.MarginLayoutParams params) {
            if (params.getMarginStart() != marginStart) {
                params.setMarginStart(marginStart);
                view.setLayoutParams(params);
            }
        }
    }

    // Theme & Color Utility Methods

    private static ColorStateList getBackgroundTintList(
            Context context, boolean isSelected, boolean isIncognito) {
        if (isSelected) {
            int color =
                    isIncognito
                            ? ContextCompat.getColor(
                                    context, R.color.incognito_tab_bg_selected_color)
                            : SemanticColorUtils.getColorSurface(context);
            return ColorStateList.valueOf(color);
        }
        return ColorStateList.valueOf(Color.TRANSPARENT);
    }

    private static @ColorInt int getTextColor(
            Context context, boolean isSelected, boolean isIncognito) {
        if (isSelected) {
            return isIncognito
                    ? ContextCompat.getColor(context, R.color.incognito_tab_title_selected_color)
                    : SemanticColorUtils.getColorOnSurface(context);
        } else {
            return isIncognito
                    ? ContextCompat.getColor(context, R.color.incognito_tab_title_color)
                    : SemanticColorUtils.getDefaultTextColorSecondary(context);
        }
    }

    private static ColorStateList getActionButtonTintList(
            Context context, boolean isSelected, boolean isIncognito) {
        int color =
                isIncognito
                        ? ContextCompat.getColor(
                                context,
                                isSelected
                                        ? R.color.incognito_tab_title_selected_color
                                        : R.color.incognito_tab_title_color)
                        : (isSelected
                                ? SemanticColorUtils.getDefaultIconColor(context)
                                : SemanticColorUtils.getDefaultIconColorSecondary(context));
        return ColorStateList.valueOf(color);
    }

    // Gesture & Interaction Layout Helpers

    private static void setupCloseButtonHoverListener(PropertyModel model, ViewGroup view) {
        @Nullable ImageView actionButton = view.findViewById(R.id.action_button);
        if (actionButton == null) return;

        view.setOnHoverListener(
                (rowView, motionEvent) -> {
                    boolean isSelected = model.get(TabProperties.IS_SELECTED);
                    if (isSelected) {
                        actionButton.setVisibility(View.VISIBLE);
                        return false;
                    }

                    switch (motionEvent.getAction()) {
                        case MotionEvent.ACTION_HOVER_ENTER:
                            actionButton.setVisibility(View.VISIBLE);
                            break;
                        case MotionEvent.ACTION_HOVER_EXIT:
                            actionButton.setVisibility(View.INVISIBLE);
                            break;
                    }
                    return false;
                });
    }
}
