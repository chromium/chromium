// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.CommandLine;
import org.chromium.base.CommandLineInitUtil;
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
    // Features set by original command-line --enable-features / --disable-features.
    private static Map<String, Boolean> sOrigFeatures = Collections.emptyMap();
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
            // Store features from initial command-line for proper merging later.
            CommandLine commandLine = CommandLine.getInstance();
            String origEnabledFeatures = commandLine.getSwitchValue(ENABLE_FEATURES, "");
            String origDisabledFeatures = commandLine.getSwitchValue(ENABLE_FEATURES, "");
            sOrigFeatures =
                    collectFeaturesFromFlags(
                            List.of(
                                    ENABLE_FEATURES + "=" + origEnabledFeatures,
                                    DISABLE_FEATURES + "=" + origDisabledFeatures));
        }
    }

    private static void processAnnotations(Annotation[] annotations, List<String> flags) {
        for (Annotation annotation : annotations) {
            if (annotation instanceof CommandLineFlags.Add addAnnotation) {
                Collections.addAll(flags, addAnnotation.value());
            } else if (annotation instanceof CommandLineFlags.Remove removeAnnotation) {
                flags.removeAll(Arrays.asList(removeAnnotation.value()));
            } else if (annotation instanceof EnableFeatures) {
                for (String featureName : ((EnableFeatures) annotation).value()) {
                    flags.add(ENABLE_FEATURES + "=" + featureName);
                }
            } else if (annotation instanceof DisableFeatures) {
                for (String featureName : ((DisableFeatures) annotation).value()) {
                    flags.add(DISABLE_FEATURES + "=" + featureName);
                }
            }
        }
    }

    public static void reset(
            Annotation[] classAnnotations, @Nullable Annotation[] methodAnnotations) {
        Features.resetCachedFlags();
        List<String> newFlags = new ArrayList<>();
        processAnnotations(classAnnotations, newFlags);
        if (methodAnnotations != null) {
            processAnnotations(methodAnnotations, newFlags);
        }
        Map<String, Boolean> flagStates = collectFeaturesFromFlags(newFlags);
        newFlags = updateFeatureFlags(newFlags, flagStates);
        boolean anyChanges = applyChanges(newFlags);
        // If flags did not change, and no feature-related flags are present, then do not clobber
        // flag values so that a test can use FeatureList.setTestValues() in @BeforeClass.
        if (anyChanges || !flagStates.isEmpty()) {
            Features.reset(flagStates);
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

    private static Map<String, Boolean> collectFeaturesFromFlags(List<String> flags) {
        // Collect via a Map rather than two lists to correctly handle the a feature being enabled
        // via class flags and disabled via method flags (or vice versa).
        Map<String, Boolean> flagStates = new HashMap<>(sOrigFeatures);
        for (String flag : flags) {
            String[] keyValue = flag.split("=", 2);
            boolean enable = ENABLE_FEATURES.equals(keyValue[0]);
            if (!enable && !DISABLE_FEATURES.equals(keyValue[0])) {
                continue;
            }
            if (keyValue.length == 1 || keyValue[1].isEmpty()) {
                continue;
            }
            for (String featureName : keyValue[1].split(",")) {
                flagStates.put(featureName, enable);
            }
        }
        return flagStates;
    }

    private static List<String> updateFeatureFlags(
            List<String> curFlags, Map<String, Boolean> flagStates) {
        List<String> newFlags = new ArrayList<>();
        for (String flag : curFlags) {
            String flagName = flag.split("=", 2)[0];
            if (!ENABLE_FEATURES.equals(flagName) && !DISABLE_FEATURES.equals(flagName)) {
                newFlags.add(flag);
            }
        }

        List<String> enabledFlags = new ArrayList<>();
        List<String> disabledFlags = new ArrayList<>();
        for (var entry : flagStates.entrySet()) {
            var target = entry.getValue() ? enabledFlags : disabledFlags;
            target.add(entry.getKey());
        }
        if (!enabledFlags.isEmpty()) {
            newFlags.add(
                    String.format("%s=%s", ENABLE_FEATURES, TextUtils.join(",", enabledFlags)));
        }
        if (!disabledFlags.isEmpty()) {
            newFlags.add(
                    String.format("%s=%s", DISABLE_FEATURES, TextUtils.join(",", disabledFlags)));
        }
        return newFlags;
    }

    private CommandLineFlags() {}

    public static String getTestCmdLineFile() {
        return "test-cmdline-file";
    }
}
