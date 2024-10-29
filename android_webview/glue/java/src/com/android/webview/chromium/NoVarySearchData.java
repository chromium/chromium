// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.AwNoVarySearchData;
import org.chromium.android_webview.common.Lifetime;

import java.util.List;

/**
 * The No-Vary-Search data specifies a set of rules that define how a URL's query parameters will
 * affect cache matching. These rules dictate whether the same URL with different URL parameters
 * should be saved as separate browser cache entries.
 *
 * <p>See https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/No-Vary-Search to learn more
 * about No-Vary-Search.
 */
@Lifetime.Temporary
public class NoVarySearchData {
    public final boolean varyOnKeyOrder;

    public final boolean ignoreDifferencesInParameters;

    public final @Nullable List<String> ignoredQueryParameters;

    public final @Nullable List<String> consideredQueryParameters;

    public NoVarySearchData(
            boolean varyOnKeyOrder,
            boolean ignoreDifferencesInParameters,
            @Nullable List<String> ignoredQueryParameters,
            @Nullable List<String> consideredQueryParameters) {
        this.varyOnKeyOrder = varyOnKeyOrder;
        this.ignoreDifferencesInParameters = ignoreDifferencesInParameters;
        this.ignoredQueryParameters = ignoredQueryParameters;
        this.consideredQueryParameters = consideredQueryParameters;
    }

    @NonNull
    public AwNoVarySearchData toAwNoVarySearchData() {
        return new AwNoVarySearchData(
                varyOnKeyOrder,
                ignoreDifferencesInParameters,
                ignoredQueryParameters == null
                        ? null
                        : ignoredQueryParameters.toArray(new String[0]),
                consideredQueryParameters == null
                        ? null
                        : consideredQueryParameters.toArray(new String[0]));
    }
}
