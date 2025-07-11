// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds the reader mode bottom sheet properties to the view. */
@NullMarked
public class ReaderModeBottomSheetViewBinder {
    /**
     * Binds PropertyKeys to View properties for the reader mode bottom sheet.
     *
     * @param model The PropertyModel for the View.
     * @param view The View to be bound.
     * @param key The key that's being bound.
     */
    public static void bind(PropertyModel model, ReaderModeBottomSheetView view, PropertyKey key) {
        if (key == ReaderModeBottomSheetProperties.CONTENT_VIEW) {
            ViewGroup controlsContainer = view.findViewById(R.id.controls_container);
            controlsContainer.removeAllViews();
            controlsContainer.addView(model.get(ReaderModeBottomSheetProperties.CONTENT_VIEW));
        }
    }
}
