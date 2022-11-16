// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Flag;

/**
 * Flags of this type may be used before native is loaded and return the value read
 * from native and cached to SharedPreferences in a previous run.
 * @see {@link CachedFeatureFlags#isEnabled(String, boolean)}
 */
public class CachedFlag extends Flag {
    private final boolean mDefaultValue;

    public CachedFlag(String featureName, boolean defaultValue) {
        super(featureName);
        mDefaultValue = defaultValue;
    }

    @Override
    public boolean isEnabled() {
        return CachedFeatureFlags.isEnabled(mFeatureName, mDefaultValue);
    }

    @Override
    protected void clearInMemoryCachedValueForTesting() {}

    @VisibleForTesting
    public void setForTesting(@Nullable Boolean value) {
        CachedFeatureFlags.setForTesting(mFeatureName, value);
    }

    public void cacheFeature() {
        CachedFeatureFlags.cacheFeature(mFeatureName);
    }
}
