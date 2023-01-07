// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.common;

import android.content.res.AssetFileDescriptor;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolateCallback;

/**
 * Used by the embedding app to execute JavaScript in a sandboxed environment.
 * @hide
 */
interface IJsSandboxIsolate {
    /**
     * @param code the JavaScript code
     * to be evaluated in the sandbox.
     * @param callback used to pass the information back to the embedding app
     * from the sandbox.
     */
    void evaluateJavascript(String code, in IJsSandboxIsolateCallback callback) = 0;

    /**
     * Stop the execution of the Isolate as soon as possible and destroy it.
     */
    void close() = 1;

    /**
     * Provides the data represented by afd such that it can be
     * retrieved in the JS code by calling `consumeNamedDataAs*(name)` APIs.
     * @param name   the id used to refer to the data in JS.
     * @param afd    input AssetFileDescriptor which will be read to retrieve data.
     * @return     true if data with the given name can be retrieved
     *             in JS code, else false.
     */
    boolean provideNamedData(String name, in AssetFileDescriptor afd) = 2;
}
