// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import com.android.webview.chromium.SharedWebViewChromium;

import org.chromium.build.annotations.NullMarked;

import java.lang.ref.WeakReference;

/**
 * WebView-specific Header Interceptor Handle, which allows embedders to unregister an interceptor.
 */
@NullMarked
/* package */ class SupportLibWebViewHeaderInterceptorHandle implements Runnable {
    private final WeakReference<SharedWebViewChromium> mSharedWebViewChromium;
    private final int mId;

    public SupportLibWebViewHeaderInterceptorHandle(
            WeakReference<SharedWebViewChromium> sharedWebViewChromium, int id) {
        mSharedWebViewChromium = sharedWebViewChromium;
        mId = id;
    }

    @Override
    public void run() {
        SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
        if (sharedWebViewChromium == null) {
            // The WebView associated to the Header Interceptor died. Ignore this call.
            return;
        }

        sharedWebViewChromium.getAwContents().getHeaderInterceptorStore().removeInterceptor(mId);
    }
}
