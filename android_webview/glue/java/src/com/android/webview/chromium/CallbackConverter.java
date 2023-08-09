// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.ValueCallback;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.Callback;

/**
 * Utility class for converting a {@link android.webkit.ValueCallback} into a
 * {@link org.chromium.base.Callback}.
 */
@Lifetime.Singleton
public final class CallbackConverter {
    public static <T> Callback<T> fromValueCallback(final ValueCallback<T> valueCallback) {
        return valueCallback == null ? null : result -> valueCallback.onReceiveValue(result);
    }

    // Do not instantiate this class
    private CallbackConverter() {}
}
