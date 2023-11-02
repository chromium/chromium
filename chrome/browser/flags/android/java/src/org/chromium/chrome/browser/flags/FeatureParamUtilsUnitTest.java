// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests the behavior of {@link FeatureParamUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {FeatureParamUtilsUnitTest.ShadowChromeFeatureList.class})
public class FeatureParamUtilsUnitTest {
    private static final String FEATURE_NAME = "feature12345";
    private static final String PARAM_NAME = "param12345";

    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        static String sParamValue;

        @Implementation
        public static String getFieldTrialParamByFeature(String featureName, String paramName) {
            Assert.assertEquals(FEATURE_NAME, featureName);
            Assert.assertEquals(PARAM_NAME, paramName);
            return sParamValue;
        }
    }

    @Test
    public void testParamExistsAndDoesNotMatch() {
        ShadowChromeFeatureList.sParamValue = "";
        Assert.assertFalse("Empty string param should be treated as not existing",
                FeatureParamUtils.paramExistsAndDoesNotMatch(FEATURE_NAME, PARAM_NAME, false));
        Assert.assertFalse("Empty string param should be treated as not existing",
                FeatureParamUtils.paramExistsAndDoesNotMatch(FEATURE_NAME, PARAM_NAME, true));

        ShadowChromeFeatureList.sParamValue = "true";
        Assert.assertTrue("True and false do not match",
                FeatureParamUtils.paramExistsAndDoesNotMatch(FEATURE_NAME, PARAM_NAME, false));
        Assert.assertFalse("Both are true and should be treated as matching",
                FeatureParamUtils.paramExistsAndDoesNotMatch(FEATURE_NAME, PARAM_NAME, true));

        ShadowChromeFeatureList.sParamValue = "false";
        Assert.assertFalse("Both are false and should be treated as matching",
                FeatureParamUtils.paramExistsAndDoesNotMatch(FEATURE_NAME, PARAM_NAME, false));
        Assert.assertTrue("True and false do not match",
                FeatureParamUtils.paramExistsAndDoesNotMatch(FEATURE_NAME, PARAM_NAME, true));
    }

    @Test
    public void testGetFieldTrialParamByFeatureAsBooleanOrNull() {
        ShadowChromeFeatureList.sParamValue = "";
        Assert.assertNull("Empty string should be treated as not existing",
                FeatureParamUtils.getFieldTrialParamByFeatureAsBooleanOrNull(
                        FEATURE_NAME, PARAM_NAME));

        ShadowChromeFeatureList.sParamValue = null;
        Assert.assertNull("Null should should be treated as not existing",
                FeatureParamUtils.getFieldTrialParamByFeatureAsBooleanOrNull(
                        FEATURE_NAME, PARAM_NAME));

        ShadowChromeFeatureList.sParamValue = "true";
        Assert.assertTrue("Should have parsed to true",
                FeatureParamUtils.getFieldTrialParamByFeatureAsBooleanOrNull(
                        FEATURE_NAME, PARAM_NAME));

        ShadowChromeFeatureList.sParamValue = "True";
        Assert.assertTrue("Should have parsed to true",
                FeatureParamUtils.getFieldTrialParamByFeatureAsBooleanOrNull(
                        FEATURE_NAME, PARAM_NAME));

        ShadowChromeFeatureList.sParamValue = "false";
        Assert.assertFalse("Should have parsed to false",
                FeatureParamUtils.getFieldTrialParamByFeatureAsBooleanOrNull(
                        FEATURE_NAME, PARAM_NAME));

        ShadowChromeFeatureList.sParamValue = "False";
        Assert.assertFalse("Should have parsed to false",
                FeatureParamUtils.getFieldTrialParamByFeatureAsBooleanOrNull(
                        FEATURE_NAME, PARAM_NAME));

        ShadowChromeFeatureList.sParamValue = "null";
        Assert.assertFalse("Should have parsed to false",
                FeatureParamUtils.getFieldTrialParamByFeatureAsBooleanOrNull(
                        FEATURE_NAME, PARAM_NAME));

        ShadowChromeFeatureList.sParamValue = "bogus";
        Assert.assertFalse("Should have parsed to false",
                FeatureParamUtils.getFieldTrialParamByFeatureAsBooleanOrNull(
                        FEATURE_NAME, PARAM_NAME));

        ShadowChromeFeatureList.sParamValue = " ∑ \t\n";
        Assert.assertFalse("Should have parsed to false",
                FeatureParamUtils.getFieldTrialParamByFeatureAsBooleanOrNull(
                        FEATURE_NAME, PARAM_NAME));
    }
}