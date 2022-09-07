// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;

import java.util.List;

/**
 * Test class for {@link CommandLineFlags}.
 */
@RunWith(CommandLineFlagsTest.ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@CommandLineFlags.
Add({CommandLineFlagsTest.FLAG_1, "flagwithvalue=foo", "enable-features=feature1,feature2"})
public class CommandLineFlagsTest {
    public static class ClassRunner extends BaseJUnit4ClassRunner {
        public ClassRunner(final Class<?> klass) throws InitializationError {
            super(klass);
        }

        // Verify class-level modifications are reset after class finishes.
        @Override
        protected List<ClassHook> getPostClassHooks() {
            return addToList(ClassRunner.super.getPostClassHooks(), (targetContext, testClass) -> {
                verifyCommandLine(false, false, false, false, false, false, false);
                Assert.assertFalse(CommandLine.getInstance().hasSwitch("flagwithvalue"));
                String enabledFeatures =
                        CommandLine.getInstance().getSwitchValue("enable-features");
                if (enabledFeatures != null) {
                    Assert.assertFalse(enabledFeatures.contains("feature1"));
                    Assert.assertFalse(enabledFeatures.contains("feature2"));
                }
            });
        }

        // Verify that after each test, flags are reset to class-level state.
        @Override
        protected List<TestHook> getPostTestHooks() {
            return addToList(ClassRunner.super.getPostTestHooks(),
                    (targetContext, testMethod) -> { verifyClassLevelStateOnly(); });
        }
    }

    static final String FLAG_1 = "flag1";
    private static final String FLAG_2 = "flag2";
    private static final String FLAG_3 = "flag3";
    private static final String FLAG_4 = "flag4";
    private static final String FLAG_5 = "flag5";
    private static final String FLAG_6 = "flag6";
    private static final String FLAG_7 = "flag7";

    @CommandLineFlags.Add(FLAG_2)
    private static class EmptyRule implements TestRule {
        @Override
        public Statement apply(Statement base, Description description) {
            return base;
        }
    }

    @CommandLineFlags.Add(FLAG_3)
    private static class MyRule extends EmptyRule {
        @CommandLineFlags.Add(FLAG_4)
        private static class InnerRule extends EmptyRule {}

        @SuppressWarnings("UnusedNestedClass")
        @CommandLineFlags.Add(FLAG_5)
        private static class UnusedRule extends EmptyRule {}

        @Rule
        public InnerRule mInnerRule = new InnerRule();
    }

    @Rule
    public MyRule mRule = new MyRule();

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized();
    }

    private static void verifyCommandLine(boolean flag1, boolean flag2, boolean flag3,
            boolean flag4, boolean flag5, boolean flag6, boolean flag7) {
        CommandLine cmdLine = CommandLine.getInstance();
        Assert.assertEquals(flag1, cmdLine.hasSwitch(FLAG_1));
        Assert.assertEquals(flag2, cmdLine.hasSwitch(FLAG_2));
        Assert.assertEquals(flag3, cmdLine.hasSwitch(FLAG_3));
        Assert.assertEquals(flag4, cmdLine.hasSwitch(FLAG_4));
        Assert.assertEquals(flag5, cmdLine.hasSwitch(FLAG_5));
        Assert.assertEquals(flag6, cmdLine.hasSwitch(FLAG_6));
        Assert.assertEquals(flag7, cmdLine.hasSwitch(FLAG_7));
    }

    private static void verifyClassLevelStateOnly() {
        verifyCommandLine(true, true, true, true, false, false, false);
        Assert.assertEquals("foo", CommandLine.getInstance().getSwitchValue("flagwithvalue"));
        String enabledFeatures = CommandLine.getInstance().getSwitchValue("enable-features");
        Assert.assertTrue(enabledFeatures.contains("feature1"));
        Assert.assertTrue(enabledFeatures.contains("feature2"));
        Assert.assertFalse(
                CommandLine.getInstance().getSwitchValue("enable-features").contains("feature3"));
        String disabledFeatures = CommandLine.getInstance().getSwitchValue("disable-features");
        if (disabledFeatures != null) Assert.assertFalse(disabledFeatures.contains("feature2"));
    }

    @Test
    @SmallTest
    @Feature({"CommandLine"})
    public void testNoMethodModifications() {
        verifyClassLevelStateOnly();
    }

    @Test
    @SmallTest
    @Feature({"CommandLine"})
    @CommandLineFlags.Add({FLAG_1, FLAG_6})
    public void testMethodAdd() {
        verifyCommandLine(true, true, true, true, false, true, false);
    }

    @Test
    @SmallTest
    @Feature({"CommandLine"})
    @CommandLineFlags.Remove({FLAG_4, FLAG_7})
    public void testMethodRemove() {
        verifyCommandLine(true, true, true, false, false, false, false);
    }

    @Test
    @SmallTest
    @Feature({"CommandLine"})
    @CommandLineFlags.Add({"flagwithvalue=bar"})
    public void testOverrideFlagValue() {
        Assert.assertEquals("bar", CommandLine.getInstance().getSwitchValue("flagwithvalue"));
    }

    @Test
    @SmallTest
    @Feature({"CommandLine"})
    @CommandLineFlags.Add({"enable-features=feature3", "disable-features=feature2"})
    public void testFeatures() {
        String enabledFeatures = CommandLine.getInstance().getSwitchValue("enable-features");
        Assert.assertTrue(enabledFeatures.contains("feature1"));
        Assert.assertTrue(enabledFeatures.contains("feature2"));
        Assert.assertTrue(enabledFeatures.contains("feature3"));
        Assert.assertTrue(
                CommandLine.getInstance().getSwitchValue("disable-features").contains("feature2"));
    }
}
