// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import android.graphics.Bitmap;
import android.view.View;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties for the "edit URL" suggestion in the omnibox suggestions. */
class EditUrlSuggestionProperties {
    /** The key for the title displayed by the suggestion item. */
    public static final WritableObjectPropertyKey<String> TITLE_TEXT =
            new WritableObjectPropertyKey<>();

    /** The key for the URL displayed for the suggestion item. */
    public static final WritableObjectPropertyKey<String> URL_TEXT =
            new WritableObjectPropertyKey<>();

    /** The key for the click listener that all of the buttons use. */
    public static final WritableObjectPropertyKey<View.OnClickListener> BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The key for the click listener that the text of the suggestion uses. */
    public static final WritableObjectPropertyKey<View.OnClickListener> TEXT_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<Bitmap> SITE_FAVICON =
            new WritableObjectPropertyKey<>();

    private static final PropertyKey[] ALL_UNIQUE_KEYS = new PropertyKey[] {
            TITLE_TEXT, URL_TEXT, BUTTON_CLICK_LISTENER, TEXT_CLICK_LISTENER, SITE_FAVICON};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
