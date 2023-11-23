// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.CommandLine;
import org.chromium.base.FeatureList;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Base class to help with setting Feature flags during tests. Relies on registering the
 * appropriate {@link Processor} rule on the test class.
 *
 * Subclasses should introduce a {@code EnableFeatures} and {@code DisableFeatures}
 * annotation and register them in classes that extend the {@link BaseJUnitProcessor} or
 * {@link BaseInstrumentationProcessor}
 *
 * See {@link org.chromium.chrome.test.util.browser.Features} for an example of this.
 *
 * Subclasses should offer Singleton access to enable and disable features, letting other rules
 * affect the final configuration before the start of the test.
 */
public class FeaturesBase {
    protected static @Nullable FeaturesBase sInstance;
    protected final Map<String, Boolean> mRegisteredState = new HashMap<>();

    /**
     * Explicitly applies features collected so far to the command line.
     * Note: This is only valid during instrumentation tests.
     * TODO(dgn): remove once we have the compound test rule is available to enable a deterministic
     * rule execution order.
     */
    public void ensureCommandLineIsUpToDate() {
        sInstance.applyForInstrumentation();
    }

    /** Collects the provided features to be registered as enabled. */
    public void enable(String... featureNames) {
        // TODO(dgn): assert that it's not being called too late and will be able to be applied.
        for (String featureName : featureNames) mRegisteredState.put(featureName, true);
    }

    /** Collects the provided features to be registered as disabled. */
    public void disable(String... featureNames) {
        // TODO(dgn): assert that it's not being called too late and will be able to be applied.
        for (String featureName : featureNames) mRegisteredState.put(featureName, false);
    }

    protected void applyForJUnit() {
        FeatureList.setTestFeatures(mRegisteredState);
    }

    protected void applyForInstrumentation() {
        FeatureList.setTestCanUseDefaultsForTesting();
        mergeFeatureLists("enable-features", true);
        mergeFeatureLists("disable-features", false);
    }

    /**
     * Feature processor intended to be used in unit tests. The collected feature states would be
     * applied to {@link FeatureList}'s internal test-only feature map.
     */
    public abstract static class BaseJUnitProcessor extends Processor {
        public BaseJUnitProcessor(Class enabledFeatures, Class disabledFeatures) {
            super(enabledFeatures, disabledFeatures);
        }

        @Override
        protected void applyFeatures() {
            sInstance.applyForJUnit();
        }

        @Override
        protected void after() {
            super.after();
            sInstance = null;
        }
    }

    /**
     * Feature processor intended to be used in instrumentation tests with native library. The
     * collected feature states would be applied to {@link CommandLine}.
     */
    public abstract static class BaseInstrumentationProcessor extends Processor {
        public BaseInstrumentationProcessor(Class enableFeatures, Class disableFeatures) {
            super(enableFeatures, disableFeatures);
        }

        @Override
        protected void applyFeatures() {
            sInstance.applyForInstrumentation();
        }
    }

    /** Resets Features-related state that might persist in between tests. */
    private static void reset() {
        FeatureList.setTestFeatures(null);
        FeatureList.resetTestCanUseDefaultsForTesting();
    }

    private void clearRegisteredState() {
        mRegisteredState.clear();
    }

    /**
     * Add this rule to tests to activate the {@link Features} annotations and choose flags
     * to enable, or get rid of exceptions when the production code tries to check for enabled
     * features.
     */
    private abstract static class Processor extends AnnotationRule {
        public Processor(Class enableFeatures, Class disableFeatures) {
            super(enableFeatures, disableFeatures);
        }

        @Override
        protected void before() {
            assert sInstance != null
                    : "Classes extending BaseProcessor need to create an instance.";
            collectFeatures();
            applyFeatures();
        }

        @Override
        protected void after() {
            reset();

            // sInstance may already be null if there are nested usages.
            if (sInstance == null) return;

            sInstance.clearRegisteredState();
        }

        protected abstract void applyFeatures();

        protected abstract void collectFeatures();
    }

    /**
     * Updates the reference list of features held by the CommandLine by merging it with the feature
     * state registered via this utility.
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
}
