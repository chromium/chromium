// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

/** Factory for creating {@link HubLayoutAnimationRunner}s. */
public class HubLayoutAnimationRunnerFactory {
    /**
     * Creates a new instance of {@link HubLayoutAnimatorRunner} to run the provided animation.
     *
     * @param animatorProvider The {@link HubLayoutAnimatorProvider} that will provide the
     *     animation.
     * @return an instance of {@link HubLayoutAnimatorRunnerImpl}.
     */
    public static HubLayoutAnimationRunner createHubLayoutAnimationRunner(
            HubLayoutAnimatorProvider animatorProvider) {
        return new HubLayoutAnimationRunnerImpl(animatorProvider);
    }
}
