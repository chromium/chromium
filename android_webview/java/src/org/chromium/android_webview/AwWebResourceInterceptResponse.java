// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.embedder_support.util.WebResourceResponseInfo;

/** The response information that is to be returned for a particular resource fetch. */
@JNINamespace("android_webview")
public class AwWebResourceInterceptResponse {
    private WebResourceResponseInfo mResponse;
    private boolean mRaisedException;

    public AwWebResourceInterceptResponse(
            WebResourceResponseInfo response, boolean raisedException) {
        mResponse = response;
        mRaisedException = raisedException;
    }

    @CalledByNative
    public WebResourceResponseInfo getResponse() {
        return mResponse;
    }

    @CalledByNative
    public boolean getRaisedException() {
        return mRaisedException;
    }
}
