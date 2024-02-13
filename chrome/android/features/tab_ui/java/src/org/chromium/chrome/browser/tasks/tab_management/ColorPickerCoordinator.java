// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.FrameLayout;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/** Coordinator for the color picker interface. */
public class ColorPickerCoordinator implements ColorPicker {
    private final ColorPickerContainer mContainerView;
    private final ColorPickerMediator mMediator;

    /**
     * Coordinator for the color picker interface.
     *
     * @param context The current context.
     * @param colors The list of color ids corresponding to the color items in this color picker.
     * @param colorPickerLayout The layout resource to be inflated for this color picker.
     * @param colorPickerType The {@link ColorPickerType} associated with this color picker.
     * @param isIncognito Whether the current tab model is in incognito mode.
     */
    public ColorPickerCoordinator(
            @NonNull Context context,
            @NonNull List<Integer> colors,
            @NonNull @LayoutRes int colorPickerLayout,
            @NonNull @ColorPickerType int colorPickerType,
            @NonNull boolean isIncognito) {
        mContainerView =
                (ColorPickerContainer)
                        LayoutInflater.from(context).inflate(colorPickerLayout, /* root= */ null);

        List<PropertyModel> colorItems = new ArrayList<>();
        List<FrameLayout> colorViews = new ArrayList<>();

        for (int color : colors) {
            FrameLayout view = (FrameLayout) ColorPickerItemViewBinder.createItemView(context);
            colorViews.add(view);

            PropertyModel model =
                    ColorPickerItemProperties.create(
                            /* color= */ color,
                            /* colorPickerType= */ colorPickerType,
                            /* isIncognito= */ isIncognito,
                            /* onClickListener= */ () -> {
                                setSelectedColorItem(color);
                            },
                            /* isSelected= */ false);
            colorItems.add(model);
            PropertyModelChangeProcessor.create(model, view, ColorPickerItemViewBinder::bind);
        }

        // Set all color item views on the parent container view.
        mContainerView.setColorViews(colorViews);

        mMediator = new ColorPickerMediator(colorItems);
    }

    /** Returns the container view hosting the color picker component */
    @Override
    public ColorPickerContainer getContainerView() {
        return mContainerView;
    }

    /**
     * Set the currently selected color item. Only one item can be selected at a time and there is
     * no default selection. If a default selection is desired it must be selected here.
     *
     * @param selectedColor The id of the color to be selected.
     */
    @Override
    public void setSelectedColorItem(int selectedColor) {
        mMediator.setSelectedColorItem(selectedColor);
    }

    /** Retrieve the currently selected color supplier in the color picker. */
    @Override
    public ObservableSupplier<Integer> getSelectedColorSupplier() {
        return mMediator.getSelectedColorSupplier();
    }
}
