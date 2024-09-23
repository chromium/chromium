// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.model_execution;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.model_execution.ExecutionResult.ExecutionError;

/**
 * Instantiable version of {@link ModelExecutionSession}. don't add anything to this class.
 * Downstream targets may provide a different implementation. In GN, we specify that {@link
 * ModelExecutionSession} is compiled separately from its implementation; other projects may specify
 * a different ModelExecutionSession via GN.
 */
public class ModelExecutionSessionImpl extends ModelExecutionSession {

    public ModelExecutionSessionImpl(@ModelExecutionFeature int feature) {}

    @Override
    public void executeModel(String request, Callback<ExecutionResult> streamingResultCallback) {
        streamingResultCallback.onResult(new ExecutionResult(ExecutionError.NOT_AVAILABLE));
    }
}
