// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.chromium.base.CommandLine;

import java.lang.annotation.Annotation;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Helps with setting Feature flags during tests. Relies on registering the appropriate
 * {@code Processor} rule on the test class.
 **
 * Use {@link EnableFeatures} and {@link DisableFeatures} to specify the features to register and
 * whether they should be enabled.
 *
 * Sample code:
 *
 * <pre>
 * public class Test {
 *    &#64;Rule
 *    public TestRule mProcessor = new Features.JUnitProcessor();
 *
 *    &#64;EnableFeatures(BaseFeatures.Foo)
 *    public void testFoo() { ... }
 *
 *    &#64;EnableFeatures(ContentFeatureList.Foo)
 *    public void testFoo() { ... }
 * }
 * </pre>
 *
 * This class also offers Singleton access to enable and disable features, letting other rules
 * affect the final configuration before the start of the test.
 *
 * See {@link FeaturesBase} for more details.
 */
public class Features extends FeaturesBase {
    @Retention(RetentionPolicy.RUNTIME)
    public @interface EnableFeatures {
        String[] value();
    }

    @Retention(RetentionPolicy.RUNTIME)
    public @interface DisableFeatures {
        String[] value();
    }

    private Features() {}

    /**
     * @return the instance of this class, creating a new one if necessary.
     */
    public static Features getInstance() {
        if (sInstance == null) sInstance = new Features();
        assert sInstance
                instanceof Features
            : "Mixed use of Features annotations detected. "
              + "Ensure the correct base/ or chrome/ version is being used.";
        return (Features) sInstance;
    }

    /**
     * Feature processor intended to be used in unit tests tests. The collected feature states would
     * be applied to {@link FeatureList}'s internal test-only feature map.
     */
    public static class JUnitProcessor extends BaseJUnitProcessor {
        public JUnitProcessor() {
            super(EnableFeatures.class, DisableFeatures.class);
            getInstance();
        }

        @Override
        protected void before() {
            getInstance();
            super.before();
        }

        @Override
        protected void collectFeatures() {
            collectFeaturesImpl(getAnnotations());
        }
    }

    /**
     * Feature processor intended to be used in instrumentation tests with native library. The
     * collected feature states would be applied to {@link CommandLine}.
     */
    public static class InstrumentationProcessor extends BaseInstrumentationProcessor {
        public InstrumentationProcessor() {
            super(EnableFeatures.class, DisableFeatures.class);
            getInstance();
        }

        @Override
        protected void collectFeatures() {
            collectFeaturesImpl(getAnnotations());
        }
    }

    private static void collectFeaturesImpl(List<Annotation> annotations) {
        for (Annotation annotation : annotations) {
            if (annotation instanceof EnableFeatures) {
                sInstance.enable(((EnableFeatures) annotation).value());
            } else if (annotation instanceof DisableFeatures) {
                sInstance.disable(((DisableFeatures) annotation).value());
            }
        }
    }
}
