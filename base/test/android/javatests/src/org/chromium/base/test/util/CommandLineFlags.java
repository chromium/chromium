// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.CommandLine;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.Log;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;

import java.lang.annotation.Annotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Inherited;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Provides annotations for setting command-line flags. Enabled by default for Robolectric and
 * on-device tests.
 */
public final class CommandLineFlags {
    private static final String TAG = "CommandLineFlags";
    private static final String DISABLE_FEATURES = "disable-features";
    private static final String ENABLE_FEATURES = "enable-features";
    private static final String FORCE_FIELDTRIALS = "force-fieldtrials";
    private static final String FORCE_FIELDTRIAL_PARAMS = "force-fieldtrial-params";
    private static final Set<String> MERGED_FLAGS =
            Set.of(ENABLE_FEATURES, DISABLE_FEATURES, FORCE_FIELDTRIALS, FORCE_FIELDTRIAL_PARAMS);

    // Features set by original command-line --enable-features / --disable-features.
    private static FieldTrials sOrigFieldTrials = new FieldTrials();
    private static final Map<String, String> sActiveFlagPrevValues = new HashMap<>();

    /** Adds command-line flags to the {@link org.chromium.base.CommandLine} for this test. */
    @Inherited
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD, ElementType.TYPE})
    public @interface Add {
        String[] value();
    }

    /**
     * Removes command-line flags from the {@link org.chromium.base.CommandLine} from this test.
     *
     * Note that this can only be applied to test methods. This restriction is due to complexities
     * in resolving the order that annotations are applied, and given how rare it is to need to
     * remove command line flags, this annotation must be applied directly to each test method
     * wishing to remove a flag.
     */
    @Inherited
    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.METHOD})
    public @interface Remove {
        String[] value();
    }

    public static void ensureInitialized() {
        if (!CommandLine.isInitialized()) {
            // Override in a persistent way so that if command-line is re-initialized by code
            // under-test, it will still use the test flags file.
            CommandLineInitUtil.setFilenameOverrideForTesting(getTestCmdLineFile());
            CommandLineInitUtil.initCommandLine(null, () -> true);

            // Store features from initial command-line into |sOrigFieldTrials| for proper merging
            // later.
            CommandLine commandLine = CommandLine.getInstance();
            List<String> initialFlags = new ArrayList<>();
            for (var e : commandLine.getSwitches().entrySet()) {
                initialFlags.add(e.getKey() + "=" + e.getValue());
            }
            separateFlagsIntoFieldTrialsAndOtherFlags(
                    initialFlags, sOrigFieldTrials, new ArrayList<>());
        }
    }

    private static void processAnnotationsIntoFlags(
            Annotation[] annotations, List<String> outputFlags) {
        for (Annotation annotation : annotations) {
            if (annotation instanceof CommandLineFlags.Add addAnnotation) {
                Collections.addAll(outputFlags, addAnnotation.value());
            } else if (annotation instanceof CommandLineFlags.Remove removeAnnotation) {
                outputFlags.removeAll(Arrays.asList(removeAnnotation.value()));
            } else if (annotation instanceof EnableFeatures enableAnnotation) {
                for (String featureSpec : enableAnnotation.value()) {
                    outputFlags.add(ENABLE_FEATURES + "=" + featureSpec);
                }
            } else if (annotation instanceof DisableFeatures disableAnnotation) {
                for (String featureSpec : disableAnnotation.value()) {
                    outputFlags.add(DISABLE_FEATURES + "=" + featureSpec);
                }
            }
        }
    }

    public static void reset(
            Annotation[] classAnnotations, @Nullable Annotation[] methodAnnotations) {
        Features.resetCachedFlags();

        // Collect @CommandLineFlags, @EnableFeatures and @DisableFeatures.
        List<String> flagsFromAnnotations = new ArrayList<>();
        processAnnotationsIntoFlags(classAnnotations, flagsFromAnnotations);
        if (methodAnnotations != null) {
            processAnnotationsIntoFlags(methodAnnotations, flagsFromAnnotations);
        }

        // Synchronize |fieldTrials| with |newFlags|.
        FieldTrials fieldTrials = sOrigFieldTrials.createCopy();
        List<String> otherFlags = new ArrayList<>();
        separateFlagsIntoFieldTrialsAndOtherFlags(flagsFromAnnotations, fieldTrials, otherFlags);
        List<String> newFlags = mergeCurrentFlagsWithFieldTrials(otherFlags, fieldTrials);

        // Apply changes to the CommandLine.
        boolean anyChanges = applyChanges(newFlags);

        // Apply changes to FeatureList.
        TestValues testValues = fieldTrials.createTestValues();
        // If flags did not change, and no feature-related flags are present, then do not clobber
        // flag values so that a test can use FeatureList.setTestValues() in @BeforeClass.
        if (anyChanges || !testValues.isEmpty()) {
            // TODO(agrieve): Use ScopedFeatureList to update native feature states even after
            //     native feature list has been initialized.
            FeatureList.setTestValuesNoResetForTesting(testValues);
        }
    }

    private static boolean applyChanges(List<String> newFlags) {
        // Track and apply changes in flags (rather than clearing each time) because flags are added
        // as part of normal start-up (which need to be maintained).
        boolean anyChanges = false;
        CommandLine commandLine = CommandLine.getInstance();
        Set<String> newFlagNames = new HashSet<>();
        for (String flag : newFlags) {
            String[] keyValue = flag.split("=", 2);
            String flagName = keyValue[0];
            String flagValue = keyValue.length == 1 ? "" : keyValue[1];
            String prevValue =
                    commandLine.hasSwitch(flagName) ? commandLine.getSwitchValue(flagName) : null;
            newFlagNames.add(flagName);
            if (!flagValue.equals(prevValue)) {
                anyChanges = true;
                commandLine.appendSwitchWithValue(flagName, flagValue);
                if (!sActiveFlagPrevValues.containsKey(flagName)) {
                    sActiveFlagPrevValues.put(flagName, prevValue);
                }
            }
        }
        // Undo previously applied flags.
        for (var it = sActiveFlagPrevValues.entrySet().iterator(); it.hasNext(); ) {
            var entry = it.next();
            String flagName = entry.getKey();
            String flagValue = entry.getValue();
            if (!newFlagNames.contains(flagName)) {
                anyChanges = true;
                if (flagValue == null) {
                    commandLine.removeSwitch(flagName);
                } else {
                    commandLine.appendSwitchWithValue(flagName, flagValue);
                }
                it.remove();
            }
        }
        Log.i(
                TAG,
                "Java %scommand line set to: %s",
                CommandLine.isNativeImplementationForTesting() ? "(and native) " : "",
                serializeCommandLine());
        return anyChanges;
    }

    private static String serializeCommandLine() {
        Map<String, String> switches = CommandLine.getInstance().getSwitches();
        if (switches.isEmpty()) {
            return "";
        }
        StringBuilder sb = new StringBuilder();
        for (var entry : switches.entrySet()) {
            sb.append("--").append(entry.getKey());
            if (!TextUtils.isEmpty(entry.getValue())) {
                sb.append('=').append(entry.getValue());
            }
            sb.append(' ');
        }
        sb.setLength(sb.length() - 1);
        return sb.toString();
    }

    private static void separateFlagsIntoFieldTrialsAndOtherFlags(
            List<String> flags, FieldTrials outputFieldTrials, List<String> outputOtherFlags) {
        // Collect via a Map rather than two lists to correctly handle the a feature being enabled
        // via class flags and disabled via method flags (or vice versa).
        for (String flag : flags) {
            String[] keyValue = flag.split("=", 2);
            if (keyValue.length == 1 || keyValue[1].isEmpty()) {
                outputOtherFlags.add(flag);
                continue;
            }
            if (ENABLE_FEATURES.equals(keyValue[0])) {
                for (String featureSpec : keyValue[1].split(",")) {
                    outputFieldTrials.incorporateEnableFeaturesFlag(featureSpec);
                }
            } else if (DISABLE_FEATURES.equals(keyValue[0])) {
                for (String feature : keyValue[1].split(",")) {
                    outputFieldTrials.incorporateDisableFeaturesFlag(feature);
                }
            } else if (FORCE_FIELDTRIALS.equals(keyValue[0])) {
                outputFieldTrials.incorporateForceFieldTrialsFlag(keyValue[1]);
            } else if (FORCE_FIELDTRIAL_PARAMS.equals(keyValue[0])) {
                for (String paramSpec : keyValue[1].split(",")) {
                    outputFieldTrials.incorporateForceFieldTrialParamsFlag(paramSpec);
                }
            } else {
                outputOtherFlags.add(flag);
            }
        }
    }

    private static List<String> mergeCurrentFlagsWithFieldTrials(
            List<String> otherFlags, FieldTrials fieldTrials) {
        // Copy over flags that are not merged with special handling logic below.
        List<String> newFlags = new ArrayList<>(otherFlags);

        List<String> enabledFlags = new ArrayList<>();
        List<String> disabledFlags = new ArrayList<>();
        for (var entry : fieldTrials.getFeatureFlags().entrySet()) {
            String feature = entry.getKey();
            boolean enabled = entry.getValue();
            List<String> target = enabled ? enabledFlags : disabledFlags;
            target.add(feature);
        }

        // Add --enable-features
        if (!enabledFlags.isEmpty()) {
            List<String> featureSpecs = new ArrayList<>();
            for (String feature : enabledFlags) {
                String params = "";
                String paramPairSeparator = "";
                if (fieldTrials.getFeatureToParams().containsKey(feature)) {
                    StringBuilder sb = new StringBuilder(":");
                    for (var e : fieldTrials.getFeatureToParams().get(feature).entrySet()) {
                        String param = e.getKey();
                        String paramValue = FieldTrials.escapeParam(e.getValue());
                        sb.append(paramPairSeparator).append(param).append("/").append(paramValue);
                        paramPairSeparator = "/";
                    }
                    params = sb.toString();
                }

                String fieldTrial = fieldTrials.getFeatureToTrial().getOrDefault(feature, null);

                if (fieldTrial != null) {
                    String group = fieldTrials.getTrialToGroup().get(fieldTrial);
                    featureSpecs.add(feature + "<" + fieldTrial + "." + group + params);
                } else {
                    featureSpecs.add(feature + params);
                }
            }
            newFlags.add(
                    String.format("%s=%s", ENABLE_FEATURES, TextUtils.join(",", featureSpecs)));
        }

        // Add --disable-features
        if (!disabledFlags.isEmpty()) {
            newFlags.add(
                    String.format("%s=%s", DISABLE_FEATURES, TextUtils.join(",", disabledFlags)));
        }

        // Add --force-fieldtrial-params
        if (!fieldTrials.getTrialAndGroupToParams().isEmpty()) {
            StringBuilder sb = new StringBuilder();
            String trialSeparator = "";
            for (var e : fieldTrials.getTrialAndGroupToParams().entrySet()) {
                String trialAndGroup = e.getKey();
                Map<String, String> paramNameToValue = e.getValue();
                if (paramNameToValue.isEmpty()) {
                    throw new IllegalStateException("No params for trial " + trialAndGroup);
                }
                sb.append(trialSeparator).append(trialAndGroup).append(":");
                trialSeparator = ",";

                String paramPairSeparator = "";
                for (var f : paramNameToValue.entrySet()) {
                    String param = f.getKey();
                    String paramValue = FieldTrials.escapeParam(f.getValue());
                    sb.append(paramPairSeparator).append(param).append("/").append(paramValue);
                    paramPairSeparator = "/";
                }
            }

            newFlags.add(String.format("%s=%s", FORCE_FIELDTRIAL_PARAMS, sb));
        }

        // Add --force-fieldtrials
        if (!fieldTrials.getTrialToGroup().isEmpty()) {
            StringBuilder sb = new StringBuilder();
            String trialSeparator = "";
            for (var e : fieldTrials.getTrialToGroup().entrySet()) {
                String trial = e.getKey();
                String group = e.getValue();
                sb.append(trialSeparator).append(trial).append("/").append(group);
                trialSeparator = ",";
            }

            newFlags.add(String.format("%s=%s", FORCE_FIELDTRIALS, sb));
        }

        return newFlags;
    }

    private CommandLineFlags() {}

    public static String getTestCmdLineFile() {
        return "test-cmdline-file";
    }
}
