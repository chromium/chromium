// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.drawable.Drawable;
import android.widget.CompoundButton;
import android.widget.ImageView.ScaleType;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the desktop android bookmark popup. */
@NullMarked
public class BookmarkPopupProperties {
    /** Listener for click events on the top-right close button. */
    public static final WritableObjectPropertyKey<Runnable> CLOSE_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Listener for click events on the done action button. */
    public static final WritableObjectPropertyKey<Runnable> DONE_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The folder name of the bookmark. */
    public static final WritableObjectPropertyKey<String> FOLDER_NAME =
            new WritableObjectPropertyKey<>();

    /** Listener for click events on the folder selector row. */
    public static final WritableObjectPropertyKey<Runnable> FOLDER_ROW_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The header text of the popup (e.g. "Bookmark added"). */
    public static final WritableObjectPropertyKey<String> HEADER_TEXT =
            new WritableObjectPropertyKey<>();

    /** The salient image or favicon for the bookmarked URL. */
    public static final WritableObjectPropertyKey<Drawable> IMAGE_DRAWABLE =
            new WritableObjectPropertyKey<>();

    /** Scale type to apply to the image drawable. This varies between salient image and favicon. */
    public static final WritableObjectPropertyKey<ScaleType> IMAGE_SCALE_TYPE =
            new WritableObjectPropertyKey<>();

    /** Listener for click events on the remove action button. */
    public static final WritableObjectPropertyKey<Runnable> REMOVE_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The title of the bookmark. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    /** Listener for title text edits. */
    public static final WritableObjectPropertyKey<Callback<String>> TITLE_CHANGED_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Visibility of the price tracking section. */
    public static final WritableBooleanPropertyKey PRICE_TRACKING_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Enabled state of the price tracking section (notably the switch). */
    public static final WritableBooleanPropertyKey PRICE_TRACKING_ENABLED =
            new WritableBooleanPropertyKey();

    /** Checked state of the price tracking switch. */
    public static final WritableBooleanPropertyKey PRICE_TRACKING_SWITCH_CHECKED =
            new WritableBooleanPropertyKey();

    /** Listener for price tracking switch toggles. */
    public static final WritableObjectPropertyKey<CompoundButton.OnCheckedChangeListener>
            PRICE_TRACKING_SWITCH_LISTENER = new WritableObjectPropertyKey<>();

    /** List of all keys defined for this property model. */
    public static final PropertyKey[] ALL_KEYS = {
        CLOSE_BUTTON_CLICK_LISTENER,
        DONE_BUTTON_CLICK_LISTENER,
        FOLDER_NAME,
        FOLDER_ROW_CLICK_LISTENER,
        HEADER_TEXT,
        IMAGE_DRAWABLE,
        IMAGE_SCALE_TYPE,
        PRICE_TRACKING_ENABLED,
        PRICE_TRACKING_SWITCH_CHECKED,
        PRICE_TRACKING_SWITCH_LISTENER,
        PRICE_TRACKING_VISIBLE,
        REMOVE_BUTTON_CLICK_LISTENER,
        TITLE,
        TITLE_CHANGED_LISTENER
    };
}
