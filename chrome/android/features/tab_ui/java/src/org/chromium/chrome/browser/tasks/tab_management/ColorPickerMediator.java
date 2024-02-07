// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** Contains the logic to set the state of the model and react to color change clicks. */
public class ColorPickerMediator {
    private final @NonNull List<PropertyModel> mColorItems;
    private final @NonNull List<Integer> mColors;
    private final @NonNull @ColorPickerType int mColorPickerType;
    private ObservableSupplierImpl<Integer> mSelectedColorSupplier = new ObservableSupplierImpl<>();

    public ColorPickerMediator(List<Integer> colors, @ColorPickerType int colorPickerType) {
        this(colors, new ArrayList<>(), colorPickerType);
    }

    protected ColorPickerMediator(
            List<Integer> colors,
            List<PropertyModel> colorItems,
            @ColorPickerType int colorPickerType) {
        mColors = colors;
        mColorItems = colorItems;
        mColorPickerType = colorPickerType;
    }

    /**
     * Sets the color items and creates the corresponding models for the color item entries on the
     * color picker UI. The default color is selected from all colors in the list.
     *
     * @param containerView The parent container for all color items inflated by this component.
     */
    public void setColorListItems(ColorPickerContainer containerView) {
        List<FrameLayout> colorViews = new ArrayList<>();
        Context context = containerView.getContext();

        for (int color : mColors) {
            FrameLayout view = (FrameLayout) ColorPickerItemViewBinder.createItemView(context);
            colorViews.add(view);

            PropertyModel model =
                    ColorPickerItemProperties.create(
                            /* color= */ color,
                            /* isSelected= */ false,
                            /* colorPickerType= */ mColorPickerType,
                            /* onClickListener= */ () -> {
                                setSelectedColorItem(color);
                            });
            mColorItems.add(model);

            PropertyModelChangeProcessor.create(model, view, ColorPickerItemViewBinder::bind);
        }

        // Set all color item views on the parent container view.
        containerView.setColorViews(colorViews);
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
