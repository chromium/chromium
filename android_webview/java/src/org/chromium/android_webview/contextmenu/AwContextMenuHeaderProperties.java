// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the context menu header. This class is public for testing purposes. */
@NullMarked
public class AwContextMenuHeaderProperties {
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Drawable> HEADER_ICON =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {TITLE, HEADER_ICON};
}
