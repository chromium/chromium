// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import leakcanary.LeakAssertions;
import leakcanary.LeakCanary;

import org.junit.runners.model.FrameworkMethod;

import shark.AndroidReferenceMatchers;
import shark.ReferenceMatcher;
import shark.ReferenceMatcherKt;
import shark.ReferencePattern;
import shark.ReferencePattern.InstanceFieldPattern;
import shark.ReferencePattern.JavaLocalPattern;
import shark.ReferencePattern.StaticFieldPattern;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.test.BaseJUnit4ClassRunner.AfterCleanupCheck;
import org.chromium.base.test.BaseJUnit4ClassRunner.ClassCleanupHook;
import org.chromium.build.annotations.ServiceImpl;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.ServiceLoader;

public class LeakCanaryChecker {
    private static final String TAG = "LeakCanaryChecker";

    /**
     * @return True if LeakCanary should run for this test class.
     */
    public static boolean isEnabled(Class<?> testClass) {
        boolean enabledByAnnotation = testClass.getAnnotation(EnableLeakChecks.class) != null;
        boolean enabledByFlag = CommandLine.getInstance().hasSwitch("enable-leak-checks");
        boolean disabledByAnnotation = testClass.getAnnotation(DisableLeakChecks.class) != null;

        if (enabledByAnnotation && disabledByAnnotation) {
            throw new IllegalStateException(
                    "Both @EnableLeakChecks and @DisableLeakChecks are specified on "
                            + testClass.getName());
        }

        if ((enabledByAnnotation || enabledByFlag) && disabledByAnnotation) {
            Log.w(TAG, "Leak check skipped by @DisableLeakChecks");
        }

        return (enabledByAnnotation || enabledByFlag) && !disabledByAnnotation;
    }

    @ServiceImpl(ClassCleanupHook.class)
    public static class MockitoResetHook implements ClassCleanupHook {
        @Override
        public void onAfterTest(FrameworkMethod method, Object test) {
            if (isEnabled(test.getClass())) {
                MockitoResetter.addMocks(test);
            }
        }

        @Override
        public void onAfterTestClass(Class<?> testClass) {
            if (isEnabled(testClass)) {
                MockitoResetter.resetRecordedMocks();
                MockitoResetter.clearOngoingStubbing();
            }
        }
    }

    @ServiceImpl(AfterCleanupCheck.class)
    public static class LeakCheckHook implements AfterCleanupCheck {
        @Override
        public void onAfterTestClass(Class<?> testClass) {
            if (isEnabled(testClass)) {
                Log.i(TAG, "Running LeakCanary assertion");
                checkLeaks();
            }
        }
    }

