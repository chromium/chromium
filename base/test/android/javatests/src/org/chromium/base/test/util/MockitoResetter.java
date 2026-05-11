// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import androidx.test.InstrumentationRegistry;

import org.mockito.Mockito;
import org.mockito.internal.progress.ThreadSafeMockingProgress;

import java.lang.reflect.Field;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.Set;

/**
 * Automatically resets and clears internal Mockito state to prevent memory leaks in Android tests.
 */
public class MockitoResetter {
    private static final Set<Object> sMocksToReset =
            Collections.newSetFromMap(new IdentityHashMap<>());

    /**
     * Clears Mockito's ongoing stubbing on both Instrumentation and Main threads.
     *
     * <p>Mockito stores stubbing progress and invocation logs in ThreadLocal variables. In Android
     * tests, these ThreadLocals can retain references to Activities and Contexts if not cleared,
     * leading to severe memory leaks across tests.
     */
    public static void clearOngoingStubbing() {
        Runnable clearProgress =
                () -> {
                    ThreadSafeMockingProgress.mockingProgress().resetOngoingStubbing();
                };

        // Clear progress on the current (Instrumentation) thread.
        clearProgress.run();

        // Clear progress on the UI/Main thread where leaks often reside.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(clearProgress);
    }

    /**
     * Remembers all mocks declared as fields in the target object to be reset later.
     *
     * @param target The object to scan for mocks (usually the test instance).
     */
    public static void addMocks(Object target) {
        if (target == null) return;

        Class<?> clazz = target.getClass();
        while (clazz != null) {
            for (Field field : clazz.getDeclaredFields()) {
                field.setAccessible(true);
                try {
                    Object value = field.get(target);
                    if (value != null && Mockito.mockingDetails(value).isMock()) {
                        sMocksToReset.add(value);
                    }
                } catch (IllegalAccessException e) {
                    throw new RuntimeException(e);
                }
            }
            clazz = clazz.getSuperclass();
        }
    }

    /**
     * Resets all recorded mocks and clears the registry. This should be called after activities are
     * finished to catch late invocations.
     */
    public static void resetRecordedMocks() {
        try {
            for (Object mock : sMocksToReset) {
                Mockito.reset(mock);
            }
        } finally {
            sMocksToReset.clear();
        }
    }
}
