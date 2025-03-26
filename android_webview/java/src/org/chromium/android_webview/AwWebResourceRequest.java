// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;

import java.util.HashMap;
import java.util.Map;

/** Parameters for the {@link AwContentsClient#shouldInterceptRequest} method. */
@Lifetime.Temporary
@JNINamespace("android_webview")
@NullMarked
public class AwWebResourceRequest {

    /** Url of the request. */
    public final String url;

    /** Is this for the outermost main frame or a sub-frame? */
    public final boolean isOutermostMainFrame;

    /** Was a gesture associated with the request? Don't trust can easily be spoofed. */
    public final boolean hasUserGesture;

    /** Was it a result of a server-side redirect? */
    public final boolean isRedirect;

    /** Method used (GET/POST/OPTIONS) */
    public final String method;

    /** Headers that would have been sent to server. */
    public final Map<String, String> requestHeaders;

    public AwWebResourceRequest(
            String url,
            boolean isOutermostMainFrame,
            boolean hasUserGesture,
            boolean isRedirect,
            String method,
            @Nullable Map<String, String> requestHeaders) {
        this.url = url;
        this.isOutermostMainFrame = isOutermostMainFrame;
        this.hasUserGesture = hasUserGesture;
        this.isRedirect = isRedirect;
        this.method = method;
        if (requestHeaders != null) {
            this.requestHeaders = requestHeaders;
        } else {
            this.requestHeaders = new HashMap<>();
        }
    }

    public AwWebResourceRequest(
            String url,
            boolean isOutermostMainFrame,
            boolean hasUserGesture,
            String method,
            String[] requestHeaderNames,
            String[] requestHeaderValues) {
        // Note: we intentionally let isRedirect default initialize to false. This is because we
        // don't always know if this request is associated with a redirect or not.
        this(
                url,
                isOutermostMainFrame,
                hasUserGesture,
                /* isRedirect= */ false,
                method,
                headerMapFromArrays(requestHeaderNames, requestHeaderValues));
    }

    private static Map<String, String> headerMapFromArrays(
            String[] requestHeaderNames, String[] requestHeaderValues) {
        assert requestHeaderNames.length == requestHeaderValues.length;
        HashMap<String, String> headers = new HashMap<>(requestHeaderNames.length);
        for (int i = 0; i < requestHeaderNames.length; ++i) {
            headers.put(requestHeaderNames[i], requestHeaderValues[i]);
        }
        return headers;
    }

    @CalledByNative
    private static AwWebResourceRequest create(
            @JniType("std::string") String url,
            boolean isMainFrame,
            boolean hasUserGesture,
            @JniType("std::string") String method,
            @JniType("std::vector<std::string>") String[] requestHeaderNames,
            @JniType("std::vector<std::string>") String[] requestHeaderValues) {
        return new AwWebResourceRequest(
                url, isMainFrame, hasUserGesture, method, requestHeaderNames, requestHeaderValues);
    }
}
