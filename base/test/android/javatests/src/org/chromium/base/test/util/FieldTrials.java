// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.util.ArraySet;

import org.chromium.base.FeatureList;
import org.chromium.base.Log;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Test overrides for feature flags, field trials, feature params and field trial params.
 *
 * <p>Collected from command line flags and indirectly from annotations.
 *
 * <p>Applied to CommandLine and FeatureList.
 */
class FieldTrials {
    private static final String TAG = "FieldTrials";

    private final Map<String, Boolean> mFeatureFlags = new HashMap<>();
    private final Map<String, Boolean> mUnmodifiableFeatureFlags =
            Collections.unmodifiableMap(mFeatureFlags);
    private final Map<String, Map<String, String>> mFeatureToParams = new HashMap<>();
    private final Map<String, Map<String, String>> mUnmodifiableFeatureToParams =
            Collections.unmodifiableMap(mFeatureToParams);
    private final Map<String, String> mTrialToGroup = new HashMap<>();
    private final Map<String, String> mUnmodifiableTrialToGroup =
            Collections.unmodifiableMap(mTrialToGroup);
    private final Map<String, Map<String, String>> mTrialAndGroupToParams = new HashMap<>();
    private final Map<String, Map<String, String>> mUnmodifiableTrialAndGroupToParams =
            Collections.unmodifiableMap(mTrialAndGroupToParams);
    private final Map<String, Set<String>> mTrialToFeatures = new HashMap<>();
    private final Map<String, String> mFeatureToTrial = new HashMap<>();
    private final Map<String, String> mUnmodifiableFeatureToTrial =
            Collections.unmodifiableMap(mFeatureToTrial);

    public FieldTrials() {}

    Map<String, Boolean> getFeatureFlags() {
        return mUnmodifiableFeatureFlags;
    }

    Map<String, Map<String, String>> getFeatureToParams() {
        return mUnmodifiableFeatureToParams;
    }

    Map<String, String> getTrialToGroup() {
        return mUnmodifiableTrialToGroup;
    }

    Map<String, Map<String, String>> getTrialAndGroupToParams() {
        return mUnmodifiableTrialAndGroupToParams;
    }

    Map<String, String> getFeatureToTrial() {
        return mUnmodifiableFeatureToTrial;
    }

    FeatureList.TestValues createTestValues() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        for (var e : mFeatureFlags.entrySet()) {
            String feature = e.getKey();
            Boolean featureValue = e.getValue();
            testValues.addFeatureFlagOverride(feature, featureValue);
        }

        for (var e : mFeatureToParams.entrySet()) {
            String feature = e.getKey();
            Map<String, String> featureToParams = e.getValue();
            for (var f : featureToParams.entrySet()) {
                String param = f.getKey();
                String paramValue = f.getValue();
                testValues.addFieldTrialParamOverride(feature, param, paramValue);
            }
        }

