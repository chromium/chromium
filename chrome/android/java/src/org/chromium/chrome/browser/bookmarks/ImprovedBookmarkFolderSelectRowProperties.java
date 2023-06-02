// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Responsible for hosting properties of the improved bookmark folder select view. */
public class ImprovedBookmarkFolderSelectRowProperties {
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    // Sets the background color for the start image, both for the image and folder view.
    public static final WritableIntPropertyKey START_AREA_BACKGROUND_COLOR =
            new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<Drawable> START_ICON_DRAWABLE =
            new WritableObjectPropertyKey<>();
    // Sets the tint color for the start image, both for the image and folder view.
    public static final WritableObjectPropertyKey<ColorStateList> START_ICON_TINT =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Pair<Drawable, Drawable>>
            START_IMAGE_FOLDER_DRAWABLES = new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey FOLDER_CHILD_COUNT = new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey END_ICON_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<View.OnClickListener> ROW_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {TITLE, START_AREA_BACKGROUND_COLOR,
            START_ICON_DRAWABLE, START_ICON_TINT, START_IMAGE_FOLDER_DRAWABLES, FOLDER_CHILD_COUNT,
            END_ICON_VISIBLE, ROW_CLICK_LISTENER};
}
