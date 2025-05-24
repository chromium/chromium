// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Flags of this type assume native is loaded and the value can be retrieved directly from native.
 */
@NullMarked
public class PostNativeFlag extends Flag {
    private @Nullable Boolean mInMemoryCachedValue;

    public PostNativeFlag(FeatureMap featureMap, String featureName) {
        super(featureMap, featureName);
    }

    @Override
    public boolean isEnabled() {
        if (mInMemoryCachedValue != null) return mInMemoryCachedValue;

        if (FeatureOverrides.hasTestFeature(mFeatureName)) {
            return mFeatureMap.isEnabledInNative(mFeatureName);
        }

        mInMemoryCachedValue = mFeatureMap.isEnabledInNative(mFeatureName);
        return mInMemoryCachedValue;
    }

    @Override
    protected void clearInMemoryCachedValueForTesting() {
        mInMemoryCachedValue = null;
    }
}
