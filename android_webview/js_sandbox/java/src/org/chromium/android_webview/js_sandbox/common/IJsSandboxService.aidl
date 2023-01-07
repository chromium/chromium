// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.js_sandbox.common;
import org.chromium.android_webview.js_sandbox.common.IJsSandboxIsolate;

/**
 * Used by the embedding app to execute JavaScript in a sandboxed environment.
 * @hide
 */
interface IJsSandboxService {
    IJsSandboxIsolate createIsolate() = 0;

    /**
     * Feature flag indicating that closing an isolate will terminate its
     * execution as soon as possible, instead of allowing previously-requested
     * executions to run to completion first.
     */
    const String ISOLATE_TERMINATION = "ISOLATE_TERMINATION";

    /**
     * Feature flag indicating that isolate creation can take a parameter
     * to set the maximum heap size limit of the isolate.
     */
    const String ISOLATE_MAX_HEAP_SIZE_LIMIT = "ISOLATE_MAX_HEAP_SIZE_LIMIT";

    /**
     * This feature flag is a combination of three sub-features:
     * - If evaluateJavascript() returns a promise, we wait for the promise
     *   to resolve and then return the resolved value.
     * - Supports Java API provideNamedData() and JS API
     *   android.consumeNamedDataAsArrayBuffer().
     * - WebAssembly.compile() API is supported. Wasm can be compiled from
     *   an array buffer.
     */
    const String WASM_FROM_ARRAY_BUFFER = "WASM_FROM_ARRAY_BUFFER";

    /**
     * @return A list of feature names supported by this implementation.
     */
    List<String> getSupportedFeatures() = 1;

    IJsSandboxIsolate createIsolateWithMaxHeapSizeBytes(long maxHeapSize) = 2;

}
