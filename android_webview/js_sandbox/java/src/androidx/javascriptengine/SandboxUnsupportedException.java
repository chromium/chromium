// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package androidx.javascriptengine;

import androidx.annotation.NonNull;

/**
 * Exception thrown when attempting to create a {@link JavaScriptSandbox} via
 * {@link JavaScriptSandbox#createConnectedInstanceAsync(Context)} when doing so is not supported.
 * <p>
 * This can occur when the WebView package is too old to provide a sandbox implementation.
 */
public final class SandboxUnsupportedException extends RuntimeException {
    public SandboxUnsupportedException(@NonNull String error) {
        super(error);
    }
}
