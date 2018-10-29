// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

import org.chromium.base.test.util.Feature;

/**
 * Unit tests for VersionNumber.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class VersionNumberTest {
    private static final String DEVELOPER_BUILD = "0.1 Developer Build";
    private static final String BUILD_BOGUS_1_2_3_4 = "Bogus-1.2.3.4";
    private static final String BUILD_1_2_3 = "1.2.3";
    private static final String BUILD_1_2_3_4_5 = "1.2.3.4.5";
    private static final String STRING_1_2_3_4 = "1.2.3.4";

    private static final VersionNumber BUILD_0_2_3_4 = VersionNumber.fromString("0.2.3.4");
    private static final VersionNumber BUILD_1_1_3_4 = VersionNumber.fromString("1.1.3.4");
    private static final VersionNumber BUILD_1_2_2_4 = VersionNumber.fromString("1.2.2.4");
    private static final VersionNumber BUILD_1_2_3_3 = VersionNumber.fromString("1.2.3.3");
    private static final VersionNumber BUILD_1_2_3_4 = VersionNumber.fromString(STRING_1_2_3_4);

    private static final VersionNumber BUILD_27_0_1453_42 =
            VersionNumber.fromString("27.0.1453.42");
    private static final VersionNumber BUILD_26_0_1410_49 =
            VersionNumber.fromString("26.0.1410.49");

    @Test
    @Feature({"Omaha"})
    public void testSuccessfulParsing() {
        VersionNumber number = VersionNumber.fromString(STRING_1_2_3_4);
        Assert.assertFalse("Failed to parse valid build number", number == null);
    }

    @Test
    @Feature({"Omaha"})
    public void testDeveloperBuild() {
        checkFailParse(DEVELOPER_BUILD);
    }

    @Test
    @Feature({"Omaha"})
    public void testBogusVersion() {
        checkFailParse(BUILD_BOGUS_1_2_3_4);
    }

    @Test
    @Feature({"Omaha"})
    public void testTooFew() {
        checkFailParse(BUILD_1_2_3);
    }

    @Test
    @Feature({"Omaha"})
    public void testTooMany() {
        checkFailParse(BUILD_1_2_3_4_5);
    }

    @Test
    @Feature({"Omaha"})
    public void testEqualNumbers() {
        VersionNumber first = VersionNumber.fromString(STRING_1_2_3_4);
        VersionNumber second = VersionNumber.fromString(STRING_1_2_3_4);
        Assert.assertFalse("Numbers should be equal.", first.isSmallerThan(second));
        Assert.assertFalse("Numbers should be equal.", second.isSmallerThan(first));
    }

    @Test
    @Feature({"Omaha"})
    public void testSmallerThan() {
        Assert.assertTrue("Should have been smaller", BUILD_0_2_3_4.isSmallerThan(BUILD_1_2_3_4));
        Assert.assertTrue("Should have been smaller", BUILD_1_1_3_4.isSmallerThan(BUILD_1_2_3_4));
        Assert.assertTrue("Should have been smaller", BUILD_1_2_2_4.isSmallerThan(BUILD_1_2_3_4));
        Assert.assertTrue("Should have been smaller", BUILD_1_2_3_3.isSmallerThan(BUILD_1_2_3_4));
        Assert.assertTrue("Should have been smaller",
                BUILD_26_0_1410_49.isSmallerThan(BUILD_27_0_1453_42));

        Assert.assertFalse("Should have been bigger", BUILD_1_2_3_4.isSmallerThan(BUILD_0_2_3_4));
        Assert.assertFalse("Should have been bigger", BUILD_1_2_3_4.isSmallerThan(BUILD_1_1_3_4));
        Assert.assertFalse("Should have been bigger", BUILD_1_2_3_4.isSmallerThan(BUILD_1_2_2_4));
        Assert.assertFalse("Should have been bigger", BUILD_1_2_3_4.isSmallerThan(BUILD_1_2_3_3));
        Assert.assertFalse("Should have been bigger",
                BUILD_27_0_1453_42.isSmallerThan(BUILD_26_0_1410_49));
    }

    /** Asserts that parsing a particular string should fail. */
    public void checkFailParse(String str) {
        VersionNumber number = VersionNumber.fromString(str);
        Assert.assertTrue("Successfully parsed invalid number: " + str, number == null);
    }
}
