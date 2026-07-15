// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.drawable.Drawable;
import android.widget.ImageView.ScaleType;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the desktop android bookmark popup. */
@NullMarked
public class BookmarkPopupProperties {
    public static final WritableObjectPropertyKey<String> HEADER_TEXT =
            new WritableObjectPropertyKey<>();
    // The salient image or favicon for the bookmarked URL.
    public static final WritableObjectPropertyKey<Drawable> IMAGE_DRAWABLE =
            new WritableObjectPropertyKey<>();
    // Scale type to apply to the image drawable. This varies between salient image and favicon.
    public static final WritableObjectPropertyKey<ScaleType> IMAGE_SCALE_TYPE =
            new WritableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Runnable> REMOVE_BUTTON_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Runnable> CLOSE_BUTTON_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Runnable> DONE_BUTTON_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Callback<String>> TITLE_CHANGED_LISTENER =
            new ReadableObjectPropertyKey<>();
    public static final ReadableObjectPropertyKey<Runnable> FOLDER_ROW_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>();

    // Bookmark properties.
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> FOLDER_NAME =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        HEADER_TEXT,
        IMAGE_DRAWABLE,
        IMAGE_SCALE_TYPE,
        REMOVE_BUTTON_CLICK_LISTENER,
        CLOSE_BUTTON_CLICK_LISTENER,
        DONE_BUTTON_CLICK_LISTENER,
        TITLE_CHANGED_LISTENER,
        FOLDER_ROW_CLICK_LISTENER,
        TITLE,
        FOLDER_NAME
    };
}
