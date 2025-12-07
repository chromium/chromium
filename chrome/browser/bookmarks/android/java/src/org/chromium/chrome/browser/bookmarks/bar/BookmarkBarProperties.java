// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.content.res.ColorStateList;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the bookmark bar which provides users with bookmark access from top chrome. */
@NullMarked
class BookmarkBarProperties {

    /** The callback to notify of bookmark bar overflow button click events. */
    public static final WritableObjectPropertyKey<Runnable> OVERFLOW_BUTTON_CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();

    /** The visibility of the bookmark bar overflow button. */
    public static final WritableIntPropertyKey OVERFLOW_BUTTON_VISIBILITY =
            new WritableIntPropertyKey();

    /** The top margin to use during bookmark bar layout. */
    public static final WritableIntPropertyKey TOP_MARGIN = new WritableIntPropertyKey();

    /** The visibility of the bookmark bar. */
    public static final WritableIntPropertyKey VISIBILITY = new WritableIntPropertyKey();

    /** The tint for the overflow button. */
    public static final WritableObjectPropertyKey<ColorStateList> OVERFLOW_BUTTON_TINT_LIST =
            new WritableObjectPropertyKey<>();

    /** The color for the vertical divider. */
    public static final WritableIntPropertyKey DIVIDER_COLOR = new WritableIntPropertyKey();

    /** The color for the bottom hairline. */
    public static final WritableIntPropertyKey HAIRLINE_COLOR = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                OVERFLOW_BUTTON_CLICK_CALLBACK,
                OVERFLOW_BUTTON_VISIBILITY,
                TOP_MARGIN,
                VISIBILITY,
                OVERFLOW_BUTTON_TINT_LIST,
                DIVIDER_COLOR,
                HAIRLINE_COLOR
            };
}
