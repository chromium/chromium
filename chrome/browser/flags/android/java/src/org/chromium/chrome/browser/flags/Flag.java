// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import java.util.HashSet;

/**
 * Defines a feature flag for use in Java.
 *
 * Duplicate flag definitions are not permitted, so only a single
 * instance can be created with a given feature name.
 *
 * To create a flag, instantiate a subclass of Flag, either {@link CachedFlag},
 * {@link MutableFlagWithSafeDefault} or {@link PostNativeFlag}.
 */
public abstract class Flag {
    private static HashSet<String> sFlagsCreated = new HashSet<>();
    protected final String mFeatureName;

    Flag(String featureName) {
        assert !sFlagsCreated.contains(featureName);
        mFeatureName = featureName;
        sFlagsCreated.add(mFeatureName);
    }

    /**
     * Checks if a feature flag is enabled.
     * @return whether the feature should be considered enabled.
     */
    public abstract boolean isEnabled();

    static void resetFlagsForTesting() {
        sFlagsCreated.clear();
    }
}
