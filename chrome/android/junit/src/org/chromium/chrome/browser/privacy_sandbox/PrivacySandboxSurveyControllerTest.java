// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.hats.TestSurveyUtils.setTestSurveyConfigForTrigger;

import android.app.Activity;
import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.hats.SurveyClientFactory;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** Unit tests for {@link PrivacySandboxSurveyController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SENTIMENT_SURVEY)
public class PrivacySandboxSurveyControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock UserPrefs.Natives mUserPrefsJniMock;
    @Mock PrefService mPrefServiceMock;
    @Mock TabModelSelector mSelector;
    @Mock ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock Activity mActivity;
    @Mock Profile mProfile;
    @Mock MessageDispatcher mMessageDispatcher;
    @Mock ActivityTabProvider mTabProvider;

    private static final String SENTIMENT_SURVEY_TRIGGER_ID = "privacy-sandbox-sentiment-survey";

    @Before
    public void before() {
        PrivacySandboxSurveyController.setEnableForTesting();
        doReturn(Mockito.mock(Resources.class)).when(mActivity).getResources();
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefServiceMock);
        when(mPrefServiceMock.getBoolean(Pref.FEEDBACK_SURVEYS_ENABLED)).thenReturn(true);
        SurveyClientFactory.initialize(null);
    }

    @Test
    public void surveyControllerInitializes() {
        setTestSurveyConfigForTrigger(SENTIMENT_SURVEY_TRIGGER_ID, new String[0], new String[0]);
        PrivacySandboxSurveyController controller =
                PrivacySandboxSurveyController.initialize(
                        mSelector,
                        mActivityLifecycleDispatcher,
                        mActivity,
                        mMessageDispatcher,
                        mTabProvider,
                        mProfile);
        Assert.assertNotNull(controller);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SENTIMENT_SURVEY)
    public void surveyControllerDoesNotInitalizeWhenFeatureDisabled() {
        setTestSurveyConfigForTrigger(SENTIMENT_SURVEY_TRIGGER_ID, new String[0], new String[0]);
        PrivacySandboxSurveyController controller =
                PrivacySandboxSurveyController.initialize(
                        mSelector,
                        mActivityLifecycleDispatcher,
                        mActivity,
                        mMessageDispatcher,
                        mTabProvider,
                        mProfile);
        Assert.assertNull(controller);
    }
}
