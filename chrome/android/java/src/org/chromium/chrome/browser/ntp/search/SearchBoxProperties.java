// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;


import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.text.TextWatcher;
import android.view.View.OnClickListener;
import android.view.View.OnDragListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties required to build the fake search box on new tab page. */
@NullMarked
interface SearchBoxProperties {
    WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();
    WritableBooleanPropertyKey VISIBILITY = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey VOICE_SEARCH_VISIBILITY = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<Drawable> VOICE_SEARCH_DRAWABLE = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<ColorStateList> VOICE_SEARCH_COLOR_STATE_LIST =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<OnClickListener> VOICE_SEARCH_CLICK_CALLBACK =
            new WritableObjectPropertyKey<>();

    WritableBooleanPropertyKey COMPOSEPLATE_BUTTON_VISIBILITY = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<OnClickListener> COMPOSEPLATE_BUTTON_CLICK_CALLBACK =
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
    WritableIntPropertyKey SEARCH_BOX_START_PADDING = new WritableIntPropertyKey();
    WritableIntPropertyKey SEARCH_BOX_TEXT_STYLE_RES_ID = new WritableIntPropertyKey();
    WritableBooleanPropertyKey ENABLE_SEARCH_BOX_EDIT_TEXT = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<String> SEARCH_BOX_HINT_TEXT = new WritableObjectPropertyKey<>();
    WritableBooleanPropertyKey APPLY_WHITE_BACKGROUND_WITH_SHADOW =
            new WritableBooleanPropertyKey();

    WritableIntPropertyKey COMPOSEPLATE_BUTTON_ICON_RAW_RES_ID = new WritableIntPropertyKey();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ALPHA,
                VISIBILITY,
                VOICE_SEARCH_VISIBILITY,
                VOICE_SEARCH_DRAWABLE,
                VOICE_SEARCH_COLOR_STATE_LIST,
                VOICE_SEARCH_CLICK_CALLBACK,
                COMPOSEPLATE_BUTTON_VISIBILITY,
                COMPOSEPLATE_BUTTON_CLICK_CALLBACK,
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
                SEARCH_BOX_START_PADDING,
                SEARCH_BOX_TEXT_STYLE_RES_ID,
                ENABLE_SEARCH_BOX_EDIT_TEXT,
                SEARCH_BOX_HINT_TEXT,
                APPLY_WHITE_BACKGROUND_WITH_SHADOW,
                COMPOSEPLATE_BUTTON_ICON_RAW_RES_ID,
            };
}
