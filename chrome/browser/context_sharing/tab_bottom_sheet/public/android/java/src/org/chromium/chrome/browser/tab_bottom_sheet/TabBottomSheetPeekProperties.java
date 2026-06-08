// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.res.ColorStateList;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the Tab Bottom Sheet Peek View. */
@NullMarked
public class TabBottomSheetPeekProperties {

    public static final WritableObjectPropertyKey<String> TITLE_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey TITLE_TEXT_APPEARANCE = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<String> DESCRIPTION_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey DESCRIPTION_VISIBILITY =
            new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<String> ACTION_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey ACTION_BUTTON_VISIBILITY =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey ACTION_BUTTON_ICON = new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<ColorStateList> ACTION_BUTTON_BACKGROUND_TINT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<ColorStateList> ACTION_BUTTON_ICON_TINT =
            new WritableObjectPropertyKey<>();

    public static final WritableIntPropertyKey ACTION_BUTTON_HORIZONTAL_PADDING =
            new WritableIntPropertyKey();

    public static final WritableObjectPropertyKey<String> ACTION_BUTTON_CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();

    public static final ReadableObjectPropertyKey<Runnable> ON_ACTION_BUTTON_CLICKED =
            new ReadableObjectPropertyKey<>();

    public static final ReadableObjectPropertyKey<Runnable> ON_CLOSE_CLICKED =
            new ReadableObjectPropertyKey<>();

    public static final ReadableObjectPropertyKey<Runnable> ON_PEEK_VIEW_CLICKED =
            new ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        TITLE_TEXT,
        TITLE_TEXT_APPEARANCE,
        DESCRIPTION_TEXT,
        DESCRIPTION_VISIBILITY,
        ACTION_BUTTON_TEXT,
        ACTION_BUTTON_VISIBILITY,
        ACTION_BUTTON_ICON,
        ACTION_BUTTON_BACKGROUND_TINT,
        ACTION_BUTTON_ICON_TINT,
        ACTION_BUTTON_HORIZONTAL_PADDING,
        ACTION_BUTTON_CONTENT_DESCRIPTION,
        ON_ACTION_BUTTON_CLICKED,
        ON_CLOSE_CLICKED,
        ON_PEEK_VIEW_CLICKED
    };
}
