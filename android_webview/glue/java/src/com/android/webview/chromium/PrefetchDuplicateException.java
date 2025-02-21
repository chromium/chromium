// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import org.chromium.android_webview.common.Lifetime;

/** A variation of {@link PrefetchException} for duplicate prefetch requests. */
@Lifetime.Temporary
public class PrefetchDuplicateException extends PrefetchException {
    public PrefetchDuplicateException(String message) {
        super(message);
    }
}
