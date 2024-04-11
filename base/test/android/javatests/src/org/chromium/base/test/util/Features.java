// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.CommandLine;
import org.chromium.base.FeatureList;
import org.chromium.base.cached_flags.CachedFlag;
import org.chromium.base.cached_flags.CachedFlagUtils;
import org.chromium.base.test.util.AnnotationProcessingUtils.AnnotationExtractor;

import java.lang.annotation.Annotation;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Method;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Helps with setting Feature flags during tests. Relies on registering the appropriate {@code
 * Processor} rule on the test class.
 *
 * <p>Use {@link EnableFeatures} and {@link DisableFeatures} to specify the features to register and
 * whether they should be enabled.
 *
 * <p>Sample code:
 *
 * <pre>
 * public class Test {
 *    &#64;Rule
 *    public TestRule mProcessor = new Features.JUnitProcessor();
 *
 *    &#64;EnableFeatures(BaseFeatures.Foo)
 *    public void testFoo() { ... }
 *
 *    &#64;DisableFeatures(ContentFeatureList.Foo)
 *    public void testFoo() { ... }
 * }
 * </pre>
 */
public class Features {
    @Retention(RetentionPolicy.RUNTIME)
    public @interface EnableFeatures {
        String[] value();
    }

    @Retention(RetentionPolicy.RUNTIME)
    public @interface DisableFeatures {
        String[] value();
    }

    private static @Nullable Features sInstance;
    private final Map<String, Boolean> mRegisteredState = new HashMap<>();

    private Features() {}

    /**
     * @return the instance of this class, creating a new one if necessary.
     */
    public static Features getInstance() {
        if (sInstance == null) sInstance = new Features();
        return sInstance;
    }

    /**
     * Feature processor intended to be used in unit tests. The collected feature states are applied
     * to {@link FeatureList}'s test values.
     */
    public static class JUnitProcessor extends Processor {

        @Override
        protected void collectFeatures() {
            getInstance().collectFeatures(getAnnotations());
        }

        @Override
        protected void applyFeatures() {
            getInstance().applyForJUnit();
        }

        @Override
        protected void after() {
            super.after();
            sInstance = null;
            resetCachedFlags(/* forInstrumentation= */ false);
        }
    }

    /**
     * Feature processor intended to be used in instrumentation tests with native library. The
     * collected feature states are applied to {@link CommandLine}.
     */
    public static class InstrumentationProcessor extends Processor {

        @Override
        protected void collectFeatures() {
            getInstance().collectFeatures(getAnnotations());
        }

        @Override
        protected void after() {
            super.after();
            resetCachedFlags(/* forInstrumentation= */ true);
        }

        @Override
        protected void applyFeatures() {
            getInstance().applyForInstrumentation();
        }
    }

    /**
     * Add this rule to tests to activate the {@link Features} annotations and choose flags to
     * enable, or get rid of exceptions when the production code tries to check for enabled
     * features.
     */
    private abstract static class Processor extends AnnotationRule {
        public Processor(
                Class<? extends Annotation> firstAnnotationType,
                Class<? extends Annotation>... additionalTypes) {
            super(firstAnnotationType, additionalTypes);
        }

        public Processor() {
            super(EnableFeatures.class, DisableFeatures.class);
        }

        @Override
        protected void before() {
            collectFeatures();
            applyFeatures();
        }

        @Override
        protected void after() {
            // Resets state that might persist in between tests.
            FeatureList.setTestFeatures(null);

            // sInstance may already be null if there are nested usages.
            if (sInstance == null) return;

            sInstance.clearRegisteredState();
        }

        protected abstract void applyFeatures();

        protected abstract void collectFeatures();
    }

    private void collectFeatures(List<Annotation> annotations) {
        for (Annotation annotation : annotations) {
            if (annotation instanceof EnableFeatures) {
                enable(((EnableFeatures) annotation).value());
            } else if (annotation instanceof DisableFeatures) {
                disable(((DisableFeatures) annotation).value());
            }
        }
    }

