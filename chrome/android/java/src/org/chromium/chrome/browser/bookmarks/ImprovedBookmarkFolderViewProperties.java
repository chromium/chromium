// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.Pair;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Responsible for hosting properties of the improved bookmark folder view. */
class ImprovedBookmarkFolderViewProperties {
    static final WritableIntPropertyKey START_AREA_BACKGROUND_COLOR = new WritableIntPropertyKey();
    static final WritableObjectPropertyKey<ColorStateList> START_ICON_TINT =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Drawable> START_ICON_DRAWABLE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Pair<Drawable, Drawable>> START_IMAGE_FOLDER_DRAWABLES =
            new WritableObjectPropertyKey<>();
    static final WritableIntPropertyKey FOLDER_CHILD_COUNT = new WritableIntPropertyKey();

    static final PropertyKey[] ALL_KEYS = {START_AREA_BACKGROUND_COLOR, START_ICON_TINT,
            START_ICON_DRAWABLE, START_IMAGE_FOLDER_DRAWABLES, FOLDER_CHILD_COUNT};
}
