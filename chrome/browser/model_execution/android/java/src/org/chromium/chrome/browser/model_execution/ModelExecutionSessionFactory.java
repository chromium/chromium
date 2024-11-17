// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.model_execution;

public interface ModelExecutionSessionFactory {
    /**
     * Create a ModelExecutionSession for the given {@code feature}. If an invalid value of
     * ModelExecutionFeature is passed then it'll throw an assertion error.
     */
    ModelExecutionSession forFeature(@ModelExecutionFeature int feature);
}
