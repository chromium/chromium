// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview;

/**
 * A set of callbacks related to WebView prefetching operations.
 *
 * @param <T> the result type.
 */
public interface AwPrefetchOperationCallback<T> {

    void onResult(T result);

    void onError(Throwable e);
}
