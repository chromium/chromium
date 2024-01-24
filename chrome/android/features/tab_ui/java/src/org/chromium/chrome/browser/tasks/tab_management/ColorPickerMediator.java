// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.tasks.tab_management.ColorPickerItemProperties.ColorItemType;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Contains the logic to set the state of the model and react to color change clicks. */
public class ColorPickerMediator {
    private ModelList mColorItems;
    private @TabGroupColorId int mSelectedColor;
    private List<Integer> mColors = new ArrayList<>();

    public ColorPickerMediator(ModelList colorItems) {
        mColorItems = colorItems;

        // The color ids used here can be found in {@link TabGroupColorId}. Note that it is assumed
        // the id list is contiguous from 0 to size-1.
        for (int i = 0; i < TabGroupColorId.class.getDeclaredFields().length; i++) {
            mColors.add(i);
        }
    }

    /**
     * Sets the color items and creates the corresponding models for the color item entries on the
     * color picker UI. The default color is selected from all colors in the list.
     */
    public void setColorListItems() {
        // The default selected color, which is the 0th item in the list.
        int defaultSelectedColor = TabGroupColorId.GREY;

        for (int color : mColors) {
            PropertyModel model =
                    ColorPickerItemProperties.create(
                            /* color= */ color,
                            /* isSelected= */ false,
                            /* onClickListener= */ () -> {
                                setSelectedColorItem(color);
                            });
            mColorItems.add(new ListItem(ColorItemType.DEFAULT_ITEM, model));
        }

        setSelectedColorItem(defaultSelectedColor);
    }

    private void setSelectedColorItem(int selectedColor) {
        for (ListItem item : mColorItems) {
            boolean isSelected =
                    selectedColor == item.model.get(ColorPickerItemProperties.COLOR_ID);
            item.model.set(ColorPickerItemProperties.IS_SELECTED, isSelected);

            if (isSelected) {
                mSelectedColor = selectedColor;
            }
        }
    }

    /**
     * Retrieve the currently selected color in the color picker.
     *
     * @return the currently selected color id.
     */
    int getSelectedColor() {
        return mSelectedColor;
    }
}
