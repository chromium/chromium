// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;


import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Contains the logic to set the state of the model and react to color change clicks. */
public class ColorPickerMediator {
    private final @NonNull List<PropertyModel> mColorItems;
    private ObservableSupplierImpl<Integer> mSelectedColorSupplier = new ObservableSupplierImpl<>();

    /**
     * Contains the logic to set the state of the model and react to color change clicks. This
     * constructor is used with the coordinator to facilitate color picker backend logic.
     *
     * @param colorItems The list of property models representing the color items in this color
     *     picker.
     */
    public ColorPickerMediator(List<PropertyModel> colorItems) {
        mColorItems = colorItems;
    }

    void setSelectedColorItem(int selectedColor) {
        for (PropertyModel model : mColorItems) {
            boolean isSelected = selectedColor == model.get(ColorPickerItemProperties.COLOR_ID);
            model.set(ColorPickerItemProperties.IS_SELECTED, isSelected);
        }

        mSelectedColorSupplier.set(selectedColor);
    }

    ObservableSupplier<Integer> getSelectedColorSupplier() {
        return mSelectedColorSupplier;
    }
}
