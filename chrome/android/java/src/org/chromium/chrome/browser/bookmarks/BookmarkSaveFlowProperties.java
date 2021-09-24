// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Hosts the properties for the bookmarks save flow. */
public class BookmarkSaveFlowProperties {
    public static final WritableObjectPropertyKey<View.OnClickListener> EDIT_ONCLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener>
            FOLDER_SELECT_ONCLICK_LISTENER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Drawable> TITLE_START_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> TITLE_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Drawable> FOLDER_SELECT_START_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> FOLDER_SELECT_TEXT =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<CharSequence> SUBTITLE_TEXT =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_PROPERTIES = {EDIT_ONCLICK_LISTENER,
            FOLDER_SELECT_ONCLICK_LISTENER, TITLE_START_ICON, TITLE_TEXT, FOLDER_SELECT_START_ICON,
            FOLDER_SELECT_TEXT, SUBTITLE_TEXT};
}
