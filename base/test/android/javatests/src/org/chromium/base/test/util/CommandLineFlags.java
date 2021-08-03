// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.app.Activity;
import android.text.TextUtils;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.Log;
import org.chromium.base.test.BaseJUnit4ClassRunner.ClassHook;
import org.chromium.base.test.BaseJUnit4ClassRunner.TestHook;

import java.lang.annotation.ElementType;
import java.lang.annotation.Inherited;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

/**
 * Provides annotations related to command-line flag handling.
 *
 * <p>This can be used in either an on-device instrumentation test or a junit (robolectric) test
 * running on the host. To use in an instrumentation test, just {@code RunWith} {@link
 * BaseJUnit4ClassRunner} (or a runner which extends that class). To use from a robolectric test,
 * add the following test rule to your class:
 *
 * <pre>
 * &#64Rule
 * TestRule mRule = CommandLineFlags.getTestRule();
 * </pre>
 *
 * <p>Then you can annotate the test class, test methods, or test rules with {@code
 * CommandLineFlags.Add} or {@code CommandLineFlags.Remove}. Uses of these annotations on a derived
 * class will take precedence over uses on its base classes, so a derived class can add a
 * command-line flag that a base class has removed (or vice versa). Similarly, uses of these
 * annotations on a test method will take precedence over uses on the containing class.
 *
 * <p>
 * These annonations may also be used on Junit4 Rule classes and on their base classes. Note,
 * however that the annotation processor only looks at the declared type of the Rule, not its actual
 * type, so in, for example:
 *
 * <pre>
 * &#64Rule
 * TestRule mRule = new ChromeActivityTestRule();
 * </pre>
 *
 * will only look for CommandLineFlags annotations on TestRule, not for CommandLineFlags annotations
 * on ChromeActivityTestRule.
 * <p>
 * In addition a rule may not remove flags added by an independently invoked rule, although it may
 * remove flags added by its base classes.
 * <p>
 * Uses of these annotations on the test class or methods take precedence over uses on Rule classes.
 * <p>
 * Note that this class should never be instantiated.
 */
public final class CommandLineFlags {
    private static final String TAG = "CommandLineFlags";
    private static final String DISABLE_FEATURES = "disable-features";
    private static final String ENABLE_FEATURES = "enable-features";

    private static boolean sInitializedForTest;

    // These members are used to track CommandLine state modifications made by the class/test method
    // currently being run, to be undone when the class/test method finishes.
    private static Set<String> sClassFlagsToRemove;
    private static Map<String, String> sClassFlagsToAdd;
    private static Set<String> sMethodFlagsToRemove;
    private static Map<String, String> sMethodFlagsToAdd;

    /**
     * Adds command-line flags to the {@link org.chromium.base.CommandLine} for this test.
     */
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

    /**
     * Sets up the CommandLine with the appropriate flags.
     *
     * This will add the difference of the sets of flags specified by {@link CommandLineFlags.Add}
     * and {@link CommandLineFlags.Remove} to the {@link org.chromium.base.CommandLine}. Note that
     * trying to remove a flag set externally, i.e. by the command-line flags file, will not work.
     */
    private static void setUpClass(Class<?> clazz) {
        // The command line may already have been initialized by Application-level init. We need to
        // re-initialize it with test flags.
        if (!sInitializedForTest) {
            CommandLine.reset();
            CommandLineInitUtil.initCommandLine(getTestCmdLineFile());
            sInitializedForTest = true;
        }

        Set<String> flags = new HashSet<>();
        updateFlagsForClass(clazz, flags);
        sClassFlagsToRemove = new HashSet<>();
        sClassFlagsToAdd = new HashMap<>();
        applyFlags(flags, null, sClassFlagsToRemove, sClassFlagsToAdd);
    }

    private static void tearDownClass() {
        for (Activity a : ApplicationStatus.getRunningActivities()) {
            if (ApplicationStatus.getStateForActivity(a) < ActivityState.RESUMED) {
                Log.w(TAG,
                        "Activity " + a + ", is still starting up while the Command Line flags are "
                                + "being reset. This is a known source of flakiness.");
            }
        }
        restoreFlags(sClassFlagsToRemove, sClassFlagsToAdd);
        sClassFlagsToRemove = null;
        sClassFlagsToAdd = null;
    }

    private static void setUpMethod(Method method) {
        Set<String> flagsToAdd = new HashSet<>();
        Set<String> flagsToRemove = new HashSet<>();
        updateFlagsForMethod(method, flagsToAdd, flagsToRemove);
        sMethodFlagsToRemove = new HashSet<>();
        sMethodFlagsToAdd = new HashMap<>();
        applyFlags(flagsToAdd, flagsToRemove, sMethodFlagsToRemove, sMethodFlagsToAdd);
    }

    private static void tearDownMethod() {
        restoreFlags(sMethodFlagsToRemove, sMethodFlagsToAdd);
        sMethodFlagsToRemove = null;
        sMethodFlagsToAdd = null;
    }

    private static void restoreFlags(Set<String> flagsToRemove, Map<String, String> flagsToAdd) {
        for (String flag : flagsToRemove) {
            CommandLine.getInstance().removeSwitch(flag);
        }
        for (Entry<String, String> flag : flagsToAdd.entrySet()) {
            CommandLine.getInstance().appendSwitchWithValue(flag.getKey(), flag.getValue());
        }
    }

