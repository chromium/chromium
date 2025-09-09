// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import android.graphics.Bitmap;
import android.util.Pair;
import android.view.View;

import androidx.core.view.OnApplyWindowInsetsListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class NtpThemeProperty {
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            LEARN_MORE_BUTTON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<Pair<Integer, Integer>>
            LEADING_ICON_FOR_THEME_COLLECTIONS = new PropertyModel.WritableObjectPropertyKey<>();

    // The key manages the visibility of trailing icon for each section, with the integer
    // representing the section type and the boolean indicating its visibility.
    public static final PropertyModel.WritableObjectPropertyKey<Pair<Integer, Boolean>>
            IS_SECTION_TRAILING_ICON_VISIBLE = new PropertyModel.WritableObjectPropertyKey<>();

    // The key manages the {@link View.OnClickListener} of each section, with the integer
    // representing the section type.
    public static final PropertyModel.WritableObjectPropertyKey<Pair<Integer, View.OnClickListener>>
            SECTION_ON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    // The bitmap to be displayed and cropped in the preview dialog.
    public static final PropertyModel.WritableObjectPropertyKey<Bitmap> BITMAP_FOR_PREVIEW =
            new PropertyModel.WritableObjectPropertyKey<>();

    // The listener for the "Save" button click event in the preview dialog.
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            PREVIEW_SAVE_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    // The listener for the "Cancel" button click event in the preview dialog.
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            PREVIEW_CANCEL_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    // The listener for setting the margin bottom of the "Cancel" and "Save" buttons in the preview
    // dialog.
    public static final PropertyModel.WritableObjectPropertyKey<OnApplyWindowInsetsListener>
            PREVIEW_SET_WINDOW_INSETS_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] THEME_KEYS =
            new PropertyKey[] {
                LEARN_MORE_BUTTON_CLICK_LISTENER,
                IS_SECTION_TRAILING_ICON_VISIBLE,
                SECTION_ON_CLICK_LISTENER,
                LEADING_ICON_FOR_THEME_COLLECTIONS
            };

    public static final PropertyKey[] PREVIEW_KEYS =
            new PropertyKey[] {
                BITMAP_FOR_PREVIEW,
                PREVIEW_SAVE_CLICK_LISTENER,
                PREVIEW_CANCEL_CLICK_LISTENER,
                PREVIEW_SET_WINDOW_INSETS_LISTENER,
            };
}
