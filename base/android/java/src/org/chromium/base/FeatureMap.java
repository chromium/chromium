// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Java accessor for state of feature flags and their field trial parameters.
 *
 * This class provides methods to access values of feature flags listed in a native feature list
 * and to access their field trial parameters.
 *
 * This class needs to be derived for each native feature list (such as a component's feature list)
 * and the derived class must implement the abstract {@link #getNativeMap()} by calling a JNI method
 * to get the pointer to the base::android::FeatureMap. The derived class will provide Java code
 * access to the list of base::Features passed to the base::android::FeatureMap.
 */
@JNINamespace("base::android")
public abstract class FeatureMap {
    private long mNativeMapPtr;

    protected FeatureMap() {}

    /**
     * Should return the native pointer to the specific base::FeatureMap for the component/layer.
     */
    protected abstract long getNativeMap();

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Calling this has the side effect of bucketing this client, which may cause an experiment to
     * be marked as active.
     *
     * Should be called only after native is loaded. If {@link FeatureList#isInitialized()} returns
     * true, this method is safe to call.  In tests, this will return any values set through
     * {@link FeatureList#setTestFeatures(Map)}, even before native is loaded.
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public boolean isEnabledInNative(String featureName) {
        Boolean testValue = FeatureList.getTestValueForFeature(featureName);
        if (testValue != null) return testValue;
        ensureNativeMapInit();
        return FeatureMapJni.get().isEnabled(mNativeMapPtr, featureName);
    }

    /**
     * Returns a field trial param for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @return The parameter value as a String. The string is empty if the feature does not exist or
     *   the specified parameter does not exist.
     */
    public String getFieldTrialParamByFeature(String featureName, String paramName) {
        String testValue = FeatureList.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return testValue;
        if (FeatureList.getDisableNativeForTesting()) return "";
        ensureNativeMapInit();
        return FeatureMapJni.get()
                .getFieldTrialParamByFeature(mNativeMapPtr, featureName, paramName);
    }

    /**
     * Returns a field trial param as a boolean for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The boolean value to use if the param is not available.
     * @return The parameter value as a boolean. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value is neither "true" nor "false".
     */
    public boolean getFieldTrialParamByFeatureAsBoolean(
            String featureName, String paramName, boolean defaultValue) {
        String testValue = FeatureList.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Boolean.valueOf(testValue);
        if (FeatureList.getDisableNativeForTesting()) return defaultValue;
        ensureNativeMapInit();
        return FeatureMapJni.get()
                .getFieldTrialParamByFeatureAsBoolean(
                        mNativeMapPtr, featureName, paramName, defaultValue);
    }

    /**
     * Returns a field trial param as an int for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The integer value to use if the param is not available.
     * @return The parameter value as an int. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value does not represent an int.
     */
    public int getFieldTrialParamByFeatureAsInt(
            String featureName, String paramName, int defaultValue) {
        String testValue = FeatureList.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Integer.valueOf(testValue);
        if (FeatureList.getDisableNativeForTesting()) return defaultValue;
        ensureNativeMapInit();
        return FeatureMapJni.get()
                .getFieldTrialParamByFeatureAsInt(
                        mNativeMapPtr, featureName, paramName, defaultValue);
    }

    /**
     * Returns a field trial param as a double for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The double value to use if the param is not available.
     * @return The parameter value as a double. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value does not represent a double.
     */
    public double getFieldTrialParamByFeatureAsDouble(
            String featureName, String paramName, double defaultValue) {
        String testValue = FeatureList.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Double.valueOf(testValue);
        if (FeatureList.getDisableNativeForTesting()) return defaultValue;
        ensureNativeMapInit();
        return FeatureMapJni.get()
                .getFieldTrialParamByFeatureAsDouble(
                        mNativeMapPtr, featureName, paramName, defaultValue);
    }

    /** Returns all the field trial parameters for the specified feature. */
    public Map<String, String> getFieldTrialParamsForFeature(String featureName) {
        Map<String, String> testValues =
                FeatureList.getTestValuesForAllFieldTrialParamsForFeature(featureName);
        if (testValues != null) return testValues;
        if (FeatureList.getDisableNativeForTesting()) return Collections.emptyMap();

        ensureNativeMapInit();
        Map<String, String> result = new HashMap<>();
        String[] flattenedParams =
                FeatureMapJni.get()
                        .getFlattedFieldTrialParamsForFeature(mNativeMapPtr, featureName);
        for (int i = 0; i < flattenedParams.length; i += 2) {
            result.put(flattenedParams[i], flattenedParams[i + 1]);
        }
        return result;
    }

    /** Create a {@link MutableFlagWithSafeDefault} in this FeatureMap. */
    public MutableFlagWithSafeDefault mutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return new MutableFlagWithSafeDefault(this, featureName, defaultValue);
    }

    private void ensureNativeMapInit() {
        assert FeatureList.isNativeInitialized();

        if (mNativeMapPtr == 0) {
            mNativeMapPtr = getNativeMap();
            assert mNativeMapPtr != 0;
        }
    }

    @NativeMethods
    interface Natives {
        boolean isEnabled(long featureMap, @JniType("std::string") String featureName);

        @JniType("std::string")
        String getFieldTrialParamByFeature(
                long featureMap,
                @JniType("std::string") String featureName,
                @JniType("std::string") String paramName);

        int getFieldTrialParamByFeatureAsInt(
                long featureMap,
                @JniType("std::string") String featureName,
                @JniType("std::string") String paramName,
                int defaultValue);

        double getFieldTrialParamByFeatureAsDouble(
                long featureMap,
                @JniType("std::string") String featureName,
                @JniType("std::string") String paramName,
                double defaultValue);

        boolean getFieldTrialParamByFeatureAsBoolean(
                long featureMap,
                @JniType("std::string") String featureName,
                @JniType("std::string") String paramName,
                boolean defaultValue);

        @JniType("std::vector<std::string>")
        String[] getFlattedFieldTrialParamsForFeature(
                long featureMap, @JniType("std::string") String featureName);
    }
}
