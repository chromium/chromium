// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class RevampedContextMenuShareItemProperties extends RevampedContextMenuItemProperties {
    public static final WritableObjectPropertyKey<Drawable> IMAGE =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<CharSequence> CONTENT_DESC =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(RevampedContextMenuItemProperties.ALL_KEYS,
                    new PropertyKey[] {IMAGE, CONTENT_DESC, CLICK_LISTENER});
}
