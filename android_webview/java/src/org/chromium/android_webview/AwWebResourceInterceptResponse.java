// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * The response information that is to be returned for a particular resource fetch.
 */
@JNINamespace("android_webview")
public class AwWebResourceInterceptResponse {
    private AwWebResourceResponse mResponse;
    private boolean mRaisedException;

    public AwWebResourceInterceptResponse(AwWebResourceResponse response, boolean raisedException) {
        mResponse = response;
        mRaisedException = raisedException;
    }

    @CalledByNative
    public AwWebResourceResponse getResponse() {
        return mResponse;
    }

    @CalledByNative
    public boolean getRaisedException() {
        return mRaisedException;
    }
}
