// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js.common;

import org.chromium.android_webview.js.common.IJsSandboxContextCallback;

/**
 * Used by the embedding app to execute JavaScript in a sandboxed environment.
 */
interface IJsSandboxContext {
    /**
     * @param code the JavaScript code
     * to be evaluated in the sandbox.
     * @param callback used to pass the information back to the embedding app
     * from the sandbox.
     */
    void evaluateJavascript(String code, in IJsSandboxContextCallback callback) = 0;

    /**
     * Stop the execution of the context as soon as possible and destroy it.
     */
    void close() = 1;
}