        // Support deprecated "--force-fieldtrial-params=MyStudy.Group1:x/100/y/Test"
        for (var e : mTrialAndGroupToParams.entrySet()) {
            String trialAndGroup = e.getKey();
            String[] parts = trialAndGroup.split("\\.");
            if (parts.length != 2) {
                throw new IllegalArgumentException(
                        String.format("trialAndGroup is %s", trialAndGroup));
            }
            String trial = parts[0];
            String group = parts[1];
            Map<String, String> trialParams = e.getValue();
            if (!mTrialToGroup.containsKey(trial)) {
                throw new IllegalArgumentException(
                        "Did you forget force-fieldtrials=" + trialAndGroup + "?");
            }
            if (!mTrialToGroup.get(trial).equals(group)) {
                throw new IllegalArgumentException(
                        String.format(
                                "Trial %s already forced to group %s, did you forget"
                                        + " force-fieldtrials=%s?",
                                trial, mTrialToGroup.get(trial), trialAndGroup));
            }
            if (mTrialToFeatures.containsKey(trial)) {
                for (String feature : mTrialToFeatures.get(trial)) {
                    for (var f : trialParams.entrySet()) {
                        String param = f.getKey();
                        String paramValue = f.getValue();
                        testValues.addFieldTrialParamOverride(feature, param, paramValue);
                    }
                }
            } else {
                Log.w(TAG, "Did you forget enable-features=FeatureName<%s?", trial);
            }
        }
        return testValues;
    }

    FieldTrials createCopy() {
        FieldTrials copy = new FieldTrials();
        copy.mFeatureFlags.putAll(mFeatureFlags);
        for (Map.Entry<String, Map<String, String>> e : mFeatureToParams.entrySet()) {
            copy.mFeatureToParams.put(e.getKey(), new HashMap<>(e.getValue()));
        }
        copy.mTrialToGroup.putAll(mTrialToGroup);
        for (Map.Entry<String, Map<String, String>> e : mTrialAndGroupToParams.entrySet()) {
            copy.mTrialAndGroupToParams.put(e.getKey(), new HashMap<>(e.getValue()));
        }
        for (Map.Entry<String, Set<String>> e : mTrialToFeatures.entrySet()) {
            copy.mTrialToFeatures.put(e.getKey(), new HashSet<>(e.getValue()));
        }
        copy.mFeatureToTrial.putAll(mFeatureToTrial);
        return copy;
    }

    /**
     * Returns a multi-line representation of the FieldTrials.
     *
     * <p>The format returned is:
     *
     * <pre>
     * FieldTrials {
     *   mFeatureFlags {
     *     FeatureA: true
     *     FeatureB: true
     *     FeatureC: true
     *     FeatureD: false
     *   }
     *   mFeatureToParams {
     *      FeatureA:
     *       Param1: Value1
     *   }
     *   mTrialToGroup {
     *     Study: Group
     *   }
     *   mTrialAndGroupToParams {
     *     Study.Group:
     *       Param2: Value2
     *       Param3: Value3
     *   }
     *   mTrialToFeatures {
     *     Study: FeatureB, FeatureC
     *   }
     *   mFeatureToTrial {
     *     FeatureB: Study
     *     FeatureC: Study
     *   }
     * }
     *  </pre>
     */
    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder("FieldTrials {\n");

        sb.append("  mFeatureFlags {\n");
        for (var e : mFeatureFlags.entrySet()) {
            sb.append("    ").append(e.getKey()).append(": ").append(e.getValue()).append("\n");
        }
        sb.append("  }\n");

        sb.append("  mFeatureToParams {\n");
        for (var e : mFeatureToParams.entrySet()) {
            sb.append("    ").append(e.getKey()).append(":\n");
            for (var f : e.getValue().entrySet()) {
                sb.append("      ")
                        .append(f.getKey())
                        .append(": ")
                        .append(f.getValue())
                        .append("\n");
            }
        }
        sb.append("  }\n");

        sb.append("  mTrialToGroup {\n");
        for (var e : mTrialToGroup.entrySet()) {
            sb.append("    ").append(e.getKey()).append(": ").append(e.getValue()).append("\n");
        }
        sb.append("  }\n");

        sb.append("  mTrialAndGroupToParams {\n");
        for (var e : mTrialAndGroupToParams.entrySet()) {
            sb.append("    ").append(e.getKey()).append(":\n");
            for (var f : e.getValue().entrySet()) {
                sb.append("      ")
                        .append(f.getKey())
                        .append(": ")
                        .append(f.getValue())
                        .append("\n");
            }
        }
        sb.append("  }\n");

        sb.append("  mTrialToFeatures {\n");
        for (var e : mTrialToFeatures.entrySet()) {
            sb.append("    ")
                    .append(e.getKey())
                    .append(": ")
                    .append(String.join(", ", e.getValue()))
                    .append("\n");
        }
        sb.append("  }\n");

        sb.append("  mFeatureToTrial {\n");
        for (var e : mFeatureToTrial.entrySet()) {
            sb.append("    ").append(e.getKey()).append(": ").append(e.getValue()).append("\n");
        }
        sb.append("  }\n");

        sb.append("}");
        return sb.toString();
    }

    /**
     * Incorporates a raw feature spec argument passed to --enable-features which can look like:
     *
     * <ul>
     *   <li>FeatureName
     *   <li>FeatureName<TrialName
     *   <li>FeatureName<TrialName.GroupName
     *   <li>FeatureName:Param1/Value1/Param2/Value2
     *   <li>FeatureName<TrialName:Param1/Value1/Param2/Value2
     *   <li>FeatureName<TrialName.GroupName:Param1/Value1/Param2/Value2
     * </ul>
     */
    void incorporateEnableFeaturesFlag(String featureSpec) {
        String featureSpecMinusParams;
        Map<String, String> params;
        if (featureSpec.contains(":")) {
            String[] parts = featureSpec.split(":", 2);
            featureSpecMinusParams = parts[0];
            params = parseParamNameValuePairs(parts[1]);
        } else {
            featureSpecMinusParams = featureSpec;
            params = Collections.emptyMap();
        }

        String feature;
        if (featureSpecMinusParams.contains("<")) {
            String[] parts = featureSpec.split("<", 2);
            feature = parts[0];
            String fieldTrialAndGroup = parts[1];
            String fieldTrial;
            if (fieldTrialAndGroup.contains(".")) {
                String[] fieldTrialParts = fieldTrialAndGroup.split("\\.", 2);
                fieldTrial = fieldTrialParts[0];
                //                String fieldTrialGroup = fieldTrialParts[1];
            } else {
                fieldTrial = fieldTrialAndGroup;
            }
            associateFeatureToFieldTrial(feature, fieldTrial);
        } else {
            feature = featureSpecMinusParams;
        }
        addFeatureFlag(feature, true);

        for (var e : params.entrySet()) {
            String param = e.getKey();
            String paramValue = e.getValue();
            addFeatureParam(feature, param, paramValue);
        }
    }

    /**
     * Incorporates a feature name argument passed to --disable-features.
     *
     * @param feature The format is: "Feature1"
     */
    void incorporateDisableFeaturesFlag(String feature) {
        addFeatureFlag(feature, false);
    }

    /**
     * Incorporates a raw force fieldtrials argument passed to --force-fieldtrials.
     *
     * @param forceFieldTrials The format is: "Trial1/Group1/Trial2/Group2"
     */
    void incorporateForceFieldTrialsFlag(String forceFieldTrials) {
        String[] trialGroups = forceFieldTrials.split("/");
        if (trialGroups.length % 2 != 0) {
            throw new IllegalArgumentException("The field trial and group info aren't paired up!");
        }
        for (int i = 0; i < trialGroups.length; i += 2) {
            mTrialToGroup.put(trialGroups[i], trialGroups[i + 1]);
        }
    }

    /**
     * Incorporates a raw force fieldtrial params argument passed to --force-fieldtrial-params.
     *
     * @param fieldTrialParam The format is: "Trial1.Group1:param1/value1/param2/value2"
     */
    void incorporateForceFieldTrialParamsFlag(String fieldTrialParam) {
        if (!fieldTrialParam.contains(".")) {
            throw new IllegalArgumentException(
                    String.format(
                            "The trial name and group name should be separated by a '.' in"
                                    + " '%s'.",
                            fieldTrialParam));
        }

        String[] parts = fieldTrialParam.split(":");
        if (parts.length != 2) {
            throw new IllegalArgumentException(
                    "The group name and field trial parameters" + " should be separated by a ':'.");
        }
        String trialAndGroup = parts[0];
        String params = parts[1];

        Map<String, String> paramNameToValue = parseParamNameValuePairs(params);
        for (var e : paramNameToValue.entrySet()) {
            String param = e.getKey();
            String paramValue = e.getValue();
            addTrialGroupParam(trialAndGroup, param, paramValue);
        }
    }

    /** Unescapes reserved characters from a param value. */
    static String unescapeParam(String value) {
        return value.replace("%2C", ",")
                .replace("%2E", ".")
                .replace("%2F", "/")
                .replace("%3A", ":")
                .replace("%25", "%");
    }

    /** Escapes reserved characters from a param value. */
    static String escapeParam(String value) {
        return value.replace("%", "%25")
                .replace(":", "%3A")
                .replace("/", "%2F")
                .replace(".", "%2E")
                .replace(",", "%2C");
    }

    private void addFeatureFlag(String feature, Boolean value) {
        mFeatureFlags.put(feature, value);
    }

    private void associateFeatureToFieldTrial(String feature, String fieldTrial) {
        assert !fieldTrial.contains("/");
        mFeatureToTrial.put(feature, fieldTrial);
        mTrialToFeatures.computeIfAbsent(fieldTrial, key -> new ArraySet<>()).add(feature);
    }

    private void addFeatureParam(String feature, String param, String paramValue) {
        Map<String, String> paramValueMap =
                mFeatureToParams.computeIfAbsent(feature, key -> new HashMap<>());
        paramValueMap.put(param, paramValue);
    }

    private void addTrialGroupParam(String trialAndGroup, String param, String paramValue) {
        assert trialAndGroup.contains(".");
        Map<String, String> paramValueMap =
                mTrialAndGroupToParams.computeIfAbsent(trialAndGroup, key -> new HashMap<>());
        paramValueMap.put(param, paramValue);
    }

    private static Map<String, String> parseParamNameValuePairs(String rawParams) {
        Map<String, String> resultParams = new HashMap<>();
        String[] paramValuePairs = rawParams.split("/");
        if (paramValuePairs.length % 2 != 0) {
            throw new IllegalArgumentException(
                    "The param names and value aren't paired up: "
                            + rawParams
                            + " isn't paired up!");
        }

        for (int count = 0; count < paramValuePairs.length; count += 2) {
            String paramName = paramValuePairs[count];
            String paramValue = unescapeParam(paramValuePairs[count + 1]);
            resultParams.put(paramName, paramValue);
        }

        return resultParams;
    }
}
