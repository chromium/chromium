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

    private final String mUrl;

    private final boolean mIsOutermostMainFrame;

    private final boolean mHasUserGesture;

    private final boolean mIsRedirect;

    private final String mMethod;

    private final Map<String, String> mRequestHeaders;

    public AwWebResourceRequest(
            String url,
            boolean isOutermostMainFrame,
            boolean hasUserGesture,
            boolean isRedirect,
            String method,
            @Nullable Map<String, String> requestHeaders) {
        this.mUrl = url;
        this.mIsOutermostMainFrame = isOutermostMainFrame;
        this.mHasUserGesture = hasUserGesture;
        this.mIsRedirect = isRedirect;
        this.mMethod = method;
        this.mRequestHeaders = requestHeaders != null ? requestHeaders : new HashMap<>();
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

    /** Url of the request. */
    public String getUrl() {
        return mUrl;
    }

    /** Is this for the outermost main frame or a sub-frame? */
    public boolean isOutermostMainFrame() {
        return mIsOutermostMainFrame;
    }

    /** Was a gesture associated with the request? Don't trust can easily be spoofed. */
    public boolean hasUserGesture() {
        return mHasUserGesture;
    }

    /** Was it a result of a server-side redirect? */
    public boolean isRedirect() {
        return mIsRedirect;
    }

    /** Method used (GET/POST/OPTIONS) */
    public String getMethod() {
        return mMethod;
    }

    /** Headers that would have been sent to server. */
    public Map<String, String> getRequestHeaders() {
        return mRequestHeaders;
    }
}
