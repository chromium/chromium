// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.AwNoVarySearchData;
import org.chromium.android_webview.AwPrefetchParameters;
import org.chromium.android_webview.common.Lifetime;

import java.util.Map;

/** This class represents the Prefetch params to be sent with the prefetch request */
@Lifetime.Temporary
public class PrefetchParams {
    public final @Nullable Map<String, String> additionalHeaders;
    public final @Nullable NoVarySearchData expectedNoVarySearch;
    public final boolean isJavascriptEnabled;

    public PrefetchParams(
            @Nullable Map<String, String> additionalHeaders,
            @Nullable NoVarySearchData expectedNoVarySearch,
            boolean isJavascriptEnabled) {
        this.additionalHeaders = additionalHeaders;
        this.expectedNoVarySearch = expectedNoVarySearch;
        this.isJavascriptEnabled = isJavascriptEnabled;
    }

    @NonNull
    public AwPrefetchParameters toAwPrefetchParams() {
        final AwNoVarySearchData expectedNoVarySearch;
        if (this.expectedNoVarySearch == null) {
            expectedNoVarySearch = null;
        } else {
            expectedNoVarySearch = this.expectedNoVarySearch.toAwNoVarySearchData();
        }
        return new AwPrefetchParameters(
                this.additionalHeaders, expectedNoVarySearch, this.isJavascriptEnabled);
    }
}
