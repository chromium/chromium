// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import androidx.test.InstrumentationRegistry;

import org.mockito.internal.progress.ThreadSafeMockingProgress;

/**
 * Automatically resets and clears internal Mockito state to prevent memory leaks in Android tests.
 */
public class MockitoResetter {

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
}
