// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.COLOR_ID;
import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.COLOR_PICKER_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.ON_CLICK_LISTENER;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;
import androidx.core.content.ContextCompat;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A binder class for color items on the color picker view. */
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
        return LayoutInflater.from(context)
                .inflate(R.layout.color_picker_item, /* root= */ null, false);
    }

    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == COLOR_ID) {
            setColorOnColorIcon(model, view);
        } else if (propertyKey == ON_CLICK_LISTENER) {
            view.setOnClickListener((v) -> model.get(ON_CLICK_LISTENER).run());
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
        final @ColorInt int selectionBackgroundColor =
                isIncognito
                        ? ContextCompat.getColor(
                                context, R.color.tab_group_color_picker_selection_bg_incognito)
                        : SemanticColorUtils.getDialogBgColor(context);

        // Update the color icon with the indicated color id.
        ImageView colorIcon = view.findViewById(R.id.color_picker_icon);
        LayerDrawable layerDrawable = (LayerDrawable) colorIcon.getBackground();
        ((GradientDrawable) layerDrawable.getDrawable(OUTER_LAYER)).setColor(color);
        ((GradientDrawable) layerDrawable.getDrawable(SELECTION_LAYER))
                .setColor(selectionBackgroundColor);
        ((GradientDrawable) layerDrawable.getDrawable(INNER_LAYER)).setColor(color);
    }

    private static void refreshColorIconOnSelection(PropertyModel model, View view) {
        ImageView colorIcon = view.findViewById(R.id.color_picker_icon);
        LayerDrawable layerDrawable = (LayerDrawable) colorIcon.getBackground();

        // Toggle the selected layer opaqueness based on the user click action.
        int alpha = model.get(IS_SELECTED) ? 0xFF : 0;
        layerDrawable.getDrawable(SELECTION_LAYER).setAlpha(alpha);

        // Refresh the color item view.
        colorIcon.invalidate();
    }

    private static @ColorInt int getColor(
            Context context,
            @ColorPickerType int colorPickerType,
            int colorListIndex,
            boolean isIncognito) {
        if (colorPickerType == ColorPickerType.TAB_GROUP) {
            return ColorPickerUtils.getTabGroupColorPickerItemColor(
                    context, colorListIndex, isIncognito);
        } else {
            return Color.TRANSPARENT;
        }
    }

    private static void setAccessibilityContent(View view, boolean isSelected, int colorId) {
        ImageView colorIcon = view.findViewById(R.id.color_picker_icon);
        Resources res = view.getContext().getResources();

        final @StringRes int colorDescRes =
                ColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(colorId);
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
}
