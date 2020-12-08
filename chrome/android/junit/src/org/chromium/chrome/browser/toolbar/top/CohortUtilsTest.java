// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.feature_engagement.Tracker;

import java.util.HashSet;
import java.util.Set;

/** Unit tests for CohortUtils. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {CohortUtilsTest.ShadowChromeFeatureList.class})
public class CohortUtilsTest {
    private static final String FEATURE_NAME = "feature12345";
    private static final String PARAM_NAME = "param12345";
    private static final String COHORT_VALUE = "cohort12345";

    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        static final Set<String> sRequestedIsEnabledFeatureNames = new HashSet<>();
        static final Set<String> sEnabledFeatureNames = new HashSet<>();
        static String sParamValue;

        @Implementation
        public static boolean isEnabled(String featureName) {
            sRequestedIsEnabledFeatureNames.add(featureName);
            return sEnabledFeatureNames.contains(featureName);
        }

        @Implementation
        public static String getFieldTrialParamByFeature(String featureName, String paramName) {
            Assert.assertEquals(FEATURE_NAME, featureName);
            Assert.assertEquals(PARAM_NAME, paramName);
            return sParamValue;
        }
    }

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Tracker mTracker;

    @Before
    public void setUp() {
        ShadowChromeFeatureList.sRequestedIsEnabledFeatureNames.clear();
        ShadowChromeFeatureList.sEnabledFeatureNames.clear();
        TrackerFactory.setTrackerForTests(mTracker);
    }

    @Test
    public void testTagCohortGroupIfTriggered() {
        ShadowChromeFeatureList.sEnabledFeatureNames.add(FEATURE_NAME);
        ShadowChromeFeatureList.sEnabledFeatureNames.add(COHORT_VALUE);
        ShadowChromeFeatureList.sParamValue = COHORT_VALUE;
        when(mTracker.hasEverTriggered(eq(FEATURE_NAME), anyBoolean())).thenReturn(true);

        CohortUtils.tagCohortGroupIfTriggered(mTracker, FEATURE_NAME, PARAM_NAME);

        Assert.assertTrue(
                ShadowChromeFeatureList.sRequestedIsEnabledFeatureNames.contains(COHORT_VALUE));
    }

    @Test
    public void testTagCohortGroupIfTriggeredNotTriggered() {
        ShadowChromeFeatureList.sEnabledFeatureNames.add(FEATURE_NAME);
        ShadowChromeFeatureList.sParamValue = COHORT_VALUE;
        when(mTracker.hasEverTriggered(eq(FEATURE_NAME), anyBoolean())).thenReturn(false);

        CohortUtils.tagCohortGroupIfTriggered(mTracker, FEATURE_NAME, PARAM_NAME);

        Assert.assertFalse(
                ShadowChromeFeatureList.sRequestedIsEnabledFeatureNames.contains(COHORT_VALUE));
    }

    @Test
    public void testTagCohortGroupIfTriggeredDisabled() {
        ShadowChromeFeatureList.sParamValue = COHORT_VALUE;

        CohortUtils.tagCohortGroupIfTriggered(mTracker, FEATURE_NAME, PARAM_NAME);

        verify(mTracker, never()).hasEverTriggered(any(), anyBoolean());
        Assert.assertFalse(
                ShadowChromeFeatureList.sRequestedIsEnabledFeatureNames.contains(COHORT_VALUE));
    }
}