// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

@NullMarked
/* Properties for the composeplate on the NTP. */
interface ComposeplateProperties {
    WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey IS_INCOGNITO_BUTTON_VISIBLE = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<View.OnClickListener> VOICE_SEARCH_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    WritableObjectPropertyKey<View.OnClickListener> LENS_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    WritableObjectPropertyKey<View.OnClickListener> INCOGNITO_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                IS_VISIBLE,
                IS_INCOGNITO_BUTTON_VISIBLE,
                VOICE_SEARCH_CLICK_LISTENER,
                LENS_CLICK_LISTENER,
                INCOGNITO_CLICK_LISTENER
            };
}
