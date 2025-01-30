// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * Delegate for handling callbacks. All methods are called on the IO thread.
 *
 * <p>You should create a separate instance for every WebContents that requires the provided
 * functionality.
 */
@JNINamespace("android_webview")
public abstract class AwContentsIoThreadClient {
    // TODO(crbug.com/389047726): Rename this to IoThreadClient.
    @CalledByNative
    public abstract int getCacheMode();

    @CalledByNative
    public abstract boolean shouldBlockContentUrls();

    @CalledByNative
    public abstract boolean shouldBlockFileUrls();

    @CalledByNative
    public abstract boolean shouldBlockSpecialFileUrls();

    @CalledByNative
    public abstract boolean shouldBlockNetworkLoads();

    @CalledByNative
    public abstract boolean shouldAcceptCookies();

    @CalledByNative
    public abstract boolean shouldAcceptThirdPartyCookies();

    @CalledByNative
    public abstract boolean getSafeBrowsingEnabled();

    @CalledByNative
    public abstract AwContentsBackgroundThreadClient getBackgroundThreadClient();

    @NativeMethods
    interface Natives {
        boolean finishShouldInterceptRequest(
                int requestId, AwWebResourceInterceptResponse response);
    }
}
