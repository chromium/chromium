// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.COLOR_ID;
import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.COLOR_PICKER_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiThemeProvider.getColorPickerDialogBackgroundColor;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.RectF;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewOverlay;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;

import com.google.android.material.button.MaterialButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.theme.ThemeModuleUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.drawable.BorderDrawable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.AttrUtils;

/** A binder class for color items on the color picker view. */
@NullMarked
public class ColorPickerItemViewBinder {
    // When a color picker item is not selected, a full circle of color is shown, and the complex
    // layer drawable that is used here is not needed. But when an item becomes selected, the desire
    // is to a show a UI element mimicking a radio button. This consists of a full circle of color
    // with a ring cut out inside that is transparent to the background color. This implementation
    // does not use transparency, and instead employs three concentric inset circles on top of each
    // other. First the original large full color circle (OUTER_LAYER) is shown, followed by making
    // the middle inset circle visible and matching the background color (SELECTION_LAYER). Lastly
    // the smallest inset circle is ensured to match the outer full circle's color (INNER_LAYER).
    public static final int OUTER_LAYER = 0;
    public static final int SELECTION_LAYER = 1;
    public static final int INNER_LAYER = 2;

    static View createItemView(Context context) {
        int layoutToInflate =
                isAndroidThemeModuleEnabled()
                        ? R.layout.color_picker_icon_button_layout
                        : R.layout.color_picker_item;

        return LayoutInflater.from(context).inflate(layoutToInflate, /* root= */ null, false);
    }

    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == COLOR_ID) {
            setColorOnColorIcon(model, view);
        } else if (propertyKey == ON_CLICK_LISTENER) {
            view.findViewById(R.id.color_picker_icon)
                    .setOnClickListener((v) -> model.get(ON_CLICK_LISTENER).run());
        } else if (propertyKey == IS_SELECTED) {
            refreshColorIconOnSelection(model, view);
            setAccessibilityContent(view, model.get(IS_SELECTED), model.get(COLOR_ID));
        }
    }

    private static void setColorOnColorIcon(PropertyModel model, View view) {
        Context context = view.getContext();
        @ColorPickerType int colorPickerType = model.get(COLOR_PICKER_TYPE);
        boolean isIncognito = model.get(IS_INCOGNITO);
        int colorId = model.get(COLOR_ID);

        final @ColorInt int color = getColor(context, colorPickerType, colorId, isIncognito);

        // Update the color icon with the indicated color id.
        if (isAndroidThemeModuleEnabled()) {
            MaterialButton colorIcon = view.findViewById(R.id.color_picker_icon);
            colorIcon.setBackgroundTintList(ColorStateList.valueOf(color));
            colorIcon.setRippleColor(
                    TabGroupColorPickerUtils.buildTabGroupColorPickerRippleColorStateList(
                            context, isIncognito));
        } else {
            final @ColorInt int selectionBackgroundColor =
                    getColorPickerDialogBackgroundColor(context, isIncognito);

            ImageView colorIcon = view.findViewById(R.id.color_picker_icon);
            LayerDrawable layerDrawable = (LayerDrawable) colorIcon.getBackground();
            ((GradientDrawable) layerDrawable.getDrawable(OUTER_LAYER)).setColor(color);
            ((GradientDrawable) layerDrawable.getDrawable(SELECTION_LAYER))
                    .setColor(selectionBackgroundColor);
            ((GradientDrawable) layerDrawable.getDrawable(INNER_LAYER)).setColor(color);
        }
    }

    private static void refreshColorIconOnSelection(PropertyModel model, View view) {
        final View colorIcon = view.findViewById(R.id.color_picker_icon);

        if (isAndroidThemeModuleEnabled()) {
            var button = (MaterialButton) colorIcon;
            button.setChecked(model.get(IS_SELECTED));
            button.setEnabled(!model.get(IS_SELECTED));

            ViewOverlay overlay = colorIcon.getOverlay();

            if (model.get(IS_SELECTED)) {
                BorderDrawable borderDrawable = getBorderDrawable(model, button);
                overlay.add(borderDrawable);
            } else {
                overlay.clear();
            }
        } else {
            LayerDrawable layerDrawable = (LayerDrawable) colorIcon.getBackground();

            // Toggle the selected layer opaqueness based on the user click action.
            int alpha = model.get(IS_SELECTED) ? 0xFF : 0;
            layerDrawable.getDrawable(SELECTION_LAYER).setAlpha(alpha);
        }

        // Refresh the color item view.
        colorIcon.invalidate();
    }

    private static BorderDrawable getBorderDrawable(PropertyModel model, MaterialButton button) {
        Resources res = button.getResources();

        // Background drawable size.
        int sizePx =
                AttrUtils.getDimensionPixelSize(button.getContext(), R.attr.minInteractTargetSize);
        // Inset of the background from the button's bounds.
        int insetPx = button.getInsetTop();
        // ShapeAppearanceModel for the checked state.
        var shapeAppearanceModel =
                button.getShapeAppearance()
                        .getShapeForState(
                                new int[] {
                                    android.R.attr.state_checkable, android.R.attr.state_checked
                                });
        // Corner size of the checked (rounded rect) background. The reason we pass a RectF to
        // #getCornerSize is because the corner size is calculated based on the bounds of the
        // drawable, e.g. it could be a percentage.
        float cornerSize =
                shapeAppearanceModel
                        .getTopLeftCornerSize()
                        .getCornerSize(new RectF(0, 0, sizePx, sizePx));
        int borderWidthPx = res.getDimensionPixelSize(R.dimen.color_picker_button_stroke_width);
        int borderOuterWidthPx =
                res.getDimensionPixelSize(R.dimen.color_picker_button_stroke_outer_width);
        // The border's corner size needs to be smaller to align with the outer corner radius.
        int borderCornerSizePx = Math.round(cornerSize) - borderWidthPx - borderOuterWidthPx;
        // We want to leave an outline around the button.
        int borderInsetPx = insetPx + borderWidthPx + borderOuterWidthPx;

        BorderDrawable borderDrawable =
                new BorderDrawable(
                        borderWidthPx,
                        borderInsetPx,
                        getColorPickerDialogBackgroundColor(
                                button.getContext(), model.get(IS_INCOGNITO)),
                        borderCornerSizePx);

        // Set the bounds of the drawable to match the color button view.
        borderDrawable.setBounds(0, 0, sizePx, sizePx);
        return borderDrawable;
    }

    private static @ColorInt int getColor(
            Context context,
            @ColorPickerType int colorPickerType,
            int colorListIndex,
            boolean isIncognito) {
        if (colorPickerType == ColorPickerType.TAB_GROUP) {
            return TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                    context, colorListIndex, isIncognito);
        } else {
            return Color.TRANSPARENT;
        }
    }

    private static void setAccessibilityContent(View view, boolean isSelected, int colorId) {
        View colorIcon = view.findViewById(R.id.color_picker_icon);
        Resources res = view.getContext().getResources();

        final @StringRes int colorDescRes =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                        colorId);
        final @StringRes int selectedFormatDescRes =
                isSelected
                        ? R.string
                                .accessibility_tab_group_color_picker_color_item_selected_description
                        : R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description;
        String colorDesc = res.getString(colorDescRes);
        String contentDescription = res.getString(selectedFormatDescRes, colorDesc);
        colorIcon.setContentDescription(contentDescription);
    }

    private static boolean isAndroidThemeModuleEnabled() {
        return ThemeModuleUtils.isEnabled();
    }
}
