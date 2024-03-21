// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.CheckDiscard;

import java.util.HashMap;
import java.util.Map;

import javax.annotation.concurrent.NotThreadSafe;

/**
 * Base class for params, that are essentially a single value scoped within a feature.
 *
 * @param <T> The boxed type of data behind held.
 */
@NotThreadSafe
public abstract class FeatureParam<T> {
    @CheckDiscard("Only needed to reset tests. Production code shouldn't use.")
    private static final Map<Pair<String, String>, FeatureParam> sParams = new HashMap<>();

    protected final FeatureMap mFeatureMap;
    protected final String mFeatureName;
    protected final String mParamName;
    protected final T mDefaultValue;

    // Null means this is not cached, and the feature map should be read from.
    @Nullable protected T mInMemoryCachedValue;

    public FeatureParam(
            @NonNull FeatureMap featureMap,
            @NonNull String featureName,
            @NonNull String paramName,
            @NonNull T defaultValue) {
        assert defaultValue != null;
        mFeatureMap = featureMap;
        mFeatureName = featureName;
        mParamName = paramName;
        mDefaultValue = defaultValue;

        if (BuildConfig.ENABLE_ASSERTS) {
            FeatureParam previous = sParams.put(new Pair<>(mFeatureName, mParamName), this);
            assert previous == null;
        }
    }

    /** Return the name of the feature this param is scoped within. */
    public String getFeatureName() {
        return mFeatureName;
    }

    /** Returns the name of this param. */
    public String getName() {
        return mParamName;
    }

    /**
     * Clears all cached param values. This will give the next test a clean slate so that test
     * values can be read through. This shouldn't be used directly by individual tests other than
     * those that exercise FeatureParam subclasses.
     */
    public static void resetAllInMemoryCachedValuesForTesting() {
        if (sParams == null) return;
        for (FeatureParam param : sParams.values()) {
            param.mInMemoryCachedValue = null;
        }
    }

    /** Most clients should not call this, only really for param tests. */
    public static void deleteParamsForTesting() {
        resetAllInMemoryCachedValuesForTesting();
        sParams.clear();
    }
}
