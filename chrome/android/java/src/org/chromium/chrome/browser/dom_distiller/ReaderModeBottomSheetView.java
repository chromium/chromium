// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;

/** The view for the reader mode bottom sheet. */
@NullMarked
public class ReaderModeBottomSheetView extends LinearLayout {

    /**
     * @param context The android context.
     * @param attrs The android attributes.
     */
    public ReaderModeBottomSheetView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }
}
