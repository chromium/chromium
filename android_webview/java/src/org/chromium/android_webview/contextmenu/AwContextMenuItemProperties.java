// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

class AwContextMenuItemProperties {
    public static final WritableObjectPropertyKey<CharSequence> TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey MENU_ID = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {TEXT, MENU_ID};
}
