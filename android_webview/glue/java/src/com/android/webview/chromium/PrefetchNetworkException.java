// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import org.chromium.android_webview.common.Lifetime;

/** A variation of {@link PrefetchException} for the network operation. */
@Lifetime.Temporary
public class PrefetchNetworkException extends PrefetchException {
    private final int mHttpResponseStatusCode;

    public PrefetchNetworkException(String error, int httpResponseStatusCode) {
        super(error);
        mHttpResponseStatusCode = httpResponseStatusCode;
    }

    /**
     * HttpResponseStatusCode is based on <a
     * href="https://developer.mozilla.org/en-US/docs/Web/HTTP/Status">MDN web response codes</a>.
     */
    public int getHttpResponseStatusCode() {
        return mHttpResponseStatusCode;
    }
}
