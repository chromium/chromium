// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import androidx.annotation.Nullable;

import org.chromium.android_webview.common.Lifetime;

/** This interface contains the required callback methods for Prefetching. */
@Lifetime.Temporary
public interface PrefetchCallback {

    void onPrefetchStarted();

    void onPrefetchResponseStarted();

    void onPrefetchResponseCompleted();

    void onPrefetchDeterminedHead();

    void onPrefetchFailed(@Nullable String failureMessage);

    void onPrefetchServed();

    void onPrefetchServeFailed(@Nullable String failureMessage);
}
