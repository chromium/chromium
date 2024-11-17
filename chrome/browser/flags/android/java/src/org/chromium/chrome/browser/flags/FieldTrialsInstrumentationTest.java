// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureMap;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.flags.ChromeCachedFlags;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.components.cached_flags.DoubleCachedFieldTrialParameter;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;
import org.chromium.components.cached_flags.StringCachedFieldTrialParameter;

/** Integration tests for {@link org.chromium.base.test.util.FieldTrials}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@DoNotBatch(
        reason =
                "Test flag configurations in isolation for predictability and control of native"
                        + " being loaded or not.")
public final class FieldTrialsInstrumentationTest {
    private static final String FEATURE_1 = ChromeFeatureList.TEST_DEFAULT_DISABLED;
    private static final String FEATURE_2 = ChromeFeatureList.TEST_DEFAULT_ENABLED;

    private static final FeatureMap FEATURE_MAP =
            new FeatureMap() {
                @Override
                protected long getNativeMap() {
                    throw new UnsupportedOperationException(
                            "FeatureMap stub for testing does not support getting the flag value"
                                    + " across the native boundary, provide test override values"
                                    + " instead.");
                }
            };

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Test
    @SmallTest
    @EnableFeatures(FEATURE_1 + ":a1/b1/a2/b2")
    public void testJava_EnableWithParams() {
        Assert.assertEquals("b1", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a1"));
        Assert.assertEquals("b2", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a2"));
    }

    @Test
    @SmallTest
    @EnableFeatures(FEATURE_1 + ":a1/b1/a2/b2")
    public void testNative_EnableWithParams() {
        // @Param overrides as Java level, but should also override at native level. Remove the
        // override at Java level to check the override at native level.
        FeatureList.setTestValues(null);
        mActivityTestRule.startMainActivityOnBlankPage();

        Assert.assertEquals("b1", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a1"));
        Assert.assertEquals("b2", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a2"));
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add("enable-features=" + FEATURE_1 + ":a1/b1/a2/b2")
    public void testJava_CommandLine_EnableWithParams() {
        Assert.assertEquals("b1", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a1"));
        Assert.assertEquals("b2", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a2"));
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add("enable-features=" + FEATURE_1 + ":a1/b1/a2/b2")
    public void testNative_CommandLine_EnableWithParams() {
        // @CommandLine overrides as Java level, but should also override at native level. Remove
        // the override at Java level to check the override at native level.
        mActivityTestRule.startMainActivityOnBlankPage();
        FeatureList.setTestValues(null);

        Assert.assertEquals("b1", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a1"));
        Assert.assertEquals("b2", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a2"));
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add(
            "enable-features=" + FEATURE_1 + ":param-with-escaped-characters/%25%3A%2F%2E%2C")
    public void testJava_ParamValueIsUnescaped_CommandLine_EnableWithParams() {
        Assert.assertEquals(
                "%:/.,",
                ChromeFeatureList.getFieldTrialParamByFeature(
                        FEATURE_1, "param-with-escaped-characters"));
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add(
            "enable-features=" + FEATURE_1 + ":param-with-escaped-characters/%25%3A%2F%2E%2C")
    public void testNative_ParamValueIsUnescaped_CommandLine_EnableWithParams() {
        // @CommandLine overrides as Java level, but should also override at native level. Remove
        // the override at Java level to check the override at native level.
        mActivityTestRule.startMainActivityOnBlankPage();
        FeatureList.setTestValues(null);

        Assert.assertEquals(
                "%:/.,",
                ChromeFeatureList.getFieldTrialParamByFeature(
                        FEATURE_1, "param-with-escaped-characters"));
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=" + FEATURE_1 + "<Study",
        "force-fieldtrials=Study/Group"
    })
    public void testJava_FeatureWithoutParams_CommandLine_EnableAndFieldTrial() {
        Assert.assertTrue(ChromeFeatureList.isEnabled(FEATURE_1));
        Assert.assertTrue(ChromeFeatureList.sTestDefaultDisabled.isEnabled());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=" + FEATURE_1 + "<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:a1/b1"
    })
    public void testJava_OneFeatureTrialGroup_CommandLine_FieldTrialParams() {
        Assert.assertTrue(ChromeFeatureList.sTestDefaultDisabled.isEnabled());
        Assert.assertEquals("b1", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a1"));

        Assert.assertTrue(ChromeFeatureList.sTestDefaultDisabled.isEnabled());
        StringCachedFieldTrialParameter parameterA1 =
                new StringCachedFieldTrialParameter(FEATURE_MAP, FEATURE_1, "a1", "default");
        Assert.assertEquals("b1", parameterA1.getValue());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=" + FEATURE_1 + "<Study," + FEATURE_2 + "<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:a1/b1/a2/b2"
    })
    public void testJava_TwoFeaturesWithSameTrialGroup_CommandLine_FieldTrialParams() {
        Assert.assertTrue(ChromeFeatureList.isEnabled(FEATURE_1));
        Assert.assertEquals("b1", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a1"));
        Assert.assertTrue(ChromeFeatureList.isEnabled(FEATURE_1));
        Assert.assertEquals("b2", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a2"));
        Assert.assertTrue(ChromeFeatureList.isEnabled(FEATURE_2));
        Assert.assertEquals("b1", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a1"));
        Assert.assertTrue(ChromeFeatureList.isEnabled(FEATURE_2));
        Assert.assertEquals("b2", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a2"));

        Assert.assertTrue(ChromeFeatureList.sTestDefaultDisabled.isEnabled());
        Assert.assertTrue(ChromeFeatureList.sTestDefaultEnabled.isEnabled());
        StringCachedFieldTrialParameter parameterA1 =
                new StringCachedFieldTrialParameter(FEATURE_MAP, FEATURE_1, "a1", "");
        Assert.assertEquals("b1", parameterA1.getValue());

        StringCachedFieldTrialParameter parameterA2 =
                new StringCachedFieldTrialParameter(FEATURE_MAP, FEATURE_1, "a2", "");
        Assert.assertEquals("b2", parameterA2.getValue());

        StringCachedFieldTrialParameter parameterB1 =
                new StringCachedFieldTrialParameter(FEATURE_MAP, FEATURE_2, "a1", "");
        Assert.assertEquals("b1", parameterB1.getValue());

        StringCachedFieldTrialParameter parameterB2 =
                new StringCachedFieldTrialParameter(FEATURE_MAP, FEATURE_2, "a2", "");
        Assert.assertEquals("b2", parameterB2.getValue());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=" + FEATURE_1 + "<Study1," + FEATURE_2 + "<Study2",
        "force-fieldtrials=Study1/Group1/Study2/Group2",
        "force-fieldtrial-params=Study1.Group1:a1/0.5/a2/100,Study2.Group2:a3/true"
    })
    public void
            testJava_TwoFeaturesWithDifferentTrialGroupsAndMutipleTypesOfValues_CommandLine_FieldTrialParams() {
        Assert.assertTrue(ChromeFeatureList.isEnabled(FEATURE_1));
        Assert.assertTrue(ChromeFeatureList.isEnabled(FEATURE_2));
        Assert.assertEquals("0.5", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a1"));
        Assert.assertEquals("100", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_1, "a2"));
        Assert.assertEquals("true", ChromeFeatureList.getFieldTrialParamByFeature(FEATURE_2, "a3"));

        Assert.assertTrue(ChromeFeatureList.sTestDefaultDisabled.isEnabled());
        Assert.assertTrue(ChromeFeatureList.sTestDefaultEnabled.isEnabled());
        DoubleCachedFieldTrialParameter parameterA1 =
                new DoubleCachedFieldTrialParameter(FEATURE_MAP, FEATURE_1, "a1", 0.1);
        Assert.assertEquals(0.5, parameterA1.getValue(), 1e-7);

        IntCachedFieldTrialParameter parameterA2 =
                new IntCachedFieldTrialParameter(FEATURE_MAP, FEATURE_1, "a2", 0);
        Assert.assertEquals(100, parameterA2.getValue());

        BooleanCachedFieldTrialParameter parameterB =
                new BooleanCachedFieldTrialParameter(FEATURE_MAP, FEATURE_2, "a3", false);
        Assert.assertEquals(true, parameterB.getValue());
    }

    @Test
    @SmallTest
    public void testGetLastUpdateFromNativeTimeMillis() {
        mActivityTestRule.startMainActivityOnBlankPage();
        Assert.assertNotEquals(0, ChromeCachedFlags.getLastCachedMinimalBrowserFlagsTimeMillis());
    }
}
