// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;

import java.util.List;

/** Coordinator for the color picker interface. */
public class ColorPickerCoordinator implements ColorPicker {
    private final ColorPickerContainer mContainerView;
    private final ColorPickerMediator mMediator;

    /** Coordinator for the color picker interface. */
    public ColorPickerCoordinator(
            @NonNull Context context,
            @NonNull List<Integer> colors,
            @NonNull @LayoutRes int colorPickerLayout,
            @NonNull @ColorPickerType int colorPickerType) {
        mContainerView =
                (ColorPickerContainer)
                        LayoutInflater.from(context).inflate(colorPickerLayout, /* root= */ null);

        mMediator = new ColorPickerMediator(colors, colorPickerType);
        mMediator.setColorListItems(mContainerView);
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
