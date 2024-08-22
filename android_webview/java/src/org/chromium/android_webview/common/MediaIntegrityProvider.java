// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/** Async handler for media integrity token requests. */
public interface MediaIntegrityProvider {

    /**
     * Asynchronously request a token.
     *
     * @param contentBinding An optional content binding string.
     * @param callback Callback to be called with the result of the request.
     */
    void requestToken2(
            @Nullable String contentBinding,
            @NonNull ValueOrErrorCallback<String, MediaIntegrityErrorWrapper> callback);
}
