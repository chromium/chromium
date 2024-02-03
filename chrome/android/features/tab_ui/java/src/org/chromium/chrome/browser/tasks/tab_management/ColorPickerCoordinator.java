// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;

import java.util.List;

/** Coordinator for the color picker interface. */
public class ColorPickerCoordinator {
    /**
     * The delegate responsible for handling UI-specific arrangements on each color picker
     * implementation.
     */
    public interface Delegate {
        /** Retrieve the UI component used for inflating the color picker. */
        int getColorPickerUIComponent();
    }

    private final ColorPickerContainer mContainerView;
    private final ColorPickerMediator mMediator;

    /** Coordinator for the color picker interface. */
    public ColorPickerCoordinator(Context context, List<Integer> colors, Delegate delegate) {
        mContainerView =
                (ColorPickerContainer)
                        LayoutInflater.from(context)
                                .inflate(delegate.getColorPickerUIComponent(), /* root= */ null);

        mMediator = new ColorPickerMediator(colors);
        mMediator.setColorListItems(mContainerView);
    }

    /** Returns the container view hosting the color picker component */
    public ColorPickerContainer getContainerView() {
        return mContainerView;
    }

    /**
     * Set the currently selected color item. Only one item can be selected at a time and there is
     * no default selection. If a default selection is desired it must be selected here.
     *
     * @param selectedColor The id of the color to be selected.
     */
    public void setSelectedColorItem(int selectedColor) {
        mMediator.setSelectedColorItem(selectedColor);
    }

    /** Retrieve the currently selected color in the color picker. */
    public int getSelectedColor() {
        return mMediator.getSelectedColor();
    }
}
