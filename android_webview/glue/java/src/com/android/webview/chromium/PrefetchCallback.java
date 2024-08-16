// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import androidx.annotation.Nullable;

import org.chromium.android_webview.common.Lifetime;

/** This interface contains the required callback methods for Prefetching. */
@Lifetime.Temporary
public interface PrefetchCallback {

    void onStarted();

    void onResponseStarted();

    void onResponseHeaderReceived();

    void onCompleted();

    void onFailed(@Nullable String failureMessage);

    void onResponseServed();

    void onResponseServeFailed(@Nullable String failureMessage);
}
