// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.addtohomescreen;

import android.graphics.Bitmap;
import android.util.Pair;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the properties that an add-to-homescreen {@link PropertyModel} can have.
 */
class AddToHomescreenProperties {
    static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyModel.WritableObjectPropertyKey<String> URL =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyModel.WritableObjectPropertyKey<String> CATEGORIES =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyModel.WritableObjectPropertyKey<Pair<Bitmap, Boolean>> ICON =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyModel.WritableIntPropertyKey TYPE =
            new PropertyModel.WritableIntPropertyKey();
    static final PropertyModel.WritableBooleanPropertyKey CAN_SUBMIT =
            new PropertyModel.WritableBooleanPropertyKey();
    static final PropertyModel.WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyModel.WritableObjectPropertyKey<String> NATIVE_INSTALL_BUTTON_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyModel.WritableFloatPropertyKey NATIVE_APP_RATING =
            new PropertyModel.WritableFloatPropertyKey();

    static final PropertyKey[] ALL_KEYS = {TITLE, URL, CATEGORIES, DESCRIPTION, ICON, TYPE,
            CAN_SUBMIT, CLICK_LISTENER, NATIVE_INSTALL_BUTTON_TEXT, NATIVE_APP_RATING};
}
