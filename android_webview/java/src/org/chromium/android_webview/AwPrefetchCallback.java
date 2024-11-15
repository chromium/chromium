// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview;

import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Callback for WebView app initiated prefetching. */
public interface AwPrefetchCallback {

    @IntDef({
        StatusCode.PREFETCH_START_FAILED,
        StatusCode.PREFETCH_RESPONSE_COMPLETED,
        StatusCode.PREFETCH_RESPONSE_SERVER_ERROR,
        StatusCode.PREFETCH_RESPONSE_GENERIC_ERROR
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface StatusCode {
        int PREFETCH_START_FAILED = 0;
        int PREFETCH_RESPONSE_COMPLETED = 1;
        int PREFETCH_RESPONSE_SERVER_ERROR = 2;
        int PREFETCH_RESPONSE_GENERIC_ERROR = 3;
    }

    public static final String EXTRA_HTTP_RESPONSE_CODE = "HttpResponseCode";

    void onStatusUpdated(@StatusCode int statusCode, @Nullable Bundle extras);

    void onError(Throwable e);
}
