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

/*
 * The parameters used for WebView initiated prefetching No-Vary-Search data.
 * This class is passed and read across JNI therefore its method and member names
 * & types should not be modified.
 */
@JNINamespace("android_webview")
@Lifetime.Temporary
public class AwNoVarySearchData {
    private final boolean mVaryOnKeyOrder;

    private final boolean mIgnoreDifferencesInParameters;

    private final @NonNull String[] mIgnoredQueryParameters;

    private final @NonNull String[] mConsideredQueryParameters;

    public AwNoVarySearchData(
            boolean varyOnKeyOrder,
            boolean ignoreDifferencesInParameters,
            @Nullable String[] ignoredQueryParameters,
            @Nullable String[] consideredQueryParameters) {
        this.mVaryOnKeyOrder = varyOnKeyOrder;
        this.mIgnoreDifferencesInParameters = ignoreDifferencesInParameters;
        this.mIgnoredQueryParameters =
                ignoredQueryParameters != null ? ignoredQueryParameters : new String[] {};
        this.mConsideredQueryParameters =
                consideredQueryParameters != null ? consideredQueryParameters : new String[] {};
    }

    @CalledByNative
    public boolean getVaryOnKeyOrder() {
        return mVaryOnKeyOrder;
    }

    @CalledByNative
    public boolean getIgnoreDifferencesInParameters() {
        return mIgnoreDifferencesInParameters;
    }

    @CalledByNative
    @NonNull
    @JniType("std::vector<std::string>")
    public String[] getIgnoredQueryParameters() {
        return mIgnoredQueryParameters;
    }

    @CalledByNative
    @NonNull
    @JniType("std::vector<std::string>")
    public String[] getConsideredQueryParameters() {
        return mConsideredQueryParameters;
    }

    @NonNull
    @Override
    public String toString() {
        StringBuilder paramsSb = new StringBuilder();
        if (mIgnoreDifferencesInParameters) {
            paramsSb.append("vary-params: [");
            for (int i = 0; i < mConsideredQueryParameters.length; i++) {
                paramsSb.append("\"").append(mConsideredQueryParameters[i]).append("\"");
                if (i < mConsideredQueryParameters.length - 1) {
                    paramsSb.append(", ");
                }
            }
            paramsSb.append("]");
        } else {
            paramsSb.append("no-vary-params: [");
            for (int i = 0; i < mIgnoredQueryParameters.length; i++) {
                paramsSb.append("\"").append(mIgnoredQueryParameters[i]).append("\"");
                if (i < mIgnoredQueryParameters.length - 1) {
                    paramsSb.append(", ");
                }
            }
            paramsSb.append("]");
        }
        return String.format("{key-order: %s, %s}", mVaryOnKeyOrder, paramsSb);
    }
}
