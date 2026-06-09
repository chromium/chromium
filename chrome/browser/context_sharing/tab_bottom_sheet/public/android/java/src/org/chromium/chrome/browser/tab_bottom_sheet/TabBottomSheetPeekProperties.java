// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

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

    public static final WritableIntPropertyKey TITLE_TEXT_APPEARANCE_ID =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey DESCRIPTION_TEXT_ID = new WritableIntPropertyKey();

    public static final WritableIntPropertyKey DESCRIPTION_VISIBILITY =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey ACTION_BUTTON_TEXT_ID = new WritableIntPropertyKey();

    public static final WritableIntPropertyKey ACTION_BUTTON_VISIBILITY =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey ACTION_BUTTON_ICON_ID = new WritableIntPropertyKey();

    public static final WritableIntPropertyKey ACTION_BUTTON_BACKGROUND_TINT_ID =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey ACTION_BUTTON_ICON_TINT_ID =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey ACTION_BUTTON_HORIZONTAL_PADDING_ID =
            new WritableIntPropertyKey();

    public static final WritableIntPropertyKey ACTION_BUTTON_CONTENT_DESCRIPTION_ID =
            new WritableIntPropertyKey();

    public static final ReadableObjectPropertyKey<Runnable> ON_ACTION_BUTTON_CLICKED =
            new ReadableObjectPropertyKey<>();

    public static final ReadableObjectPropertyKey<Runnable> ON_CLOSE_CLICKED =
            new ReadableObjectPropertyKey<>();

    public static final ReadableObjectPropertyKey<Runnable> ON_PEEK_VIEW_CLICKED =
            new ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        TITLE_TEXT,
        TITLE_TEXT_APPEARANCE_ID,
        DESCRIPTION_TEXT_ID,
        DESCRIPTION_VISIBILITY,
        ACTION_BUTTON_TEXT_ID,
        ACTION_BUTTON_VISIBILITY,
        ACTION_BUTTON_ICON_ID,
        ACTION_BUTTON_BACKGROUND_TINT_ID,
        ACTION_BUTTON_ICON_TINT_ID,
        ACTION_BUTTON_HORIZONTAL_PADDING_ID,
        ACTION_BUTTON_CONTENT_DESCRIPTION_ID,
        ON_ACTION_BUTTON_CLICKED,
        ON_CLOSE_CLICKED,
        ON_PEEK_VIEW_CLICKED
    };
}
