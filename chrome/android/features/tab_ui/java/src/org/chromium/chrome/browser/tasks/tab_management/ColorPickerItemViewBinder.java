// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.COLOR_ID;
import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.ON_CLICK_LISTENER;

import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.ColorInt;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A binder class for color items on the color picker view. */
public class ColorPickerItemViewBinder {
    private static final int OUTER_LAYER = 0;
    private static final int SELECTION_LAYER = 1;
    private static final int INNER_LAYER = 2;
    // TODO(crbug.com/1517346): Replace this value when color schemes are ready.
    private static final @TabGroupColorId int SELECTION_BG_COLOR = TabGroupColorId.GREY;

    static View create(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.color_picker_item, parent, false);
    }

    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == COLOR_ID) {
            setColorOnColorIcon(model, view);
        } else if (propertyKey == ON_CLICK_LISTENER) {
            view.setOnClickListener((v) -> model.get(ON_CLICK_LISTENER).run());
        } else if (propertyKey == IS_SELECTED) {
            refreshColorIconOnSelection(model, view);
        }
    }

    private static void setColorOnColorIcon(PropertyModel model, View view) {
        @TabGroupColorId int colorId = model.get(COLOR_ID);

        // Update the color icon with the indicated color id.
        ImageView colorIcon = view.findViewById(R.id.color_picker_icon);
        LayerDrawable layerDrawable = (LayerDrawable) colorIcon.getBackground();
        ((GradientDrawable) layerDrawable.getDrawable(OUTER_LAYER))
                .setColor(getColorScheme(colorId));
        ((GradientDrawable) layerDrawable.getDrawable(SELECTION_LAYER))
                .setColor(getColorScheme(SELECTION_BG_COLOR));
        ((GradientDrawable) layerDrawable.getDrawable(INNER_LAYER))
                .setColor(getColorScheme(colorId));

        // Refresh the color item view.
        colorIcon.invalidate();
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

    // TODO(crbug.com/1517346): Replace temp colors with proper color palette, and add accessibility
    // strings for each view.
    private static @ColorInt int getColorScheme(@TabGroupColorId int colorId) {
        switch (colorId) {
            case TabGroupColorId.GREY:
                return Color.GRAY;
            case TabGroupColorId.BLUE:
                return Color.BLUE;
            case TabGroupColorId.RED:
                return Color.RED;
            case TabGroupColorId.YELLOW:
                return Color.YELLOW;
            case TabGroupColorId.GREEN:
                return Color.GREEN;
            case TabGroupColorId.PINK:
                return Color.MAGENTA;
            case TabGroupColorId.PURPLE:
                return Color.MAGENTA;
            case TabGroupColorId.CYAN:
                return Color.BLUE;
            case TabGroupColorId.ORANGE:
                return Color.RED;
            default:
                return Color.TRANSPARENT;
        }
    }
}
