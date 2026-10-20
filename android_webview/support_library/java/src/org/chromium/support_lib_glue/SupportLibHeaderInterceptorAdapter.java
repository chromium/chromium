// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.webkit.WebResourceRequest;

import com.android.webview.chromium.WebResourceRequestAdapter;

import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import org.chromium.android_webview.AwWebResourceRequest;
import org.chromium.android_webview.http_headers.AwHeaderInterceptor;

import java.util.Map;
import java.util.function.Function;

/** Adapter between {@link AwHeaderInterceptor} and a client-provider header interceptor. */
@NullMarked
public class SupportLibHeaderInterceptorAdapter implements AwHeaderInterceptor {
    private final Function<WebResourceRequest, Map<String, @Nullable String>> mHeaderInterceptor;

    public SupportLibHeaderInterceptorAdapter(
            Function<WebResourceRequest, Map<String, @Nullable String>> headerInterceptor) {
        mHeaderInterceptor = headerInterceptor;
    }

    @Override
    public Map<String, @Nullable String> interceptHeaders(AwWebResourceRequest request) {
        return mHeaderInterceptor.apply(new WebResourceRequestAdapter(request));
    }
}
