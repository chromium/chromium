// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Hosts the properties for the bookmarks save flow. */
public class BookmarkSaveFlowProperties {
    public static final WritableObjectPropertyKey<View.OnClickListener> EDIT_ONCLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Drawable> FOLDER_SELECT_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey FOLDER_SELECT_ICON_ENABLED =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<View.OnClickListener>
            FOLDER_SELECT_ONCLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> SUBTITLE_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> TITLE_TEXT =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_PROPERTIES = {EDIT_ONCLICK_LISTENER, FOLDER_SELECT_ICON,
            FOLDER_SELECT_ICON_ENABLED, FOLDER_SELECT_ONCLICK_LISTENER, SUBTITLE_TEXT, TITLE_TEXT};
}
