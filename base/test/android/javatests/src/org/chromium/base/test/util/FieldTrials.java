// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.cached_flags.CachedFlagsSharedPreferences;
import org.chromium.base.cached_flags.ValuesOverridden;
import org.chromium.base.cached_flags.ValuesReturned;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Helps with setting Field Trial parameters during instrumentation tests. It parses the field
 * trials info from CommandLine, and applies the overrides to CachedFlag.
 */
public class FieldTrials {
    // TODO(crbug.com/40257556): Allow setting field trial via annotation.

    private static FieldTrials sInstance;
    private final Map<String, Map<String, String>> mTrialToParamValueMap = new HashMap<>();
    private final Map<String, Set<String>> mTrialToFeatureNameMap = new HashMap<>();

    private FieldTrials() {}

    public static FieldTrials getInstance() {
        if (sInstance == null) sInstance = new FieldTrials();
        return sInstance;
    }

    private static String cleanupFeatureName(String featureNameWithTrial) {
        if (!featureNameWithTrial.contains("<")) return featureNameWithTrial;
        return featureNameWithTrial.split("<")[0];
    }

    /**
     * Builds a map for each trial to a set of <param, value> pairs.
     * @param fieldTrialParams The format is: {"Trial1.Group1:param1/value1/param2/value2",
     *                         "Trial2.Group2:param3/value3"}
     */
    private void updateTrialToParamValueMap(String[] fieldTrialParams) throws Exception {
        for (String fieldTrialParam : fieldTrialParams) {
            // The format of {@link fieldTrialParam} is:
            // "Trial1.Group1:param1/value1/param2/value2".
            int separatorIndex = fieldTrialParam.indexOf(".");
            if (separatorIndex == -1) {
                throw new Exception(
                        "The trial name and group name should be" + " separated by a '.'.");
            }

            String trialName = fieldTrialParam.substring(0, separatorIndex);
            String[] groupParamPairs = fieldTrialParam.substring(separatorIndex + 1).split(":");
            if (groupParamPairs.length != 2) {
                throw new Exception(
                        "The group name and field trial parameters"
                                + " should be separated by a ':'.");
            }

            String[] paramValuePair = groupParamPairs[1].split("/");
            if (paramValuePair.length % 2 != 0) {
                throw new Exception(
                        "The param and value of the field trial group:"
                                + trialName
                                + "."
                                + groupParamPairs[0]
                                + " isn't paired up!");
            }

            Map<String, String> paramValueMap = mTrialToParamValueMap.get(trialName);
            if (paramValueMap == null) {
                paramValueMap = new HashMap<>();
                mTrialToParamValueMap.put(trialName, paramValueMap);
            }
            for (int count = 0; count < paramValuePair.length; count += 2) {
                paramValueMap.put(paramValuePair[count], paramValuePair[count + 1]);
            }
        }
    }

    /**
     * Builds a map for each trial to a set of features.
     * @param trialGroups    The format is {"Trial1", "Group1", "Trial2", "Group2"}
     * @param enableFeatures The format is {"Feature1<Trial1", "Feature2", "Feature3<Trial2"}
     */
    private void updateTrialFeatureMap(String[] trialGroups, Set<String> enableFeatures)
            throws Exception {
        if (trialGroups.length % 2 != 0) {
            throw new Exception("The field trial and group info aren't paired up!");
        }

        for (String enableFeature : enableFeatures) {
            String[] featureTrial = enableFeature.split("<");
            if (featureTrial.length < 2) continue;

            String featureName = featureTrial[0];
            String trialName = featureTrial[1];
            Set<String> featureSet = mTrialToFeatureNameMap.get(trialName);
            if (featureSet == null) {
                featureSet = new HashSet<>();
                mTrialToFeatureNameMap.put(trialName, featureSet);
            }
            featureSet.add(featureName);
        }
    }

    private void updateCachedFlags() {
        for (Map.Entry<String, Map<String, String>> entry : mTrialToParamValueMap.entrySet()) {
            String trialName = entry.getKey();
            Set<String> featureSet = mTrialToFeatureNameMap.get(trialName);
            if (featureSet == null) continue;
            for (String featureName : featureSet) {
                Map<String, String> params = entry.getValue();

                // Override value for each CachedFieldTrialParameter
                for (Map.Entry<String, String> param : params.entrySet()) {
                    String variationName = param.getKey();
                    String preferenceKey =
                            CachedFlagsSharedPreferences.generateParamSharedPreferenceKey(
                                    featureName, variationName);
                    String overrideValue = param.getValue();
                    ValuesOverridden.setOverrideForTesting(preferenceKey, overrideValue);
                }

                // Override value for AllCachedFieldTrialParameters
                String allParamsPreferenceKey =
                        CachedFlagsSharedPreferences.generateParamSharedPreferenceKey(
                                featureName, "");
                String allParamsOverrideValue = CachedFlagsSharedPreferences.encodeParams(params);
                ValuesOverridden.setOverrideForTesting(
                        allParamsPreferenceKey, allParamsOverrideValue);
            }
        }
    }

    /**
     * Applies the <feature, param, value> info to CachedFeatureFlags, and enables these features in
     * CachedFeatureFlags.
     */
    public void applyFieldTrials(CommandLine commandLine) {
        String forceFieldTrials = commandLine.getSwitchValue(BaseSwitches.FORCE_FIELD_TRIALS);
        String forceFieldTrialParams =
                commandLine.getSwitchValue(BaseSwitches.FORCE_FIELD_TRIAL_PARAMS);
        String enableFeatures = commandLine.getSwitchValue(BaseSwitches.ENABLE_FEATURES);

        Set<String> enableFeaturesSet = new HashSet<>();
        if (enableFeatures != null) {
            Collections.addAll(enableFeaturesSet, enableFeatures.split(","));

            Map<String, Boolean> enabledFeaturesMap = new HashMap<>();
            for (String enabledFeature : enableFeaturesSet) {
                enabledFeaturesMap.put(cleanupFeatureName(enabledFeature), true);
            }
            ValuesReturned.setFeaturesForTesting(enabledFeaturesMap);
        }

        if (forceFieldTrials == null || forceFieldTrialParams == null || enableFeatures == null) {
            return;
        }

        try {
            updateTrialToParamValueMap(forceFieldTrialParams.split(","));
            updateTrialFeatureMap(forceFieldTrials.split("/"), enableFeaturesSet);
            updateCachedFlags();
        } catch (Exception e) {
            assert false
                    : e.toString()
                            + "\n"
                            + "The format of field trials parameters declared isn't correct:"
                            + BaseSwitches.FORCE_FIELD_TRIALS
                            + "="
                            + forceFieldTrials
                            + ", "
                            + BaseSwitches.FORCE_FIELD_TRIAL_PARAMS
                            + "="
                            + forceFieldTrialParams
                            + ", "
                            + BaseSwitches.ENABLE_FEATURES
                            + "="
                            + enableFeatures
                            + ".";
        }
    }

    void reset() {
        mTrialToFeatureNameMap.clear();
        mTrialToParamValueMap.clear();
        sInstance = null;
    }
}
