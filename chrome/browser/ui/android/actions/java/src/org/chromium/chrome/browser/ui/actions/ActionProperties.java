// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for action buttons. */
@NullMarked
public class ActionProperties {
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<String> CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Callback<View>> ON_PRESS_CALLBACK =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Callback<View>> ON_LONG_PRESS_CALLBACK =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ICON, CONTENT_DESCRIPTION, ON_PRESS_CALLBACK, ON_LONG_PRESS_CALLBACK,
            };
}
