// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.android_webview.AwPrefetchCallback;
import org.chromium.android_webview.AwPrefetchCallback.StatusCode;
import org.chromium.android_webview.common.Lifetime;

@Lifetime.Temporary
public class PrefetchOperationResult {

    public final @PrefetchOperationStatusCode int statusCode;
    public final int httpResponseStatusCode;

    public PrefetchOperationResult(@PrefetchOperationStatusCode int statusCode) {
        this.statusCode = statusCode;
        httpResponseStatusCode = 0;
    }

    public PrefetchOperationResult(
            @PrefetchOperationStatusCode int statusCode, int httpResponseStatusCode) {
        this.statusCode = statusCode;
        this.httpResponseStatusCode = httpResponseStatusCode;
    }

    public static PrefetchOperationResult fromPrefetchStatusCode(
            @StatusCode int statusCode, @Nullable Bundle extras) {
        // TODO(crbug.com/372915075) : Implement tests.
        switch (statusCode) {
            case StatusCode.PREFETCH_RESPONSE_COMPLETED:
                return new PrefetchOperationResult(PrefetchOperationStatusCode.SUCCESS);
            case StatusCode.PREFETCH_START_FAILED:
            case StatusCode.PREFETCH_RESPONSE_GENERIC_ERROR:
                return new PrefetchOperationResult(PrefetchOperationStatusCode.FAILURE);
            case StatusCode.PREFETCH_RESPONSE_SERVER_ERROR:
                if (extras != null
                        && extras.containsKey(AwPrefetchCallback.EXTRA_HTTP_RESPONSE_CODE)) {
                    return new PrefetchOperationResult(
                            PrefetchOperationStatusCode.SERVER_FAILURE,
                            extras.getInt(AwPrefetchCallback.EXTRA_HTTP_RESPONSE_CODE));
                }
                return new PrefetchOperationResult(PrefetchOperationStatusCode.SERVER_FAILURE);
            case StatusCode.PREFETCH_START_FAILED_DUPLICATE:
                return new PrefetchOperationResult(PrefetchOperationStatusCode.DUPLICATE_REQUEST);
            default:
                throw new IllegalArgumentException(
                        "Unhandled or invalid prefetch status code - status_code=" + statusCode);
        }
    }
}
