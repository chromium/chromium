// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.chromium.base.FeatureList;
import org.chromium.base.Flag;

/**
 * Flags of this type are un-cached flags that may be called before native,
 * but not primarily. They have good default values to use before native is loaded,
 * and will switch to using the native value once native is loaded.
 * These flags replace code like:
 * if (FeatureList.isInitialized() && ChromeFeatureList.isEnabled(featureName))
 * or
 * if (!FeatureList.isInitialized() || ChromeFeatureList.isEnabled(featureName)).
 */
public class MutableFlagWithSafeDefault extends Flag {
    private final boolean mDefaultValue;
    private Boolean mInMemoryCachedValue;

    public MutableFlagWithSafeDefault(String featureName, boolean defaultValue) {
        super(featureName);
        mDefaultValue = defaultValue;
    }

    @Override
    public boolean isEnabled() {
        if (mInMemoryCachedValue != null) return mInMemoryCachedValue;
        if (FeatureList.hasTestFeature(mFeatureName)) {
            return ChromeFeatureList.isEnabled(mFeatureName);
        }

        if (FeatureList.isNativeInitialized()) {
            mInMemoryCachedValue = ChromeFeatureList.isEnabled(mFeatureName);
            return mInMemoryCachedValue;
        }

        return mDefaultValue;
    }

    @Override
    protected void clearInMemoryCachedValueForTesting() {
        mInMemoryCachedValue = null;
    }
}