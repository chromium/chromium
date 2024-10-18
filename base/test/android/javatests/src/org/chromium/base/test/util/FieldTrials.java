// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.util.ArrayMap;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.FeatureList;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * Helps with setting Field Trial parameters during tests. It parses the field trials arguments from
 * CommandLine, and applies the overrides to CachedFieldTrialParameters.
 *
 * <p>TODO(crbug.com/372962793): Override non-cached field trial params at Java level for
 * FeatureMap#getFieldTrialParamByFeature() in unit tests. In instrumentation tests, native does the
 * override by parsing the CommandLine.
 */
public class FieldTrials {
    // TODO(crbug.com/40257556): Allow setting field trial via a simpler annotation.
    // Even though this format is unnecessarily complicated for tests, it is the one that native
    // honors so we perform some checks to catch incorrect formats. For another annotation to be
    // applied in native, it needs to add arguments to the CommandLine.

    private FieldTrials() {}

    /**
     * Applies the <feature, param, value> info to CachedFeatureFlags, and enables these features in
     * CachedFeatureFlags.
     */
    public static void applyFieldTrialsParams(
            CommandLine commandLine, Map<String, Set<String>> trialToFeatures) {
        String forceFieldTrials = commandLine.getSwitchValue(BaseSwitches.FORCE_FIELD_TRIALS);
        String forceFieldTrialParams =
                commandLine.getSwitchValue(BaseSwitches.FORCE_FIELD_TRIAL_PARAMS);

        if (forceFieldTrials == null
                || forceFieldTrialParams == null
                || trialToFeatures.isEmpty()) {
            return;
        }

        try {
            validateForceFieldTrials(forceFieldTrials);
            Map<String, Map<String, String>> trialToParamValueMap =
                    parseForceFieldTrialParams(forceFieldTrialParams);
            overrideCachedFieldTrialParams(trialToFeatures, trialToParamValueMap);
        } catch (Exception e) {
            throw new IllegalArgumentException(
                    "The format of field trials parameters declared isn't correct:"
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
                            + commandLine.getSwitchValue(BaseSwitches.ENABLE_FEATURES)
                            + ".",
                    e);
        }
    }

    /**
     * Builds a map for each trial to a set of <param, value> pairs.
     *
     * @param forceFieldTrialParams The format is: "Trial1.Group1:param1/value1/param2/value2,
     *     Trial2.Group2:param3/value3"
     * @return {"Trial1": {"param1": "value1", "param2": "value2"}, "Trial2": {"param3": "value3"}}
     */
    private static Map<String, Map<String, String>> parseForceFieldTrialParams(
            String forceFieldTrialParams) throws Exception {
        String[] fieldTrialParams = forceFieldTrialParams.split(",");
        Map<String, Map<String, String>> trialToParamValueMap = new ArrayMap<>();

        for (String fieldTrialParam : fieldTrialParams) {
            // The format of each entry in {@link fieldTrialParams} is:
            // "Trial1.Group1:param1/value1/param2/value2".
            int separatorIndex = fieldTrialParam.indexOf(".");
            if (separatorIndex == -1) {
                throw new IllegalArgumentException(
                        String.format(
                                "The trial name and group name should be separated by a '.' in"
                                        + " '%s'.",
                                fieldTrialParam));
            }

            String trialName = fieldTrialParam.substring(0, separatorIndex);
            String[] groupParamPairs = fieldTrialParam.substring(separatorIndex + 1).split(":");
            if (groupParamPairs.length != 2) {
                throw new Exception(
                        "The group name and field trial parameters"
                                + " should be separated by a ':'.");
            }

            // TODO(crbug.com/40257556): Multiple groups for the same field trial are not supported;
            // assume that only
            // one group for the trial; tests don't need multiple groups. |groupParamPairs[0]| is
            // the group.
            String[] paramValuePairs = groupParamPairs[1].split("/");
            if (paramValuePairs.length % 2 != 0) {
                throw new Exception(
                        "The param and value of the field trial group:"
                                + trialName
                                + "."
                                + groupParamPairs[0]
                                + " isn't paired up!");
            }

            Map<String, String> paramValueMap =
                    trialToParamValueMap.computeIfAbsent(trialName, key -> new HashMap<>());
            for (int count = 0; count < paramValuePairs.length; count += 2) {
                paramValueMap.put(paramValuePairs[count], paramValuePairs[count + 1]);
            }
        }
        return trialToParamValueMap;
    }

    private static void validateForceFieldTrials(String forceFieldTrials) {
        String[] trialGroups = forceFieldTrials.split("/");
        if (trialGroups.length % 2 != 0) {
            throw new IllegalArgumentException("The field trial and group info aren't paired up!");
        }
    }

    private static void overrideCachedFieldTrialParams(
            Map<String, Set<String>> trialToFeatures,
            Map<String, Map<String, String>> trialToParamValueMap) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        for (Map.Entry<String, Map<String, String>> entry : trialToParamValueMap.entrySet()) {
            String trialName = entry.getKey();
            Set<String> featureSet = trialToFeatures.get(trialName);
            if (featureSet == null) {
                throw new IllegalArgumentException(
                        String.format(
                                "No Feature associated to field trial %s, did you forget"
                                        + " '--enable-features=[Feature]<%s'?",
                                trialName, trialName));
            }
            for (String featureName : featureSet) {
                Map<String, String> params = entry.getValue();

                if (params.isEmpty()) {
                    throw new IllegalArgumentException(
                            String.format(
                                    "No field trial param associated to %s<%s, did you forget"
                                            + " '--force-field-trials=%s<[Group]'?",
                                    featureName, trialName, trialName));
                }

                // Override value for each CachedFieldTrialParameter
                for (Map.Entry<String, String> param : params.entrySet()) {
                    String paramName = param.getKey();
                    String overrideValue = param.getValue();
                    testValues.addFieldTrialParamOverride(featureName, paramName, overrideValue);
                }
            }
        }

        FeatureList.mergeTestValues(testValues, /* replace= */ true);
    }
}
