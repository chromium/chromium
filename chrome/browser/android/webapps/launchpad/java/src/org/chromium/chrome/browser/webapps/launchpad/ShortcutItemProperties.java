// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.graphics.Bitmap;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class ShortcutItemProperties {
    private ShortcutItemProperties() {}

    public static final WritableObjectPropertyKey<String> NAME = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> LAUNCH_URL =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Bitmap> SHORTCUT_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey HIDE_ICON = new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<View.OnClickListener> ON_CLICK =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
            NAME, LAUNCH_URL, SHORTCUT_ICON, HIDE_ICON, ON_CLICK};
}
