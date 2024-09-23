// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric.common;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.common.Flag;
import org.chromium.android_webview.common.FlagOverrideHelper;
import org.chromium.base.CommandLine;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Unit tests for FlagOverrideHelper. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FlagOverrideHelperTest {
    @Before
    public void setUp() {
        // Create a fresh CommandLine instance for each test.
        CommandLine.resetForTesting(true);
    }

    private static final Flag[] sMockFlagList = {
        Flag.commandLine("flag-1", "This is flag 1"),
        Flag.commandLine("flag-2", "This is flag 2"),
        Flag.commandLine("flag-3", "This is flag 3", "some-value"),
        Flag.baseFeature("feature-1", "This is feature 1"),
        Flag.baseFeature("feature-2", "This is feature 2"),
    };

    private <T> Set<T> arrayToSet(T... array) {
        return new HashSet<>(Arrays.asList(array));
    }

    private void assertFeaturesHelper(Set<String> expectedFeaturesSet, boolean enabled) {
        Set<String> actualFeaturesSet =
                new HashSet<>(
                        FlagOverrideHelper.getCommaDelimitedSwitchValue(
                                enabled ? "enable-features" : "disable-features"));
        Assert.assertEquals(
                String.format("Wrong set of %s features", enabled ? "enabled" : "disabled"),
                expectedFeaturesSet,
                actualFeaturesSet);
    }

    private void assertEnabledFeatures(Set<String> expectedFeaturesSet) {
        assertFeaturesHelper(expectedFeaturesSet, true);
    }

    private void assertDisabledFeatures(Set<String> expectedFeaturesSet) {
        assertFeaturesHelper(expectedFeaturesSet, false);
    }

    @Test
    @SmallTest
    public void testGetCommaDelimitedSwitchValue() {
        CommandLine.getInstance().appendSwitchWithValue("foo", "val1,val2");
        List<String> values = FlagOverrideHelper.getCommaDelimitedSwitchValue("foo");
        Assert.assertEquals(Arrays.asList("val1", "val2"), values);
    }

    @Test
    @SmallTest
    public void testSetCommaDelimitedSwitchValue() {
        FlagOverrideHelper.setCommaDelimitedSwitchValue("foo", Arrays.asList("val1", "val2"));
        Assert.assertEquals("val1,val2", CommandLine.getInstance().getSwitchValue("foo"));
    }

    @Test
    @SmallTest
    public void testUnknownFlag() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("unknown-flag", true);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        try {
            helper.applyFlagOverrides(map); // should throw an exception

            Assert.fail("Expected a RuntimeException for 'unknown-flag'");
        } catch (RuntimeException e) {
            // This is expected.
        }
    }

    @Test
    @SmallTest
    public void testAddFlag() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("flag-1", true);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        helper.applyFlagOverrides(map);
        Assert.assertTrue(
                "The 'flag-1' commandline flag should be applied",
                CommandLine.getInstance().hasSwitch("flag-1"));
    }

    @Test
    @SmallTest
    public void testAddFlag_noValue() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("flag-1", true);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        helper.applyFlagOverrides(map);
        Assert.assertTrue(
                "The 'flag-1' commandline flag should be applied",
                CommandLine.getInstance().hasSwitch("flag-1"));
        Assert.assertNull(
                "The 'flag-1' commandline flag should not have a value",
                CommandLine.getInstance().getSwitchValue("flag-1"));
    }

    @Test
    @SmallTest
    public void testAddFlag_withValue() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("flag-3", true);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        helper.applyFlagOverrides(map);
        Assert.assertEquals(
                "The 'flag-3' commandline flag should have a value",
                "some-value",
                CommandLine.getInstance().getSwitchValue("flag-3"));
    }

    @Test
    @SmallTest
    public void testRemoveFlag_notYetAdded() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("flag-2", false);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        helper.applyFlagOverrides(map);
        Assert.assertFalse(
                "The 'flag-2' commandline flag should not be applied",
                CommandLine.getInstance().hasSwitch("flag-2"));
    }

    @Test
    @SmallTest
    public void testRemoveFlag_alreadyAdded() {
        CommandLine.getInstance().appendSwitch("flag-2");
        Map<String, Boolean> map = new HashMap<>();
        map.put("flag-2", false);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        helper.applyFlagOverrides(map);
        Assert.assertFalse(
                "The 'flag-2' commandline flag should not be applied",
                CommandLine.getInstance().hasSwitch("flag-2"));
    }

    @Test
    @SmallTest
    public void testEnableBaseFeature_notYetEnabled() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("feature-1", true);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        helper.applyFlagOverrides(map);

        assertEnabledFeatures(arrayToSet("feature-1"));
        assertDisabledFeatures(arrayToSet());
    }

    @Test
    @SmallTest
    public void testEnableBaseFeature_multiple() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("feature-1", true);
        map.put("feature-2", true);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        helper.applyFlagOverrides(map);

        assertEnabledFeatures(arrayToSet("feature-1", "feature-2"));
        assertDisabledFeatures(arrayToSet());
    }

    @Test
    @SmallTest
    public void testEnableBaseFeature_alreadyEnabled() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("feature-1", true);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        CommandLine.getInstance().appendSwitchWithValue("enable-features", "feature-1");
        helper.applyFlagOverrides(map);

        assertEnabledFeatures(arrayToSet("feature-1"));
        assertDisabledFeatures(arrayToSet());
    }

    @Test
    @SmallTest
    public void testEnableBaseFeature_alreadyDisabled() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("feature-1", true);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        CommandLine.getInstance().appendSwitchWithValue("disable-features", "feature-1");
        helper.applyFlagOverrides(map);

        assertEnabledFeatures(arrayToSet("feature-1"));
        assertDisabledFeatures(arrayToSet());
    }

    @Test
    @SmallTest
    public void testDisableBaseFeature_notYetEnabled() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("feature-2", false);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        helper.applyFlagOverrides(map);

        assertEnabledFeatures(arrayToSet());
        assertDisabledFeatures(arrayToSet("feature-2"));
    }

    @Test
    @SmallTest
    public void testDisableBaseFeature_multiple() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("feature-1", false);
        map.put("feature-2", false);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        helper.applyFlagOverrides(map);

        assertEnabledFeatures(arrayToSet());
        assertDisabledFeatures(arrayToSet("feature-1", "feature-2"));
    }

    @Test
    @SmallTest
    public void testDisableBaseFeature_alreadyEnabled() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("feature-2", false);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        CommandLine.getInstance().appendSwitchWithValue("enable-features", "feature-2");
        helper.applyFlagOverrides(map);

        assertEnabledFeatures(arrayToSet());
        assertDisabledFeatures(arrayToSet("feature-2"));
    }

    @Test
    @SmallTest
    public void testDisableBaseFeature_alreadyDisabled() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("feature-2", false);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        CommandLine.getInstance().appendSwitchWithValue("disable-features", "feature-2");
        helper.applyFlagOverrides(map);

        assertEnabledFeatures(arrayToSet());
        assertDisabledFeatures(arrayToSet("feature-2"));
    }

    @Test
    @SmallTest
    public void testLotsOfFlagsAndFeatures() {
        Map<String, Boolean> map = new HashMap<>();
        map.put("flag-1", true);
        map.put("flag-2", false);
        map.put("feature-1", true);
        map.put("feature-2", false);
        FlagOverrideHelper helper = new FlagOverrideHelper(sMockFlagList);
        helper.applyFlagOverrides(map);

        Assert.assertTrue(
                "The 'flag-1' commandline flag should be applied",
                CommandLine.getInstance().hasSwitch("flag-1"));
        Assert.assertFalse(
                "The 'flag-2' commandline flag should not be applied",
                CommandLine.getInstance().hasSwitch("flag-2"));
        assertEnabledFeatures(arrayToSet("feature-1"));
        assertDisabledFeatures(arrayToSet("feature-2"));
    }
}
