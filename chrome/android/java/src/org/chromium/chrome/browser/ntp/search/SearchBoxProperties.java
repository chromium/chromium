// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.text.TextWatcher;
import android.view.View.OnClickListener;
import android.view.View.OnDragListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties required to build the fake search box on new tab page. */
interface SearchBoxProperties {
    WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();
    WritableObjectPropertyKey<Drawable> BACKGROUND = new WritableObjectPropertyKey<>();
    WritableBooleanPropertyKey VISIBILITY = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey VOICE_SEARCH_VISIBILITY = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<Drawable> VOICE_SEARCH_DRAWABLE = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<ColorStateList> VOICE_SEARCH_COLOR_STATE_LIST =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<OnClickListener> VOICE_SEARCH_CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();
    WritableBooleanPropertyKey LENS_VISIBILITY = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<OnClickListener> LENS_CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<String> SEARCH_TEXT = new WritableObjectPropertyKey<>();
    WritableBooleanPropertyKey SEARCH_HINT_VISIBILITY = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<OnClickListener> SEARCH_BOX_CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<OnDragListener> SEARCH_BOX_DRAG_CALLBACK =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<TextWatcher> SEARCH_BOX_TEXT_WATCHER =
            new WritableObjectPropertyKey<>();
    WritableIntPropertyKey SEARCH_BOX_HEIGHT = new WritableIntPropertyKey();
    WritableIntPropertyKey SEARCH_BOX_TOP_MARGIN = new WritableIntPropertyKey();
    WritableIntPropertyKey SEARCH_BOX_END_PADDING = new WritableIntPropertyKey();
    WritableFloatPropertyKey SEARCH_TEXT_TRANSLATION_X = new WritableFloatPropertyKey();
    WritableFloatPropertyKey SEARCH_BOX_TEXT_SIZE = new WritableFloatPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ALPHA,
                BACKGROUND,
                VISIBILITY,
                VOICE_SEARCH_VISIBILITY,
                VOICE_SEARCH_DRAWABLE,
                VOICE_SEARCH_COLOR_STATE_LIST,
                VOICE_SEARCH_CLICK_CALLBACK,
                LENS_VISIBILITY,
                LENS_CLICK_CALLBACK,
                SEARCH_TEXT,
                SEARCH_HINT_VISIBILITY,
                SEARCH_BOX_CLICK_CALLBACK,
                SEARCH_BOX_DRAG_CALLBACK,
                SEARCH_BOX_TEXT_WATCHER,
                SEARCH_BOX_HEIGHT,
                SEARCH_BOX_TOP_MARGIN,
                SEARCH_BOX_END_PADDING,
                SEARCH_TEXT_TRANSLATION_X,
                SEARCH_BOX_TEXT_SIZE
            };
}
