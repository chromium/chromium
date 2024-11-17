// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.model_execution;

import org.chromium.base.ServiceLoaderUtil;

/** Class in charge of creating and maintaining model execution sessions for multiple features. */
public class ModelExecutionManager {

    public ModelExecutionSession createSession(@ModelExecutionFeature int feature) {
        ModelExecutionSessionFactory factory =
                ServiceLoaderUtil.maybeCreate(ModelExecutionSessionFactory.class);
        if (factory == null) {
            return new ModelExecutionSessionUpstreamImpl(feature);
        }
        return factory.forFeature(feature);
    }
}
