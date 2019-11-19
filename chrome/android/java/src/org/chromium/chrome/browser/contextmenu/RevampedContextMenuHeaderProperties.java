// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.graphics.Bitmap;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class RevampedContextMenuHeaderProperties {
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey TITLE_MAX_LINES = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<CharSequence> URL =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener>
            TITLE_AND_URL_CLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey URL_MAX_LINES = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<Bitmap> IMAGE = new WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey CIRCLE_BG_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {TITLE, TITLE_MAX_LINES, URL,
            TITLE_AND_URL_CLICK_LISTENER, URL_MAX_LINES, IMAGE, CIRCLE_BG_VISIBLE};
}