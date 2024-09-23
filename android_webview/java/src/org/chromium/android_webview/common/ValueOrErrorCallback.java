// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

/**
 * Generic callback for returning either a result or an exception.
 *
 * @param <T> Result type expected by the callback
 * @param <E> Error type expected by the callback
 */
public interface ValueOrErrorCallback<T, E> {
    void onResult(T result);

    void onError(E error);
}
