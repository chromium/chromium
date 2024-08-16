// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.Nullable;

/** Boundary interface for PrefetchCallback. */
public interface PrefetchCallbackBoundaryInterface {
    void onStarted();

    void onResponseStarted();

    void onResponseHeaderReceived();

    void onCompleted();

    void onFailed(@Nullable String failureMessage);

    void onResponseServed();

    void onResponseServeFailed(@Nullable String failureMessage);
}
