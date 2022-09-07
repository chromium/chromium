// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import java.lang.ref.WeakReference;

/**
 * Utils for doing garbage collection tests.
 */
public class GarbageCollectionTestUtils {
    /**
     * Relying on just one single GC might make instrumentation tests flaky.
     * Note that {@link #MAX_GC_ITERATIONS} * {@link #GC_SLEEP_TIME} should not be too large,
     * since there are tests asserting objects NOT garbage collected.
     */
    private static final int MAX_GC_ITERATIONS = 3;
    private static final long GC_SLEEP_TIME = 10;

    /**
     * Do garbage collection and see if an object is released.
     * @param reference A {@link WeakReference} pointing to the object.
     * @return Whether the object can be garbage-collected.
     */
    public static boolean canBeGarbageCollected(WeakReference<?> reference) {
        // Robolectric tests, one iteration is enough.
        final int iterations = MAX_GC_ITERATIONS;
        final long sleepTime = GC_SLEEP_TIME;
        Runtime runtime = Runtime.getRuntime();
        for (int i = 0; i < iterations; i++) {
            runtime.runFinalization();
            runtime.gc();
            if (reference.get() == null) return true;

            // Pause for a while and then go back around the loop to try again.
            try {
                Thread.sleep(sleepTime);
            } catch (InterruptedException e) {
                // Ignore any interrupts and just try again.
            }
        }

        return reference.get() == null;
    }
}
