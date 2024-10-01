// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import androidx.annotation.Nullable;

import org.chromium.android_webview.common.Lifetime;

import java.util.Map;

/** This class represents the Prefetch params to be sent with the prefetch request */
@Lifetime.Temporary
public class PrefetchParams {
    public final @Nullable Map<String, String> additionalHeaders;
    public final @Nullable NoVarySearchData noVarySearchData;

    public PrefetchParams(
            @Nullable Map<String, String> additionalHeaders,
            @Nullable NoVarySearchData noVarySearchData) {
        this.additionalHeaders = additionalHeaders;
        this.noVarySearchData = noVarySearchData;
    }
}
