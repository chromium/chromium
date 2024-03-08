// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.model_execution;

import org.chromium.base.Callback;

/** Base class that exposes methods to execute models with streaming results. */
public abstract class ModelExecutionSession {

    public boolean isAvailable() {
        return false;
    }

    /**
     * Requests model execution with an input string and expecting streaming results. Results are
     * returned as a stream of partial results and as a single full result. Partial results are a
     * couple of characters long and they are meant to be concatenated together for display purposes
     * only. Callers should wait for the final result with the full response to do anything else
     * with the text.
     *
     * @param request Model input.
     * @param streamingResultCallback Callback for streaming and final results.
     */
    public abstract void executeModel(
            String request, Callback<ExecutionResult> streamingResultCallback);
}
