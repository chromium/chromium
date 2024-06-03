// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import android.graphics.Bitmap;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties of the minimized card. */
public class MinimizedCardProperties {
    /** The title of the minimized webpage. */
    public static final WritableObjectPropertyKey<String> TITLE =
            new WritableObjectPropertyKey<>("TITLE");

    /** The URL of the minimized webpage. */
    public static final WritableObjectPropertyKey<String> URL =
            new WritableObjectPropertyKey<>("URL");

    /** The favicon of the minimized webpage. */
    public static final WritableObjectPropertyKey<Bitmap> FAVICON =
            new WritableObjectPropertyKey<>("FAVICON");

    public static final PropertyKey[] ALL_KEYS = {TITLE, URL, FAVICON};
}
