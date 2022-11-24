// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.service;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Callback interface for the native code to report a JavaScript evaluation outcome.
 */
@JNINamespace("android_webview")
public interface JsSandboxIsolateCallback {
    /**
     * Called when an evaluation succeeds immediately or after its promise resolves.
     *
     * @param result The string result of the evaluation or resolved evaluation promise.
     */
    @CalledByNative
    void onResult(String result);
    /**
     * Called in the event of an error.
     *
     * @param errorType See
     *        {@link org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback} for
     *        error types.
     * @param error String description of the error.
     */
    @CalledByNative
    void onError(int errorType, String error);
}