    // We only allow annotating entire test classes with this, since we rely on some class-level
    // cleanups to occur, which do not happen between individual test methods in a class.
    @Target({ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface EnableLeakChecks {}

    // Annotate a test class with this to disable LeakCanary checks, even if
    // @EnableLeakChecks is present or the --enable-leak-checks flag is used.
    @Target({ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface DisableLeakChecks {
        String value();
    }

    /**
     * Interface for providing leak patterns to LeakCanaryChecker. Implement this interface and
     * create a service file in META-INF/services/ to have LeakCanaryChecker pick up the custom leak
     * patterns.
     */
    public interface LeakCanaryConfigProvider {
        /**
         * @return A map of class names to field names for instance field leaks. The key is the
         *     class name, and the value is the field name in the format "ClassName#fieldName".
         */
        default Map<String, String> getInstanceFieldLeaks() {
            return Collections.emptyMap();
        }

        /**
         * @return A map of class names to field names for static field leaks. The key is the class
         *     name, and the value is the field name in the format "ClassName#fieldName".
         */
        default Map<String, String> getStaticFieldLeaks() {
            return Collections.emptyMap();
        }

        /**
         * @return A list of thread names for Java local leaks.
         */
        default List<String> getJavaLocalLeaks() {
            return Collections.emptyList();
        }
    }

    private static class LazyHolder {
        private static final List<String> sJavaLocalLeaks = new ArrayList<>();
        private static final Map<String, String> sInstanceLeaks = new HashMap<>();
        private static final Map<String, String> sStaticLeaks = new HashMap<>();

        static {
            for (LeakCanaryConfigProvider provider :
                    ServiceLoader.load(
                            LeakCanaryConfigProvider.class,
                            LeakCanaryConfigProvider.class.getClassLoader())) {
                sInstanceLeaks.putAll(provider.getInstanceFieldLeaks());
                sStaticLeaks.putAll(provider.getStaticFieldLeaks());
                sJavaLocalLeaks.addAll(provider.getJavaLocalLeaks());
            }

            List<ReferenceMatcher> referenceMatchers = AndroidReferenceMatchers.getAppDefaults();
            referenceMatchers.addAll(ignoredLeaks());
            LeakCanary.Config config =
                    LeakCanary.getConfig()
                            .newBuilder()
                            .referenceMatchers(referenceMatchers)
                            .build();
            LeakCanary.setConfig(config);
        }

        private static List<ReferenceMatcher> ignoredLeaks() {
            List<ReferenceMatcher> refMatchers = new ArrayList<>();

            for (var entry : sInstanceLeaks.entrySet()) {
                refMatchers.add(
                        ReferenceMatcherKt.ignored(
                                getMatch(entry.getKey(), entry.getValue(), true),
                                ReferenceMatcher.Companion.getALWAYS()));
            }
            for (var entry : sStaticLeaks.entrySet()) {
                refMatchers.add(
                        ReferenceMatcherKt.ignored(
                                getMatch(entry.getKey(), entry.getValue(), false),
                                ReferenceMatcher.Companion.getALWAYS()));
            }
            for (String thread : sJavaLocalLeaks) {
                refMatchers.add(
                        ReferenceMatcherKt.ignored(
                                new JavaLocalPattern(thread),
                                ReferenceMatcher.Companion.getALWAYS()));
            }

            return refMatchers;
        }
    }

    private static ReferencePattern getMatch(String clazz, String field, boolean isInstance) {
        // Due to R8 not preserving the class name with an IdentifierNameString field, we need to do
        // this ugly implementation with a map and two separate strings. Ideally, if R8 fixes
        // b/463427883, we would just use the "field" strings, and split it on the "#".
        String[] split = field.split("#");
        String fieldName = split[split.length - 1];
        if (isInstance) {
            return new InstanceFieldPattern(clazz, fieldName);
        }
        return new StaticFieldPattern(clazz, fieldName);
    }

    private static void checkLeaks() {
        // Ensure LazyHolder is initialized, which sets up LeakCanary.
        var unused = LazyHolder.sInstanceLeaks;
        try {
            LeakAssertions.INSTANCE.assertNoLeaks(TAG);
        } catch (AssertionError e) {
            String message = e.getMessage();
            if (message != null && isLikelyTestLeak(message)) {
                throw new AssertionError(
                        "LeakCanary detected a leak which is likely only in tests. "
                                + "Do not consider this a revert-worthy exception, instead add "
                                + "@DisableLeakChecks to this test class and open a bug.",
                        e);
            }
            throw e;
        }
    }

    static boolean isLikelyTestLeak(String message) {
        String[] lines = message.split("\n");
        boolean inTrace = false;
        String[] keywords = {
            "test", "fake", "junit", "mock", "stub", "mocking", "stubbing", "mockito"
        };

        for (String line : lines) {
            if (line.startsWith("┬───")) {
                inTrace = true;
            }
            if (inTrace) {
                if (line.contains("├─") || line.contains("╰→") || line.startsWith("┬───")) {
                    String[] tokens = line.split("[^a-zA-Z0-9]");
                    for (String token : tokens) {
                        if (token.isEmpty()) continue;
                        String[] parts =
                                token.split("(?<=[a-z])(?=[A-Z])|" + "(?<=[A-Z])(?=[A-Z][a-z])");
                        for (String part : parts) {
                            for (String keyword : keywords) {
                                if (part.equalsIgnoreCase(keyword)) {
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
        return false;
    }
}
