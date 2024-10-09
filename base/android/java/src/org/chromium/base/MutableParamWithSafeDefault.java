// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.NonNull;

/**
 * Abstract class for params that have a safe default before native is loaded. Because param values
 * actually come from native, which during early startup has not been initialized, it's not always
 * safe to read the actual value. The idea here is that this class will check if native has
 * initialized, and when it has not, a safe default will be returned instead. Only after native
 * initializes will we call down into the real experiment logic and get the correct value. That
 * means that the returned value this class vends may change over time, between pre and post
 * initialization.
 *
 * <p>This class should not be used to decide if or how to initialize long lived objects, especially
 * if those objects are created near or during start up.
 *
 * <p>Lastly, this class also caches the value once native is initialized, allowing us to avoid
 * crossing the JNI on every access, which is often beneficial for params that are checked
 * frequently or in performance sensitive areas.
 *
 * @param <T> The boxed type of data behind held.
 */
public abstract class MutableParamWithSafeDefault<T> extends FeatureParam<T> {
    public MutableParamWithSafeDefault(
            FeatureMap featureMap, String featureName, String paramName, @NonNull T defaultValue) {
        super(featureMap, featureName, paramName, defaultValue);
    }

    /**
     * The actual read method when the subclass pulls the param value out the feature map and
     * converts it to the correct data type. Should never return a null value, because caching
     * relies on null checks.
     */
    protected abstract @NonNull T readValueFromFeatureMap();

    /**
     * Returns the current value. Guaranteed to never be null. Subclasses should override this to
     * safely convert to their primitive type.
     */
    protected @NonNull T getValueBoxed() {
        if (mInMemoryCachedValue != null) return mInMemoryCachedValue;

        if (FeatureList.hasTestParam(mFeatureName, mParamName)) {
            return readValueFromFeatureMap();
        }

        if (FeatureList.isNativeInitialized()) {
            mInMemoryCachedValue = readValueFromFeatureMap();
            return mInMemoryCachedValue;
        }

        return mDefaultValue;
    }
}
