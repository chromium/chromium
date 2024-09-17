// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import org.chromium.android_webview.common.Lifetime;

@Lifetime.Temporary
public class PrefetchOperationResult {

    public final @PrefetchOperationStatusCode int statusCode;

    public PrefetchOperationResult(@PrefetchOperationStatusCode int statusCode) {
        this.statusCode = statusCode;
    }
}
