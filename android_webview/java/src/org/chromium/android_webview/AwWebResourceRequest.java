// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.HashMap;

/** Parameters for the {@link AwContentsClient#shouldInterceptRequest} method. */
public class AwWebResourceRequest {
    // Prefer using other constructors over this one.
    public AwWebResourceRequest() {}

    public AwWebResourceRequest(
            String url,
            boolean isOutermostMainFrame,
            boolean hasUserGesture,
            String method,
            @Nullable HashMap<String, String> requestHeaders) {
        this.url = url;
        this.isOutermostMainFrame = isOutermostMainFrame;
        this.hasUserGesture = hasUserGesture;
        // Note: we intentionally let isRedirect default initialize to false. This is because we
        // don't always know if this request is associated with a redirect or not.
        this.method = method;
        this.requestHeaders = requestHeaders;
    }

    public AwWebResourceRequest(
            String url,
            boolean isOutermostMainFrame,
            boolean hasUserGesture,
            String method,
            @NonNull String[] requestHeaderNames,
            @NonNull String[] requestHeaderValues) {
        this(
                url,
                isOutermostMainFrame,
                hasUserGesture,
                method,
                new HashMap<String, String>(requestHeaderValues.length));
        for (int i = 0; i < requestHeaderNames.length; ++i) {
            this.requestHeaders.put(requestHeaderNames[i], requestHeaderValues[i]);
        }
    }

    // Url of the request.
    public String url;
    // Is this for the outermost main frame or a subframe?
    public boolean isOutermostMainFrame;
    // Was a gesture associated with the request? Don't trust can easily be spoofed.
    public boolean hasUserGesture;
    // Was it a result of a server-side redirect?
    public boolean isRedirect;
    // Method used (GET/POST/OPTIONS)
    public String method;
    // Headers that would have been sent to server.
    public HashMap<String, String> requestHeaders;
}
