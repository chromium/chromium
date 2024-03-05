// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ResettersForTesting}. */
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

    private static class ResetsToNullAndIncrementsWithOneShotResetter {
        public static String str;
        public static int resetCount;
        private static Runnable sResetter =
                () -> {
                    str = null;
                    resetCount++;
                };

        public static void setStrForTesting(String newStr) {
            str = newStr;
            ResettersForTesting.register(sResetter);
        }
    }

    @BeforeClass
    public static void setUpClass() {
        assertNull(ResetsToOldValue.str);
        ResetsToOldValue.setStrForTesting("setUpClass");
    }

    @AfterClass
    public static void tearDownClass() {
        assertEquals("setUpClass", ResetsToOldValue.str);
        ResettersForTesting.afterClassHooksDidExecute();
        assertNull(ResetsToOldValue.str);
        // Do not fail when test runner calls onAfterClass().
        ResettersForTesting.beforeClassHooksWillExecute();
    }

    @Before
    public void setUp() {
        assertEquals("setUpClass", ResetsToOldValue.str);
        ResetsToOldValue.setStrForTesting("setUp");
    }
    @Test
    public void testTypicalUsage() {
        ResetsToNull.setStrForTesting("foo");
        assertEquals("foo", ResetsToNull.str);
        ResettersForTesting.afterHooksDidExecute();
        var result = ResetsToNull.str;
        // Do not fail when test runner calls onAfterMethod().
        ResettersForTesting.beforeHooksWillExecute();
        Assert.assertNull(result);
    }

    @Test
    public void testResetsToPreviousValue() {
        assertEquals("setUp", ResetsToOldValue.str);

        ResetsToOldValue.setStrForTesting("foo");
        assertEquals("foo", ResetsToOldValue.str);

        // After resetting the value, it should be back to the value set before setUp().
        ResettersForTesting.afterHooksDidExecute();
        var result = ResetsToOldValue.str;
        // Do not fail when test runner calls onAfterMethod().
        ResettersForTesting.beforeHooksWillExecute();
        assertEquals("setUpClass", result);
    }

    @Test
    public void testMultipleResets() {
        assertEquals("setUp", ResetsToOldValue.str);

        // Then set the next value.
        ResetsToOldValue.setStrForTesting("foo");
        assertEquals("foo", ResetsToOldValue.str);

        // Now, next that value into another one to ensure the unwinding works.
        ResetsToOldValue.setStrForTesting("bar");
        assertEquals("bar", ResetsToOldValue.str);

        // After resetting the value, it should be back to the value set before setUp().
        ResettersForTesting.afterHooksDidExecute();
        var result = ResetsToOldValue.str;
        // Do not fail when test runner calls onAfterMethod().
        ResettersForTesting.beforeHooksWillExecute();
        assertEquals("setUpClass", result);
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
        ResettersForTesting.afterHooksDidExecute();
        var result = ResetsToNullAndIncrementsWithOneShotResetter.resetCount;
        // Do not fail when test runner calls onAfterMethod().
        ResettersForTesting.beforeHooksWillExecute();
        assertEquals(1, result);
    }
}
