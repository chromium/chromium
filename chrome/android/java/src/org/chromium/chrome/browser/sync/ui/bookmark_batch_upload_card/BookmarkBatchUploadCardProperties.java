// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui.bookmark_batch_upload_card;

import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class BookmarkBatchUploadCardProperties {
    static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>("description_text");
    static final PropertyModel.ReadableIntPropertyKey BUTTON_TEXT =
            new ReadableIntPropertyKey("button_text");
    static final WritableObjectPropertyKey<OnClickListener> On_CLICK_LISTENER =
            new WritableObjectPropertyKey("on_click_listener");
    static final PropertyModel.ReadableObjectPropertyKey<Drawable> ICON =
            new PropertyModel.ReadableObjectPropertyKey<>("icon");

    public static final PropertyKey[] ALL_KEYS = {
        DESCRIPTION_TEXT, BUTTON_TEXT, On_CLICK_LISTENER, ICON
    };
}
