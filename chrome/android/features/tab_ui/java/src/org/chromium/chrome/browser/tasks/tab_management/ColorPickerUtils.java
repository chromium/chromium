// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Resources;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Helper class to handle color picker related utilities. */
@NullMarked
public class ColorPickerUtils {
    /**
     * Get the accessibility string corresponding to the respective color item. This function should
     * only be used for retrieving items from the tab group color picker.
     *
     * @param colorId The color id corresponding to the color item in the color picker.
     */
    public static @StringRes int getTabGroupColorPickerItemColorAccessibilityString(
            @TabGroupColorId int colorId) {
        switch (colorId) {
            case TabGroupColorId.GREY:
                return R.string.accessibility_tab_group_color_picker_color_item_grey;
            case TabGroupColorId.BLUE:
                return R.string.accessibility_tab_group_color_picker_color_item_blue;
            case TabGroupColorId.RED:
                return R.string.accessibility_tab_group_color_picker_color_item_red;
            case TabGroupColorId.YELLOW:
                return R.string.accessibility_tab_group_color_picker_color_item_yellow;
            case TabGroupColorId.GREEN:
                return R.string.accessibility_tab_group_color_picker_color_item_green;
            case TabGroupColorId.PINK:
                return R.string.accessibility_tab_group_color_picker_color_item_pink;
            case TabGroupColorId.PURPLE:
                return R.string.accessibility_tab_group_color_picker_color_item_purple;
            case TabGroupColorId.CYAN:
                return R.string.accessibility_tab_group_color_picker_color_item_cyan;
            case TabGroupColorId.ORANGE:
                return R.string.accessibility_tab_group_color_picker_color_item_orange;
            default:
                assert false : "Invalid tab group color id " + colorId;
                return Resources.ID_NULL;
        }
    }
}