    private static void applyFlags(Set<String> flagsToAdd, Set<String> flagsToRemove,
            Set<String> flagsToRemoveForRestore, Map<String, String> flagsToAddForRestore) {
        Set<String> enableFeatures = new HashSet<String>(getFeatureValues(ENABLE_FEATURES));
        Set<String> disableFeatures = new HashSet<String>(getFeatureValues(DISABLE_FEATURES));
        for (String flag : flagsToAdd) {
            String[] parsedFlags = flag.split("=", 2);
            if (parsedFlags.length == 1) {
                if (!CommandLine.getInstance().hasSwitch(flag)) {
                    CommandLine.getInstance().appendSwitch(flag);
                    flagsToRemoveForRestore.add(flag);
                }
            } else if (ENABLE_FEATURES.equals(parsedFlags[0])) {
                // We collect enable/disable features flags separately and aggregate them because
                // they may be specified multiple times, in which case the values will trample each
                // other.
                Collections.addAll(enableFeatures, parsedFlags[1].split(","));
            } else if (DISABLE_FEATURES.equals(parsedFlags[0])) {
                Collections.addAll(disableFeatures, parsedFlags[1].split(","));
            } else {
                String existingValue = CommandLine.getInstance().getSwitchValue(parsedFlags[0]);
                if (parsedFlags[1].equals(existingValue)) continue;
                if (existingValue != null) {
                    flagsToAddForRestore.put(parsedFlags[0], existingValue);
                    CommandLine.getInstance().removeSwitch(parsedFlags[0]);
                }
                CommandLine.getInstance().appendSwitchWithValue(parsedFlags[0], parsedFlags[1]);
                flagsToRemoveForRestore.add(parsedFlags[0]);
            }
        }

        if (enableFeatures.size() > 0) {
            String existingValue = CommandLine.getInstance().getSwitchValue(ENABLE_FEATURES);
            flagsToAddForRestore.put(ENABLE_FEATURES, existingValue);
            CommandLine.getInstance().appendSwitchWithValue(
                    ENABLE_FEATURES, TextUtils.join(",", enableFeatures));
            flagsToRemoveForRestore.add(ENABLE_FEATURES);
        }
        if (disableFeatures.size() > 0) {
            String existingValue = CommandLine.getInstance().getSwitchValue(DISABLE_FEATURES);
            flagsToAddForRestore.put(DISABLE_FEATURES, existingValue);
            CommandLine.getInstance().appendSwitchWithValue(
                    DISABLE_FEATURES, TextUtils.join(",", disableFeatures));
            flagsToRemoveForRestore.add(DISABLE_FEATURES);
        }
        if (flagsToRemove == null) return;
        for (String flag : flagsToRemove) {
            if (CommandLine.getInstance().hasSwitch(flag)) {
                CommandLine.getInstance().removeSwitch(flag);
                flagsToAddForRestore.put(flag, null);
            }
        }
    }

    private static void updateFlagsForClass(Class<?> clazz, Set<String> flags) {
        // Get flags from rules within the class.
        for (Field field : clazz.getFields()) {
            if (field.isAnnotationPresent(Rule.class)) {
                // The order in which fields are returned is undefined, so, for consistency,
                // a rule must only ever add flags.
                updateFlagsForClass(field.getType(), flags);
            }
        }
        for (Method method : clazz.getMethods()) {
            Assert.assertFalse("@Rule annotations on methods are unsupported. Cause: "
                            + method.toGenericString(),
                    method.isAnnotationPresent(Rule.class));
        }

        // Add the flags from the parent. Override any flags defined by the rules.
        Class<?> parent = clazz.getSuperclass();
        if (parent != null) updateFlagsForClass(parent, flags);

        // Flags on the element itself override all other flag sources.
        if (clazz.isAnnotationPresent(CommandLineFlags.Add.class)) {
            flags.addAll(Arrays.asList(clazz.getAnnotation(CommandLineFlags.Add.class).value()));
        }
    }

    private static void updateFlagsForMethod(
            Method method, Set<String> flagsToAdd, Set<String> flagsToRemove) {
        if (method.isAnnotationPresent(CommandLineFlags.Add.class)) {
            flagsToAdd.addAll(
                    Arrays.asList(method.getAnnotation(CommandLineFlags.Add.class).value()));
        }
        if (method.isAnnotationPresent(CommandLineFlags.Remove.class)) {
            flagsToRemove.addAll(
                    Arrays.asList(method.getAnnotation(CommandLineFlags.Remove.class).value()));
        }
    }

    private static List<String> getFeatureValues(String flag) {
        String value = CommandLine.getInstance().getSwitchValue(flag);
        if (value == null) return new ArrayList<>();
        return Arrays.asList(value.split(","));
    }

    private CommandLineFlags() {
        throw new AssertionError("CommandLineFlags is a non-instantiable class");
    }

    private static class CommandLineFlagsTestRule implements TestRule {
        @Override
        public Statement apply(final Statement base, Description description) {
            return new Statement() {
                @Override
                public void evaluate() throws Throwable {
                    try {
                        Class clazz = description.getTestClass();
                        CommandLineFlags.setUpClass(clazz);
                        CommandLineFlags.setUpMethod(clazz.getMethod(description.getMethodName()));

                        base.evaluate();
                    } finally {
                        CommandLineFlags.tearDownMethod();
                        CommandLineFlags.tearDownClass();
                    }
                }
            };
        }
    }

    public static TestRule getTestRule() {
        return new CommandLineFlagsTestRule();
    }

    public static TestHook getPreTestHook() {
        return (targetContext, testMethod) -> CommandLineFlags.setUpMethod(testMethod.getMethod());
    }

    public static ClassHook getPreClassHook() {
        return (targetContext, testClass) -> CommandLineFlags.setUpClass(testClass);
    }

    public static TestHook getPostTestHook() {
        return (targetContext, testMethod) -> CommandLineFlags.tearDownMethod();
    }

    public static ClassHook getPostClassHook() {
        return (targetContext, testClass) -> CommandLineFlags.tearDownClass();
    }

    public static String getTestCmdLineFile() {
        return "test-cmdline-file";
    }
}
