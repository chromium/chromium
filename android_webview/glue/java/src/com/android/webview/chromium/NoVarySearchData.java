// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import androidx.annotation.Nullable;

import org.chromium.android_webview.common.Lifetime;

import java.util.List;

@Lifetime.Temporary
public class NoVarySearchData {
    public final boolean mVaryOnKeyOrder;

    public final boolean mIgnoreDifferencesInParameters;

    public final @Nullable List<String> mIgnoredQueryParameters;

    public final @Nullable List<String> mConsideredQueryParameters;

    public NoVarySearchData(
            boolean varyOnKeyOrder,
            boolean ignoreDifferencesInParameters,
            @Nullable List<String> ignoredQueryParameters,
            @Nullable List<String> consideredQueryParameters) {
        this.mVaryOnKeyOrder = varyOnKeyOrder;
        this.mIgnoreDifferencesInParameters = ignoreDifferencesInParameters;
        this.mIgnoredQueryParameters = ignoredQueryParameters;
        this.mConsideredQueryParameters = consideredQueryParameters;
    }
}
