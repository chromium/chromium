// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import org.chromium.android_webview.AwPrefetchStartResultCode;
import org.chromium.android_webview.common.Lifetime;

@Lifetime.Temporary
public class PrefetchOperationResult {

    public final @PrefetchOperationStatusCode int statusCode;

    public PrefetchOperationResult(@PrefetchOperationStatusCode int statusCode) {
        this.statusCode = statusCode;
    }

    public static PrefetchOperationResult fromStartResultCode(
            @AwPrefetchStartResultCode int startResultCode) {
        int statusCode;
        switch (startResultCode) {
            case AwPrefetchStartResultCode.SUCCESS:
                statusCode = PrefetchOperationStatusCode.SUCCESS;
                break;
            case AwPrefetchStartResultCode.FAILURE:
                statusCode = PrefetchOperationStatusCode.FAILURE;
                break;
            default:
                throw new IllegalArgumentException("Invalid prefetch start result code");
        }
        return new PrefetchOperationResult(statusCode);
    }
}
