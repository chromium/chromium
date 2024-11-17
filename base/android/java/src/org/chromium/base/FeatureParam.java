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
    private static Map<Pair<String, String>, FeatureParam<?>> sParamsForTesting = new HashMap<>();

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

        if (BuildConfig.IS_FOR_TEST) {
            FeatureParam<?> previous =
                    sParamsForTesting.put(new Pair<>(mFeatureName, mParamName), this);
            assert previous == null
                    : String.format(
                            "Feature '%s' has a duplicate parameter: '%s'", featureName, paramName);
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
        if (sParamsForTesting == null) return;
        for (FeatureParam<?> param : sParamsForTesting.values()) {
            param.mInMemoryCachedValue = null;
        }
    }

    /**
     * Use an empty sParams map for this test instead of carrying over from and to other tests in
     * the same process (batched or Robolectric).
     */
    public static void useTemporaryParamsCreatedForTesting() {
        Map<Pair<String, String>, FeatureParam<?>> oldValues = sParamsForTesting;
        sParamsForTesting = new HashMap<>();
        ResettersForTesting.register(() -> sParamsForTesting = oldValues);
    }
}
