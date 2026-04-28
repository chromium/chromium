// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.color_picker;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.RadioGroup;

import androidx.annotation.IntDef;
import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/** Coordinator for the color picker interface. */
@NullMarked
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
            Context context,
            List<Integer> colors,
            View colorPickerView,
            @ColorPickerType int colorPickerType,
            boolean isIncognito,
            @ColorPickerLayoutType int layoutType,
            @Nullable Runnable onColorItemClicked) {
        mContainerView = (ColorPickerContainer) colorPickerView;

        mContainerView.setColorPickerLayoutType(layoutType);

        List<PropertyModel> colorItems = new ArrayList<>();
        List<FrameLayout> colorViews = new ArrayList<>();

        for (int i = 0; i < colors.size(); i++) {
            int color = colors.get(i);
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
                            /* isSelected= */ false,
                            /* itemIndex= */ i);
            colorItems.add(model);
            PropertyModelChangeProcessor.create(model, view, ColorPickerItemViewBinder::bind);
        }

        // Set all color item views on the parent container view.
        mContainerView.setColorViews(colorViews);

        ViewCompat.setAccessibilityDelegate(
                mContainerView,
                new AccessibilityDelegateCompat() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            View host, AccessibilityNodeInfoCompat info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);
                        info.setClassName(RadioGroup.class.getName());
                        info.setCollectionInfo(
                                AccessibilityNodeInfoCompat.CollectionInfoCompat.obtain(
                                        /* rowCount= */ 1,
                                        /* columnCount= */ colors.size(),
                                        /* hierarchical= */ false,
                                        AccessibilityNodeInfoCompat.CollectionInfoCompat
                                                .SELECTION_MODE_SINGLE));
                    }
                });

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
    public MonotonicObservableSupplier<Integer> getSelectedColorSupplier() {
        return mMediator.getSelectedColorSupplier();
    }
}
