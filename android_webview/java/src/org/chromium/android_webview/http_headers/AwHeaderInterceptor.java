// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.http_headers;

import org.chromium.android_webview.AwWebResourceRequest;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Map;

/** Intercepts HTTP headers for requests. */
@NullMarked
public interface AwHeaderInterceptor {
    /**
     * Intercepts headers for a given request.
     *
     * <p>This method will be called from a background thread (not the IO thread).
     *
     * @return a map of the headers to be set (with null values for headers that should be removed)
     */
    Map<String, @Nullable String> interceptHeaders(AwWebResourceRequest request);
}
