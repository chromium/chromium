// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/** Coordinator for the color picker interface. */
public class ColorPickerCoordinator implements ColorPicker {
    private final ColorPickerContainer mContainerView;
    private final ColorPickerMediator mMediator;

    @IntDef({
        ColorPickerLayoutType.DYNAMIC,
        ColorPickerLayoutType.SINGLE_ROW,
        ColorPickerLayoutType.DOUBLE_ROW,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ColorPickerLayoutType {
        int DYNAMIC = 0;
        int SINGLE_ROW = 1;
        int DOUBLE_ROW = 2;
    }

    /**
     * Coordinator for the color picker interface.
     *
     * @param context The current context.
     * @param colors The list of color ids corresponding to the color items in this color picker.
     * @param colorPickerView The view used for the color picker container.
     * @param colorPickerType The {@link ColorPickerType} associated with this color picker.
     * @param isIncognito Whether the current tab model is in incognito mode.
     * @param layoutType The {@ColorPickerLayoutType} that the component will be arranged as.
     * @param onColorItemClicked The runnable for performing an action on each color click event.
     */
    public ColorPickerCoordinator(
            @NonNull Context context,
            @NonNull List<Integer> colors,
            @NonNull View colorPickerView,
            @ColorPickerType int colorPickerType,
            boolean isIncognito,
            @ColorPickerLayoutType int layoutType,
            @Nullable Runnable onColorItemClicked) {
        mContainerView = (ColorPickerContainer) colorPickerView;

        mContainerView.setColorPickerLayoutType(layoutType);

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

                                // Perform the runnable after the color has been selected.
                                if (onColorItemClicked != null) {
                                    onColorItemClicked.run();
                                }
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
