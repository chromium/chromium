// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.model_execution;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.model_execution.ExecutionResult.ExecutionError;

/**
 * Instantiable version of {@link ModelExecutionSession}. don't add anything to this class.
 * Downstream targets may provide a different implementation via @ServiceImpl. Other projects may
 * specify a different ModelExecutionSession via ServiceLoader.
 */
public class ModelExecutionSessionUpstreamImpl extends ModelExecutionSession {

    public ModelExecutionSessionUpstreamImpl(@ModelExecutionFeature int feature) {}

    @Override
    public void executeModel(String request, Callback<ExecutionResult> streamingResultCallback) {
        streamingResultCallback.onResult(new ExecutionResult(ExecutionError.NOT_AVAILABLE));
    }
}
