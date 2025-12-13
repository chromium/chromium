// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import leakcanary.LeakAssertions;
import leakcanary.LeakCanary;

import shark.AndroidReferenceMatchers;
import shark.ReferenceMatcher;
import shark.ReferenceMatcherKt;
import shark.ReferencePattern;
import shark.ReferencePattern.InstanceFieldPattern;
import shark.ReferencePattern.JavaLocalPattern;
import shark.ReferencePattern.StaticFieldPattern;

import org.chromium.base.test.BaseJUnit4ClassRunner.AfterCleanupCheck;
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

@ServiceImpl(AfterCleanupCheck.class)
public class LeakCanaryChecker implements AfterCleanupCheck {
    @Override
    public void onAfterTestClass(Class<?> testClass) {
        if (testClass.getAnnotation(EnableLeakChecks.class) != null) {
            checkLeaks();
        }
    }

    // We only allow annotating entire test classes with this, since we rely on some class-level
    // cleanups to occur, which do not happen between individual test methods in a class.
    @Target({ElementType.TYPE})
    @Retention(RetentionPolicy.RUNTIME)
    public @interface EnableLeakChecks {}

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
        LeakAssertions.INSTANCE.assertNoLeaks("LeakCanaryChecker");
    }
}
