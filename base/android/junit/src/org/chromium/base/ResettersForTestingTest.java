// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static junit.framework.Assert.assertEquals;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link ResettersForTesting}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ResettersForTestingTest {
    private static class ResetsToNull {
        public static String str;
        public static void setStrForTesting(String newStr) {
            str = newStr;
            ResettersForTesting.register(() -> str = null);
        }
    }

    private static class ResetsToOldValue {
        public static String str;

        public static void setStrForTesting(String newStr) {
            String oldValue = str;
            str = newStr;
            ResettersForTesting.register(() -> str = oldValue);
        }
    }

    private static class ResetsToNullAndIncrements {
        public static String str;
        public static int resetCount;

        public static void setStrForTesting(String newStr) {
            str = newStr;
            ResettersForTesting.register(() -> {
                str = null;
                resetCount++;
            });
        }
    }

    private static class ResetsToNullAndIncrementsWithOneShotResetter {
        public static String str;
        public static int resetCount;
        private static Runnable sResetter = () -> {
            str = null;
            resetCount++;
        };

        public static void setStrForTesting(String newStr) {
            str = newStr;
            ResettersForTesting.register(sResetter);
        }
    }

    @Test
    public void testTypicalUsage() {
        ResetsToNull.setStrForTesting("foo");
        assertEquals("foo", ResetsToNull.str);
        ResettersForTesting.executeResetters();
        Assert.assertNull(ResetsToNull.str);
    }

    @Test
    public void testResetsToPreviousValue() {
        // Inject a previous value to verify that we can get back to it.
        ResetsToOldValue.str = "bar";

        ResetsToOldValue.setStrForTesting("foo");
        assertEquals("foo", ResetsToOldValue.str);

        // After resetting the value, it should be back to the first value.
        ResettersForTesting.executeResetters();
        assertEquals("bar", ResetsToOldValue.str);
    }

    @Test
    public void testMultipleResets() {
        // Inject an outer value to verify we can get back to this.
        ResetsToOldValue.str = "qux";

        // Then set the next value.
        ResetsToOldValue.setStrForTesting("foo");
        assertEquals("foo", ResetsToOldValue.str);

        // Now, next that value into another one to ensure the unwinding works.
        ResetsToOldValue.setStrForTesting("bar");
        assertEquals("bar", ResetsToOldValue.str);

        // Since we are invoking the resetters in the reverse order, we should now be back to start.
        ResettersForTesting.executeResetters();
        assertEquals("qux", ResetsToOldValue.str);
    }

    @Test
    public void testResettersExecutedOnlyOnce() {
        // Force set this to 0 for this particular test.
        ResetsToNullAndIncrements.resetCount = 0;
        ResetsToNullAndIncrements.str = null;

        // Set the initial value and register the resetter.
        ResetsToNullAndIncrements.setStrForTesting("some value");
        assertEquals("some value", ResetsToNullAndIncrements.str);

        // Now, execute all resetters and ensure it's only executed once.
        ResettersForTesting.executeResetters();
        assertEquals(1, ResetsToNullAndIncrements.resetCount);

        // Execute the resetters again, and verify it does not invoke the same resetter again.
        ResettersForTesting.executeResetters();
        assertEquals(1, ResetsToNullAndIncrements.resetCount);
    }

    @Test
    public void testResettersExecutedOnlyOnceForOneShotResetters() {
        // Force set this to 0 for this particular test.
        ResetsToNullAndIncrementsWithOneShotResetter.resetCount = 0;
        ResetsToNullAndIncrementsWithOneShotResetter.str = null;

        // Set the initial value and register the resetter twice.
        ResetsToNullAndIncrementsWithOneShotResetter.setStrForTesting("some value");
        ResetsToNullAndIncrementsWithOneShotResetter.setStrForTesting("some other value");
        assertEquals("some other value", ResetsToNullAndIncrementsWithOneShotResetter.str);

        // Now, execute all resetters and ensure it's only executed once, since it is a single
        // instance of the same resetter.
        ResettersForTesting.executeResetters();
        assertEquals(1, ResetsToNullAndIncrementsWithOneShotResetter.resetCount);

        // Execute the resetters again, and verify it does not invoke the same resetter again.
        ResettersForTesting.executeResetters();
        assertEquals(1, ResetsToNullAndIncrementsWithOneShotResetter.resetCount);
    }
}
