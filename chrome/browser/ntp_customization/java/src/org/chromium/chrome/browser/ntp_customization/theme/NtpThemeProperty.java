// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class NtpThemeProperty {
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            LEARN_MORE_BUTTON_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.WritableObjectPropertyKey<Pair<Drawable, Drawable>>
            LEADING_ICON_FOR_THEME_COLLECTIONS = new PropertyModel.WritableObjectPropertyKey<>();

    // The key manages the selection state for each section: the integer defines the section type,
    // while the boolean indicates whether it is selected.
    public static final PropertyModel.WritableObjectPropertyKey<Pair<Integer, Boolean>>
            IS_SECTION_SELECTED = new PropertyModel.WritableObjectPropertyKey<>();

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

    // The bitmap for the logo in the preview dialog. If null, the default Google logo is used.
    public static final PropertyModel.WritableObjectPropertyKey<Bitmap> LOGO_BITMAP =
            new PropertyModel.WritableObjectPropertyKey<>();

    // The callback to adjust the logo's layout parameters.
    public static final PropertyModel.WritableObjectPropertyKey<int[]> LOGO_PARAMS =
            new PropertyModel.WritableObjectPropertyKey<>();

    // The visibility of the logo view.
    public static final PropertyModel.WritableIntPropertyKey LOGO_VISIBILITY =
            new PropertyModel.WritableIntPropertyKey();

    // The top margin in pixels applied to the layout to avoid overlapping with the status bar and
    // the tool bar.
    public static final PropertyModel.WritableIntPropertyKey TOP_GUIDELINE_BEGIN =
            new PropertyModel.WritableIntPropertyKey();

    // The bottom inset in pixels to ensure interactive buttons clear the system navigation
    // bar.
    public static final PropertyModel.WritableIntPropertyKey BOTTOM_INSETS =
            new PropertyModel.WritableIntPropertyKey();

    // The left, right, and bottom insets to be applied as view padding.
    public static final PropertyModel.WritableObjectPropertyKey<Rect> SIDE_AND_BOTTOM_INSETS =
            new PropertyModel.WritableObjectPropertyKey<>();

    // The width of the search box in pixels.
    public static final PropertyModel.WritableIntPropertyKey SEARCH_BOX_WIDTH =
            new PropertyModel.WritableIntPropertyKey();

    // The height of the search box in pixels.
    public static final PropertyModel.WritableIntPropertyKey SEARCH_BOX_HEIGHT =
            new PropertyModel.WritableIntPropertyKey();

    // The top margin of the search box in pixels.
    public static final PropertyModel.WritableIntPropertyKey SEARCH_BOX_TOP_MARGIN =
            new PropertyModel.WritableIntPropertyKey();

    // The bottom margin of the buttons in pixels.
    public static final PropertyModel.WritableIntPropertyKey BUTTON_BOTTOM_MARGIN =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyKey[] THEME_KEYS =
            new PropertyKey[] {
                LEARN_MORE_BUTTON_CLICK_LISTENER,
                IS_SECTION_SELECTED,
                SECTION_ON_CLICK_LISTENER,
                LEADING_ICON_FOR_THEME_COLLECTIONS
            };

    public static final PropertyKey[] PREVIEW_KEYS =
            new PropertyKey[] {
                BITMAP_FOR_PREVIEW,
                PREVIEW_SAVE_CLICK_LISTENER,
                PREVIEW_CANCEL_CLICK_LISTENER,
                LOGO_BITMAP,
                LOGO_VISIBILITY,
                LOGO_PARAMS,
                TOP_GUIDELINE_BEGIN,
                BOTTOM_INSETS,
                SIDE_AND_BOTTOM_INSETS,
                SEARCH_BOX_WIDTH,
                SEARCH_BOX_HEIGHT,
                SEARCH_BOX_TOP_MARGIN,
                BUTTON_BOTTOM_MARGIN
            };
}
