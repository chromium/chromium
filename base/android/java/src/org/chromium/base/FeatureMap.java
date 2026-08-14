// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.CheckDiscard;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Java accessor for state of feature flags and their field trial parameters.
 *
 * <p>This class provides methods to access values of feature flags listed in a native feature list
 * and to access their field trial parameters.
 *
 * <p>This class needs to be derived for each native feature list (such as a component's feature
 * list) and the derived class must implement the abstract {@link #getNativeMap()} by calling a JNI
 * method to get the pointer to the base::android::FeatureMap. The derived class will provide Java
 * code access to the list of base::Features passed to the base::android::FeatureMap.
 *
 * <p>After overriding this class, please also add an entry to
 * ChromeCachedFlags.LIST_OF_FEATURE_MAPS.
 */
@NullMarked
@JNINamespace("base::android")
public abstract class FeatureMap {
    private long mNativeMapPtr;

    protected FeatureMap() {}

    /**
     * Returns the native pointer to the specific base::android::FeatureMap for the component/layer.
     */
    protected abstract long getNativeMap();

    /**
     * Returns a map from the feature name to the feature's default value in tests, for all features
     * associated with this feature map.
     */
    public @Nullable Map<String, Boolean> getFlagsDefaultValuesInTests() {
        return null;
    }

    /**
     * Returns a map from the feature name, to a map from the param name to the param's default
     * value, for all params associated with this feature map.
     */
    public @Nullable Map<String, Map<String, String>> getParamsDefaultValues() {
        return null;
    }

    /**
     * Returns a map from the feature name, to a map from the param name to the param's default
     * value in tests, for all params associated with this feature map.
     */
    public @Nullable Map<String, Map<String, String>> getParamsDefaultValuesInTests() {
        return null;
    }

    /**
     * Given a feature name, retrieve the default value in tests of that feature from {@link
     * FeatureMap#getFlagsDefaultValuesInTests()}. If there is no default value, return null.
     */
    @CheckDiscard("getFlagsDefaultValuesInTests() should return null in non-test builds")
    private @Nullable Boolean getFlagDefaultValueInTests(String featureName) {
        Map<String, Boolean> flagsDefaultValuesInTests = getFlagsDefaultValuesInTests();
        if (flagsDefaultValuesInTests == null) {
            return null;
        }
        return flagsDefaultValuesInTests.get(featureName);
    }

    /**
     * Given a feature name and a param name, retrieve the default value of that param from {@link
     * FeatureMap#getParamsDefaultValues()}. This method throws when the default value cannot be
     * found, because we must have a default value, which is the value to use when the specified
     * param does not exist in the field trial.
     */
    public String getParamDefaultValue(String featureName, String paramName) {
        Map<String, Map<String, String>> paramsDefaultValues = getParamsDefaultValues();
        if (paramsDefaultValues == null) {
            throw new RuntimeException(
                    "You did not override getParamsDefaultValues() in the "
                            + "FeatureMap subclass");
        }
        Map<String, String> paramNameToDefaultValue = paramsDefaultValues.get(featureName);
        if (paramNameToDefaultValue == null) {
            throw new RuntimeException("sParamsDefaultValues does not contain " + featureName);
        }
        String defaultValue = paramNameToDefaultValue.get(paramName);
        if (defaultValue == null) {
            throw new RuntimeException("sParamsDefaultValues does not contain " + paramName);
        }
        return defaultValue;
    }

    /**
     * Given a feature name and a param name, retrieve the default value in tests of that param from
     * {@link FeatureMap#getParamsDefaultValuesInTests()}.
     */
    @CheckDiscard("getParamsDefaultValuesInTests() should return null in non-test builds")
    private @Nullable String getParamDefaultValueInTests(String featureName, String paramName) {
        Map<String, Map<String, String>> paramsDefaultValuesInTests =
                getParamsDefaultValuesInTests();
        if (paramsDefaultValuesInTests == null) {
            return null;
        }
        Map<String, String> paramNameToDefaultValue = paramsDefaultValuesInTests.get(featureName);
        if (paramNameToDefaultValue == null) {
            return null;
        }
        return paramNameToDefaultValue.get(paramName);
    }

    private void throwException(String featureName, @Nullable String paramName) {
        throw new IllegalArgumentException(
                "No test value configured for feature "
                        + featureName
                        + (paramName == null ? "" : " param " + paramName)
                        + " and native is not available to provide a default value. Use"
                        + " @EnableFeatures or @DisableFeatures to provide test values for"
                        + " the feature/param.");
    }

    public boolean queryFeatureValueFromNative(String featureName) {
        ensureNativeMapInit();
        return FeatureMapJni.get().isEnabled(mNativeMapPtr, featureName);
    }

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * <p>Calling this has the side effect of bucketing this client, which may cause an experiment
     * to be marked as active. Should be called only after native is loaded.
     *
     * <p>This method performs a series of actions:
     *
     * <ol>
     *   <li>Returns any values set through @Enable/@DisableFeatures annotations or {@link
     *       FeatureOverrides}
     *   <li>If there is no value set, returns any default value in tests in {@link
     *       FeatureMap#getFlagsDefaultValuesInTests()}
     *   <li>If there is no default value in tests, this will query the feature value from native
     *       and return the native value
     * </ol>
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public boolean isEnabledInNative(String featureName) {
        Boolean testValue = FeatureOverrides.getTestValueForFeature(featureName);
        if (testValue != null) return testValue;
        Boolean defaultValueInTests = getFlagDefaultValueInTests(featureName);
        if (defaultValueInTests != null) return defaultValueInTests;
        if (FeatureList.getDisableNativeForTesting()) throwException(featureName, null);
        return queryFeatureValueFromNative(featureName);
    }

    /**
     * Returns a field trial param for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @return The parameter value as a String. The string is empty if the feature does not exist or
     *     the specified parameter does not exist.
     * @deprecated Use {@link FeatureMap#getFieldTrialParamByFeatureAsString(String, String)}
     *     instead.
     */
    public String getFieldTrialParamByFeature(String featureName, String paramName) {
        String testValue = FeatureOverrides.getTestValueForFieldTrialParam(featureName, paramName);
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
     *     specified parameter does not exist or its string value is neither "true" nor "false".
     * @deprecated Use {@link FeatureMap#getFieldTrialParamByFeatureAsBoolean(String, String)}
     *     instead.
     */
    public boolean getFieldTrialParamByFeatureAsBoolean(
            String featureName, String paramName, boolean defaultValue) {
        String testValue = FeatureOverrides.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Boolean.valueOf(testValue);
        if (FeatureList.getDisableNativeForTesting()) return defaultValue;
        ensureNativeMapInit();
        return FeatureMapJni.get()
                .getFieldTrialParamByFeatureAsBoolean(
                        mNativeMapPtr, featureName, paramName, defaultValue);
    }

    /**
     * Returns a field trial param as a boolean for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get value for.
     * @return The parameter value as a boolean. Default value from {@link
     *     FeatureMap#getParamsDefaultValues()} if the feature does not exist or the specified
     *     parameter does not exist or its string value is neither "true" nor "false".
     */
    public boolean getFieldTrialParamByFeatureAsBoolean(String featureName, String paramName) {
        String testValue = FeatureOverrides.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Boolean.valueOf(testValue);
        String defaultValueInTests = getParamDefaultValueInTests(featureName, paramName);
        if (defaultValueInTests != null) return Boolean.valueOf(defaultValueInTests);
        if (FeatureList.getDisableNativeForTesting()) throwException(featureName, paramName);
        ensureNativeMapInit();
        String defaultValue = getParamDefaultValue(featureName, paramName);
        return FeatureMapJni.get()
                .getFieldTrialParamByFeatureAsBoolean(
                        mNativeMapPtr, featureName, paramName, Boolean.valueOf(defaultValue));
    }

    /**
     * Returns a field trial param as an int for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The integer value to use if the param is not available.
     * @return The parameter value as an int. Default value if the feature does not exist or the
     *     specified parameter does not exist or its string value does not represent an int.
     * @deprecated Use {@link FeatureMap#getFieldTrialParamByFeatureAsInt(String, String)} instead.
     */
    public int getFieldTrialParamByFeatureAsInt(
            String featureName, String paramName, int defaultValue) {
        String testValue = FeatureOverrides.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Integer.valueOf(testValue);
        if (FeatureList.getDisableNativeForTesting()) return defaultValue;
        ensureNativeMapInit();
        return FeatureMapJni.get()
                .getFieldTrialParamByFeatureAsInt(
                        mNativeMapPtr, featureName, paramName, defaultValue);
    }

    /**
     * Returns a field trial param as an int for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get value for.
     * @return The parameter value as an int. Default value from {@link
     *     FeatureMap#getParamsDefaultValues()} if the feature does not exist or the specified
     *     parameter does not exist or its string value does not represent an int.
     */
    public int getFieldTrialParamByFeatureAsInt(String featureName, String paramName) {
        String testValue = FeatureOverrides.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Integer.valueOf(testValue);
        String defaultValueInTests = getParamDefaultValueInTests(featureName, paramName);
        if (defaultValueInTests != null) return Integer.valueOf(defaultValueInTests);
        if (FeatureList.getDisableNativeForTesting()) throwException(featureName, paramName);
        ensureNativeMapInit();
        String defaultValue = getParamDefaultValue(featureName, paramName);
        return FeatureMapJni.get()
                .getFieldTrialParamByFeatureAsInt(
                        mNativeMapPtr, featureName, paramName, Integer.valueOf(defaultValue));
    }

    /**
     * Returns a field trial param as a double for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as an integer.
     * @param defaultValue The double value to use if the param is not available.
     * @return The parameter value as a double. Default value if the feature does not exist or the
     *     specified parameter does not exist or its string value does not represent a double.
     * @deprecated Use {@link FeatureMap#getFieldTrialParamByFeatureAsDouble(String, String)}
     *     instead.
     */
    public double getFieldTrialParamByFeatureAsDouble(
            String featureName, String paramName, double defaultValue) {
        String testValue = FeatureOverrides.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Double.valueOf(testValue);
        if (FeatureList.getDisableNativeForTesting()) return defaultValue;
        ensureNativeMapInit();
        return FeatureMapJni.get()
                .getFieldTrialParamByFeatureAsDouble(
                        mNativeMapPtr, featureName, paramName, defaultValue);
    }

    /**
     * Returns a field trial param as a double for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get value for.
     * @return The parameter value as a double. Default value from {@link
     *     FeatureMap#getParamsDefaultValues()} if the feature does not exist or the specified
     *     parameter does not exist or its string value does not represent a double.
     */
    public double getFieldTrialParamByFeatureAsDouble(String featureName, String paramName) {
        String testValue = FeatureOverrides.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return Double.valueOf(testValue);
        String defaultValueInTests = getParamDefaultValueInTests(featureName, paramName);
        if (defaultValueInTests != null) return Double.valueOf(defaultValueInTests);
        if (FeatureList.getDisableNativeForTesting()) throwException(featureName, paramName);
        ensureNativeMapInit();
        String defaultValue = getParamDefaultValue(featureName, paramName);
        return FeatureMapJni.get()
                .getFieldTrialParamByFeatureAsDouble(
                        mNativeMapPtr, featureName, paramName, Double.valueOf(defaultValue));
    }

    /**
     * Returns a field trial param as a String for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get as a String.
     * @param defaultValue The String value to use if the param is not available.
     * @return The parameter value as a String. Default value if the feature does not exist or the
     *     specified parameter does not exist.
     * @deprecated This should only be used in tests. In production, use {@link
     *     FeatureMap#getFieldTrialParamByFeatureAsString(String, String)} instead.
     */
    public String getFieldTrialParamByFeatureAsString(
            String featureName, String paramName, String defaultValue) {
        String testValue = FeatureOverrides.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return testValue;
        if (FeatureList.getDisableNativeForTesting()) return defaultValue;
        ensureNativeMapInit();
        return FeatureMapJni.get()
                .getFieldTrialParamByFeatureAsString(
                        mNativeMapPtr, featureName, paramName, defaultValue);
    }

    /**
     * Returns a field trial param as a String for the specified feature.
     *
     * @param featureName The name of the feature to retrieve a param for.
     * @param paramName The name of the param for which to get value for.
     * @return The parameter value as a String. Default value from {@link
     *     FeatureMap#getParamsDefaultValues()} if the feature does not exist or the specified
     *     parameter does not exist.
     */
    public String getFieldTrialParamByFeatureAsString(String featureName, String paramName) {
        String testValue = FeatureOverrides.getTestValueForFieldTrialParam(featureName, paramName);
        if (testValue != null) return testValue;
        String defaultValueInTests = getParamDefaultValueInTests(featureName, paramName);
        if (defaultValueInTests != null) return defaultValueInTests;
        if (FeatureList.getDisableNativeForTesting()) throwException(featureName, paramName);
        ensureNativeMapInit();
        String defaultValue = getParamDefaultValue(featureName, paramName);
        return FeatureMapJni.get()
                .getFieldTrialParamByFeatureAsString(
                        mNativeMapPtr, featureName, paramName, defaultValue);
    }

    /** Returns all the field trial parameters for the specified feature. */
    public Map<String, String> getFieldTrialParamsForFeature(String featureName) {
        Map<String, String> testValues =
                FeatureOverrides.getTestValuesForAllFieldTrialParamsForFeature(featureName);
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

        @JniType("std::string")
        String getFieldTrialParamByFeatureAsString(
                long featureMap,
                @JniType("std::string") String featureName,
                @JniType("std::string") String paramName,
                @JniType("std::string") String defaultValue);

        @JniType("std::vector<std::string>")
        String[] getFlattedFieldTrialParamsForFeature(
                long featureMap, @JniType("std::string") String featureName);
    }
}
