// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.color_picker;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.color_picker.ColorPickerCoordinator.ColorPickerLayoutType;

import java.util.List;

/** LinearLayout for the {@link ColorPickerCoordinator} component. */
@NullMarked
public abstract class ColorPickerContainer extends LinearLayout {
    /** Constructs a new color picker. */
    public ColorPickerContainer(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Stores the color views to be arranged in this color picker component.
     *
     * @param colorViews The color views to be arranged.
     */
    public abstract void setColorViews(List<FrameLayout> colorViews);

    /** Returns the {@link ColorPickerLayoutType} to be used. */
    public abstract @ColorPickerLayoutType int getColorPickerLayoutType();

    /**
     * Stores the requested layout type to be arranged in this component.
     *
     * @param layoutType The {@link ColorPickerLayoutType} to be used.
     */
    public abstract void setColorPickerLayoutType(@ColorPickerLayoutType int layoutType);

    /**
     * @return Width (in px) the {@link ColorPickerContainer} would have in a single-row layout.
     */
    public abstract int getSingleRowWidth();

    /**
     * @return Width (in px) the {@link ColorPickerContainer} would have in a double-row layout.
     */
    public abstract int getDoubleRowWidth();
}
