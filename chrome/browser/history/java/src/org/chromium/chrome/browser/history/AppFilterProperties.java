// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Property model properties for app filter sheet UI. */
class AppFilterProperties {
    public static final ReadableObjectPropertyKey<String> ID = new ReadableObjectPropertyKey();
    public static final ReadableObjectPropertyKey<Drawable> ICON = new ReadableObjectPropertyKey();
    public static final ReadableObjectPropertyKey<CharSequence> LABEL =
            new ReadableObjectPropertyKey();
    public static final WritableObjectPropertyKey<View.OnClickListener> CLICK_LISTENER =
            new WritableObjectPropertyKey();
    public static final WritableBooleanPropertyKey SELECTED = new WritableBooleanPropertyKey();
    public static final ReadableObjectPropertyKey<View.OnClickListener> CLOSE_BUTTON_CALLBACK =
            new ReadableObjectPropertyKey();

    /** Property keys for a list item. */
    public static final PropertyKey[] LIST_ITEM_KEYS = {ID, ICON, LABEL, CLICK_LISTENER, SELECTED};

    /** Property keys for the close button. */
    public static final PropertyKey[] CLOSE_BUTTON_KEY = {CLOSE_BUTTON_CALLBACK};
}
