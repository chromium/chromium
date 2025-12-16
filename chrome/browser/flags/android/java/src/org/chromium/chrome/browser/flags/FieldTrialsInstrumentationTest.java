// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureMap;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.flags.ChromeCachedFlags;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.cached_flags.BooleanCachedFeatureParam;
import org.chromium.components.cached_flags.DoubleCachedFeatureParam;
import org.chromium.components.cached_flags.IntCachedFeatureParam;
import org.chromium.components.cached_flags.StringCachedFeatureParam;

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
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

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
        FeatureOverrides.removeAllIncludingAnnotations();
        mActivityTestRule.startOnBlankPage();

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
        mActivityTestRule.startOnBlankPage();
        FeatureOverrides.removeAllIncludingAnnotations();

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
        mActivityTestRule.startOnBlankPage();
        FeatureOverrides.removeAllIncludingAnnotations();

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
        StringCachedFeatureParam parameterA1 =
                new StringCachedFeatureParam(FEATURE_MAP, FEATURE_1, "a1", "default");
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
        StringCachedFeatureParam parameterA1 =
                new StringCachedFeatureParam(FEATURE_MAP, FEATURE_1, "a1", "");
        Assert.assertEquals("b1", parameterA1.getValue());

        StringCachedFeatureParam parameterA2 =
                new StringCachedFeatureParam(FEATURE_MAP, FEATURE_1, "a2", "");
        Assert.assertEquals("b2", parameterA2.getValue());

        StringCachedFeatureParam parameterB1 =
                new StringCachedFeatureParam(FEATURE_MAP, FEATURE_2, "a1", "");
        Assert.assertEquals("b1", parameterB1.getValue());

        StringCachedFeatureParam parameterB2 =
                new StringCachedFeatureParam(FEATURE_MAP, FEATURE_2, "a2", "");
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
        DoubleCachedFeatureParam parameterA1 =
                new DoubleCachedFeatureParam(FEATURE_MAP, FEATURE_1, "a1", 0.1);
        Assert.assertEquals(0.5, parameterA1.getValue(), 1e-7);

        IntCachedFeatureParam parameterA2 =
                new IntCachedFeatureParam(FEATURE_MAP, FEATURE_1, "a2", 0);
        Assert.assertEquals(100, parameterA2.getValue());

        BooleanCachedFeatureParam parameterB =
                new BooleanCachedFeatureParam(FEATURE_MAP, FEATURE_2, "a3", false);
        Assert.assertEquals(true, parameterB.getValue());
    }

    @Test
    @SmallTest
    public void testGetLastUpdateFromNativeTimeMillis() {
        mActivityTestRule.startOnBlankPage();
        Assert.assertNotEquals(0, ChromeCachedFlags.getLastCachedMinimalBrowserFlagsTimeMillis());
    }
}