    /**
     * Collect feature annotations form |testMethod| and apply them for robolectric tests.
     *
     * @param testMethod an @Test method from a Robolectric test.
     */
    public void applyFeaturesFromTestMethodForRobolectric(Method testMethod) {
        AnnotationExtractor annotationExtractor =
                new AnnotationExtractor(EnableFeatures.class, DisableFeatures.class);
        List<Annotation> annotations = annotationExtractor.getMatchingAnnotations(testMethod);
        collectFeatures(annotations);
        applyForJUnit();
    }

    /**
     * Explicitly applies features collected so far to the command line. Note: This is only valid
     * during instrumentation tests. TODO(dgn): remove once we have the compound test rule is
     * available to enable a deterministic rule execution order.
     */
    public void ensureCommandLineIsUpToDate() {
        sInstance.applyForInstrumentation();
    }

    /** Collects the provided features to be registered as enabled. */
    private void enable(String... featureNames) {
        // TODO(dgn): assert that it's not being called too late and will be able to be applied.
        for (String featureName : featureNames) mRegisteredState.put(featureName, true);
    }

    /** Collects the provided features to be registered as disabled. */
    private void disable(String... featureNames) {
        // TODO(dgn): assert that it's not being called too late and will be able to be applied.
        for (String featureName : featureNames) mRegisteredState.put(featureName, false);
    }

    private void applyForJUnit() {
        // In unit tests, @Enable/DisableFeatures become Java-side {@link FeatureList$TestValues}.
        // If a flag is checked but its value is not explicitly set by the test, {@link FeatureList}
        // throws an exception.
        FeatureList.setTestFeatures(mRegisteredState);

        // Set overrides for CachedFlag separately.
        CachedFlag.setFeaturesForTesting(mRegisteredState);
    }

    private void applyForInstrumentation() {
        // In instrumentation tests, command line args --enable/disable-features passed by
        // @CommandLineFlags and @Enable/DisableFeatures are merged, and actually applied via
        // {@link CommandLine}, so that their test values are reflected in native too. Thus,
        // {@link FeatureList} is configured to allow asking native for a flag value regardless of
        // whether some other flag override has been set.
        FeatureList.setTestCanUseDefaultsForTesting();
        mergeFeatureLists("enable-features", true);
        mergeFeatureLists("disable-features", false);

        // Set overrides for CachedFlag separately.
        CachedFlag.setFeaturesForTesting(mRegisteredState);

        // Apply "--force-fieldtrials" passed by @CommandLineFlags.
        FieldTrials.getInstance().applyFieldTrials();
    }

    private void clearRegisteredState() {
        mRegisteredState.clear();
    }

    /** Resets test fixtures for Feature flags after a Robolectric Test. */
    public static void resetAfterRobolectricTest() {
        // Resets state that might persist in between tests.
        FeatureList.setTestFeatures(null);

        resetCachedFlags(false);

        // sInstance may already be null if there are nested usages.
        if (sInstance == null) return;

        sInstance.clearRegisteredState();
    }

    /**
     * Updates the reference list of features held by the CommandLine by merging it with the feature
     * state registered via this utility.
     *
     * @param switchName Name of the command line switch that is the reference feature state.
     * @param enabled Whether the feature list being modified is the enabled or disabled one.
     */
    private void mergeFeatureLists(String switchName, boolean enabled) {
        CommandLine commandLine = CommandLine.getInstance();
        String switchValue = commandLine.getSwitchValue(switchName);
        Set<String> existingFeatures = new HashSet<>();
        if (switchValue != null) {
            Collections.addAll(existingFeatures, switchValue.split(","));
        }
        for (String additionalFeature : mRegisteredState.keySet()) {
            if (mRegisteredState.get(additionalFeature) != enabled) continue;
            existingFeatures.add(additionalFeature);
        }

        // Not really append, it puts the value in a map so we can override values that way too.
        commandLine.appendSwitchWithValue(switchName, TextUtils.join(",", existingFeatures));
    }

    /** Resets Features-related state that might persist in between tests. */
    private static void resetCachedFlags(boolean forInstrumentation) {
        CachedFlagUtils.resetFlagsForTesting();
        if (forInstrumentation) {
            CachedFlag.resetDiskForTesting();
        }
        FieldTrials.getInstance().reset();
    }
}
