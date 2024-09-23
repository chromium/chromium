// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;

/** Test class for {@link CommandLineFlags}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@CommandLineFlags.Add({
    CommandLineFlagsTest.FLAG_1,
    CommandLineFlagsTest.FLAG_2,
    "flagwithvalue=foo",
    "enable-features=feature1,feature2"
})
public class CommandLineFlagsTest {
    static final String FLAG_1 = "flag1";
    static final String FLAG_2 = "flag2";
    private static final String FLAG_3 = "flag3";
    private static final String FLAG_4 = "flag4";

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized();
    }

    private static void verifyCommandLine(
            boolean flag1, boolean flag2, boolean flag3, boolean flag4) {
        CommandLine cmdLine = CommandLine.getInstance();
        Assert.assertEquals(flag1, cmdLine.hasSwitch(FLAG_1));
        Assert.assertEquals(flag2, cmdLine.hasSwitch(FLAG_2));
        Assert.assertEquals(flag3, cmdLine.hasSwitch(FLAG_3));
        Assert.assertEquals(flag4, cmdLine.hasSwitch(FLAG_4));
    }

    private static void verifyClassLevelStateOnly() {
        verifyCommandLine(true, true, false, false);
        Assert.assertEquals("foo", CommandLine.getInstance().getSwitchValue("flagwithvalue"));
        String enabledFeatures = CommandLine.getInstance().getSwitchValue("enable-features");
        Assert.assertTrue(enabledFeatures.contains("feature1"));
        Assert.assertTrue(enabledFeatures.contains("feature2"));
        Assert.assertFalse(enabledFeatures.contains("feature3"));
        String disabledFeatures = CommandLine.getInstance().getSwitchValue("disable-features");
        if (disabledFeatures != null) {
            Assert.assertFalse(disabledFeatures.contains("feature2"));
        }
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
    @CommandLineFlags.Add({FLAG_1, FLAG_3})
    public void testMethodAdd() {
        verifyCommandLine(true, true, true, false);
    }

    @Test
    @SmallTest
    @Feature({"CommandLine"})
    @CommandLineFlags.Remove(FLAG_1)
    public void testMethodRemove() {
        verifyCommandLine(false, true, false, false);
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
        Assert.assertFalse(enabledFeatures.contains("feature2"));
        Assert.assertTrue(enabledFeatures.contains("feature3"));
        Assert.assertTrue(
                CommandLine.getInstance().getSwitchValue("disable-features").contains("feature2"));
    }
}
