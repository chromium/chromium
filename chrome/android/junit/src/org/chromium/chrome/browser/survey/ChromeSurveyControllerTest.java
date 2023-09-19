// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import static org.mockito.Mockito.doReturn;

import android.app.Activity;
import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.hats.SurveyClientFactory;
import org.chromium.chrome.browser.ui.hats.SurveyThrottler;
import org.chromium.chrome.browser.ui.hats.TestSurveyUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.messages.MessageDispatcher;

/**
 * Unit tests for {@link ChromeSurveyController} and {@link SurveyThrottler}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.ANDROID_HATS_REFACTOR)
public class ChromeSurveyControllerTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock
    TabModelSelector mSelector;
    @Mock
    ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    Activity mActivity;
    @Mock
    MessageDispatcher mMessageDispatcher;

    @Before
    public void before() {
        doReturn(Mockito.mock(Resources.class)).when(mActivity).getResources();
        SurveyClientFactory.initialize(null);
    }

    @Test
    public void testInitialization() {
        TestSurveyUtils.setTestSurveyConfigForTrigger(
                "startup_survey", new String[0], new String[0]);
        ChromeSurveyController controller = ChromeSurveyController.initialize(
                mSelector, mActivityLifecycleDispatcher, mActivity, mMessageDispatcher);
        Assert.assertNotNull(controller);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.ANDROID_HATS_REFACTOR)
    public void doNotInitializationWhenFeatureDisabled() {
        TestSurveyUtils.setTestSurveyConfigForTrigger(
                "startup_survey", new String[0], new String[0]);
        ChromeSurveyController controller = ChromeSurveyController.initialize(
                mSelector, mActivityLifecycleDispatcher, mActivity, mMessageDispatcher);
        Assert.assertNull(controller);
    }
}
