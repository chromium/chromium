// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import java.util.List;

/** LinearLayout for the {@link ColorPickerCoordinator} component. */
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
}
