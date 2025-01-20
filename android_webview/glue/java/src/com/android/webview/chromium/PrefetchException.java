// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import org.chromium.android_webview.common.Lifetime;

/** A generic class for the prefetch exception. */
@Lifetime.Temporary
public class PrefetchException extends Exception {

    public PrefetchException(String message) {
        super(message);
    }

    public PrefetchException(Throwable cause) {
        super(cause);
    }
}
