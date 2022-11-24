// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.common;

/**
 * Used to communicate the result of the JavaScript evaluation from the
 * sandbox to the embedding app.
 * @hide
 */
oneway interface IJsSandboxIsolateCallback {
    // An exception was thrown during the JS evaluation.
    const int JS_EVALUATION_ERROR = 0;
    // The evaluation failed and the isolate crashed due to running out of heap memory.
    const int MEMORY_LIMIT_EXCEEDED = 1;

    void reportResult(String result) = 0;

    // errorType is one of the error constants above.
    void reportError(int errorType, String error) = 1;
}
