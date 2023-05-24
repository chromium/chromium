// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.CompoundButton;

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
    public static final WritableObjectPropertyKey<Integer> NOTIFICATION_SWITCH_START_ICON_RES =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> NOTIFICATION_SWITCH_SUBTITLE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> NOTIFICATION_SWITCH_TITLE =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey NOTIFICATION_SWITCH_TOGGLED =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<CompoundButton.OnCheckedChangeListener>
            NOTIFICATION_SWITCH_TOGGLE_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey NOTIFICATION_SWITCH_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey NOTIFICATION_UI_ENABLED =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<CharSequence> SUBTITLE_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> TITLE_TEXT =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {EDIT_ONCLICK_LISTENER, FOLDER_SELECT_ICON,
            FOLDER_SELECT_ICON_ENABLED, FOLDER_SELECT_ONCLICK_LISTENER, NOTIFICATION_SWITCH_VISIBLE,
            NOTIFICATION_SWITCH_START_ICON_RES, NOTIFICATION_SWITCH_TITLE,
            NOTIFICATION_SWITCH_SUBTITLE, NOTIFICATION_SWITCH_TOGGLED,
            NOTIFICATION_SWITCH_TOGGLE_LISTENER, NOTIFICATION_UI_ENABLED, SUBTITLE_TEXT,
            TITLE_TEXT};
}
