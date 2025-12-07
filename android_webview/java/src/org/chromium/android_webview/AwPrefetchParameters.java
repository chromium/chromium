// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.android_webview.common.Lifetime;

import java.util.HashMap;
import java.util.Map;

/*
 * The parameters used for WebView app initiated prefetching. This class is passed and read across
 * JNI therefore its method and member names & types should not be modified.
 */
@JNINamespace("android_webview")
@Lifetime.Temporary
public class AwPrefetchParameters {
    private final @NonNull Map<String, String> mAdditionalHeaders;
    private final @Nullable AwNoVarySearchData mExpectedNoVarySearch;
    private final boolean mIsJavascriptEnabled;

    public AwPrefetchParameters(
            @Nullable Map<String, String> additionalHeaders,
            @Nullable AwNoVarySearchData expectedNoVarySearch,
            boolean isJavascriptEnabled) {
        mAdditionalHeaders = additionalHeaders != null ? additionalHeaders : new HashMap<>();
        mExpectedNoVarySearch = expectedNoVarySearch;
        mIsJavascriptEnabled = isJavascriptEnabled;
    }

    @CalledByNative
    @NonNull
    @JniType("std::map<std::string, std::string>")
    public Map<String, String> getAdditionalHeaders() {
        return mAdditionalHeaders;
    }

    @CalledByNative
    @Nullable
    public AwNoVarySearchData getExpectedNoVarySearch() {
        return mExpectedNoVarySearch;
    }

    @CalledByNative
    public boolean getIsJavascriptEnabled() {
        return mIsJavascriptEnabled;
    }
}
