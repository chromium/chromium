// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import leakcanary.AppWatcher;
import leakcanary.LeakAssertions;
import leakcanary.LeakCanary;
import leakcanary.RetainedObjectTracker;

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

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.ServiceLoader;

public class LeakCanaryChecker {
    private static final String TAG = "LeakCanaryChecker";

    /** How long to wait for watched objects to be garbage collected before running LeakCanary. */
    @VisibleForTesting public static final long GC_POLL_TIMEOUT_MS = 2000;

    private static final long INITIAL_GC_POLL_INTERVAL_MS = 15;
    private static final long MAX_GC_POLL_INTERVAL_MS = 100;

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

    /**
     * Gives objects watched by LeakCanary (e.g. Activities destroyed by the test) a chance to be
     * garbage collected before running LeakCanary.
     *
     * <p>LeakAssertions.assertNoLeaks() triggers one GC and, if any watched object survives it,
     * sleeps a fixed 2 seconds, checks again without triggering another GC, and then waits for the
     * rest of its 5 second retained delay before a final GC. Objects from the test that just ended
     * are usually still reachable at that first GC but become collectable within a few hundred
     * milliseconds, so most checks waited the full 5 seconds. Polling with short GC cycles lets
     * those checks finish early. If objects are still watched after GC_POLL_TIMEOUT_MS, LeakCanary
     * runs its usual sequence, so leak detection is unchanged.
     *
     * @return True if any object is still watched after polling.
     */
    private static boolean waitForWatchedObjectsToBeCollected() {
        RetainedObjectTracker tracker = AppWatcher.INSTANCE.getObjectWatcher();
        if (!tracker.getHasTrackedObjects()) return false;

        long startMs = SystemClock.uptimeMillis();
        long sleepMs = INITIAL_GC_POLL_INTERVAL_MS;
        boolean stillWatched = true;
        while (SystemClock.uptimeMillis() - startMs < GC_POLL_TIMEOUT_MS) {
            Runtime.getRuntime().gc();
            System.runFinalization();
            // ReferenceQueueDaemon enqueues cleared WeakReferences asynchronously after gc(), so
            // check immediately as a fast path and again after sleeping.
            stillWatched = tracker.getHasTrackedObjects();
            if (!stillWatched) break;
            SystemClock.sleep(sleepMs);
            stillWatched = tracker.getHasTrackedObjects();
            if (!stillWatched) break;
            sleepMs = Math.min(MAX_GC_POLL_INTERVAL_MS, sleepMs * 2);
        }
        Log.i(
                TAG,
                "Waited %dms for watched objects to be collected, still watched: %b",
                SystemClock.uptimeMillis() - startMs,
                stillWatched);
        return stillWatched;
    }

    private static void checkLeaks() {
        if (!waitForWatchedObjectsToBeCollected()) {
            // Nothing is watched, so assertNoLeaks() would be a no-op; return early so LazyHolder
            // (ServiceLoader scan + AndroidReferenceMatchers) is only initialized in processes that
            // reach heap analysis.
            return;
        }
        // Ensure LazyHolder is initialized, which sets up LeakCanary.
        var _ = LazyHolder.sInstanceLeaks;
        try {
            LeakAssertions.INSTANCE.assertNoLeaks(TAG);
        } catch (AssertionError e) {
            String message = e.getMessage();
            if (message != null && isLikelyTestLeak(message)) {
                throw new AssertionError(
                        "LeakCanary detected a test-only leak (not "
                                + "revert-worthy). Please add "
                                + "@DisableLeakChecks to the test class and "
                                + "open a bug. See guide: "
                                + "agents/skills/java-memory-leaks/"
                                + "SKILL.md.",
                        e);
            } else {
                throw new AssertionError(
                        "LeakCanary detected a likely production leak. "
                                + "If the culprit CL is not reverted, do NOT "
                                + "use @DisabledTest (removes functional "
                                + "coverage). Instead, use "
                                + "@DisableLeakChecks to keep the test "
                                + "running. See guide: "
                                + "agents/skills/java-memory-leaks/"
                                + "SKILL.md.",
                        e);
            }
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
