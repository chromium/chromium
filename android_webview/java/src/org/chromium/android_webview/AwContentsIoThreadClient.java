// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.android_webview.http_headers.AwHeaderInterceptor;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Map;

/**
 * Delegate for handling callbacks. All methods are called on the IO thread.
 *
 * <p>You should create a separate instance for every WebContents that requires the provided
 * functionality.
 */
@JNINamespace("android_webview")
@NullMarked
public abstract class AwContentsIoThreadClient {
    private static final String TAG = "IoThreadClient";

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

    /** May return {@code null} when calling shouldInterceptRequest can be skipped. */
    @CalledByNative
    @Nullable
    public abstract ShouldInterceptRequestMediator getShouldInterceptRequestMediator(
            @JniType("std::string") String url);

    @CalledByNative
    public void onLoadResource(@JniType("std::string") String url) {}

    @CalledByNative
    public abstract boolean shouldIncludeCookiesInIntercept();

    @CalledByNative
    @JniType("std::optional<std::map<std::string, std::optional<std::string>>>")
    public static @Nullable Map<String, @Nullable String> interceptHeaders(
            @JniType("android_webview::AwWebResourceRequest") AwWebResourceRequest request,
            AwHeaderInterceptor interceptor) {
        try {
            return interceptor.interceptHeaders(request);
        } catch (Throwable e) {
            Log.e(TAG, "Client raised exception in interceptHeaders. Re-throwing on UI thread.");

            AwThreadUtils.postToUiThreadLooper(
                    () -> {
                        Log.e(TAG, "The following exception was raised by interceptHeaders:");
                        throw e;
                    });

            return null;
        }
    }
}
