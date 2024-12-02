// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.hats.TestSurveyUtils.setSurveyConfigForceUsingTestingConfig;
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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.hats.SurveyClient;
import org.chromium.chrome.browser.ui.hats.SurveyClientFactory;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.url.GURL;

import java.util.Collections;

/** Unit tests for {@link PrivacySandboxSurveyController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SENTIMENT_SURVEY)
public class PrivacySandboxSurveyControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock UserPrefs.Natives mUserPrefsJniMock;
    @Mock PrefService mPrefService;
    @Mock TabModelSelector mTabModelSelector;
    @Mock ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock Activity mActivity;
    @Mock Profile mProfile;
    @Mock MessageDispatcher mMessageDispatcher;
    ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();
    @Mock SurveyClient mSurveyClient;
    @Mock SurveyClientFactory mSurveyClientFactory;
    @Mock IdentityServicesProvider mIdentityServicesProvider;
    @Mock IdentityManager mIdentityManager;

    private static final String SENTIMENT_SURVEY_TRIGGER_ID = "privacy-sandbox-sentiment-survey";

    @Before
    public void before() {
        PrivacySandboxSurveyController.setEnableForTesting();
        doReturn(Mockito.mock(Resources.class)).when(mActivity).getResources();
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.FEEDBACK_SURVEYS_ENABLED)).thenReturn(true);
        SurveyClientFactory.setInstanceForTesting(mSurveyClientFactory);
        doReturn(mSurveyClient).when(mSurveyClientFactory).createClient(any(), any(), any());
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(IdentityServicesProvider.get().getIdentityManager(mProfile))
                .thenReturn(mIdentityManager);
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
    }

    @Test
    public void surveyControllerInitializes() {
        setTestSurveyConfigForTrigger(SENTIMENT_SURVEY_TRIGGER_ID, new String[0], new String[0]);
        PrivacySandboxSurveyController controller =
                PrivacySandboxSurveyController.initialize(
                        mTabModelSelector,
                        mActivityLifecycleDispatcher,
                        mActivity,
                        mMessageDispatcher,
                        mActivityTabProvider,
                        mProfile);
        Assert.assertNotNull(controller);
        controller.destroy();
    }

    @Test
    public void surveyControllerDoesNotInitializesForOtrProfiles() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        PrivacySandboxSurveyController controller =
                PrivacySandboxSurveyController.initialize(
                        mTabModelSelector,
                        mActivityLifecycleDispatcher,
                        mActivity,
                        mMessageDispatcher,
                        mActivityTabProvider,
                        mProfile);
        Assert.assertNull(controller);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_SENTIMENT_SURVEY)
    public void surveyControllerDoesNotInitalizeWhenFeatureDisabled() {
        setTestSurveyConfigForTrigger(SENTIMENT_SURVEY_TRIGGER_ID, new String[0], new String[0]);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "PrivacySandbox.SentimentSurvey.Status",
                        PrivacySandboxSentimentSurveyStatus.FEATURE_DISABLED);
        PrivacySandboxSurveyController controller =
                PrivacySandboxSurveyController.initialize(
                        mTabModelSelector,
                        mActivityLifecycleDispatcher,
                        mActivity,
                        mMessageDispatcher,
                        mActivityTabProvider,
                        mProfile);
        Assert.assertNull(controller);
        histogramWatcher.assertExpected();
    }

    @Test
    public void surveyControllerLaunchesSurvey() {
        setTestSurveyConfigForTrigger(SENTIMENT_SURVEY_TRIGGER_ID, new String[0], new String[0]);
        MockTab startTab = new MockTab(0, mProfile);
        mActivityTabProvider.set(startTab);
        PrivacySandboxSurveyController controller =
                PrivacySandboxSurveyController.initialize(
                        mTabModelSelector,
                        mActivityLifecycleDispatcher,
                        mActivity,
                        mMessageDispatcher,
                        mActivityTabProvider,
                        mProfile);
        // Record visiting a NTP.
        MockTab firstNtpTab = new MockTab(1, mProfile);
        firstNtpTab.setUrl(new GURL(UrlConstants.NTP_URL));
        mActivityTabProvider.set(firstNtpTab);
        MockTab secondNtpTab = new MockTab(2, mProfile);
        secondNtpTab.setUrl(new GURL(UrlConstants.NTP_URL));
        // Set the survey config to null to trigger the histogram
        mActivityTabProvider.set(secondNtpTab);
        verify(mSurveyClient)
                .showSurvey(
                        mActivity,
                        mActivityLifecycleDispatcher,
                        controller.getSentimentSurveyPsb(),
                        Collections.emptyMap());
        controller.destroy();
    }

    @Test
    public void surveyControllerEmitsInvalidConfigHistogram() {
        // Ensure that we use the default null config for testing.
        setSurveyConfigForceUsingTestingConfig(true);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "PrivacySandbox.SentimentSurvey.Status",
                        PrivacySandboxSentimentSurveyStatus.INVALID_SURVEY_CONFIG);
        MockTab startTab = new MockTab(0, mProfile);
        mActivityTabProvider.set(startTab);
        PrivacySandboxSurveyController controller =
                PrivacySandboxSurveyController.initialize(
                        mTabModelSelector,
                        mActivityLifecycleDispatcher,
                        mActivity,
                        mMessageDispatcher,
                        mActivityTabProvider,
                        mProfile);
        MockTab firstNtpTab = new MockTab(1, mProfile);
        firstNtpTab.setUrl(new GURL(UrlConstants.NTP_URL));
        mActivityTabProvider.set(firstNtpTab);
        MockTab secondNtpTab = new MockTab(2, mProfile);
        secondNtpTab.setUrl(new GURL(UrlConstants.NTP_URL));
        mActivityTabProvider.set(secondNtpTab);
        verify(mSurveyClient, times(0)).showSurvey(any(), any(), any(), any());
        histogramWatcher.assertExpected();
        controller.destroy();
    }

    @Test
    public void surveyControllerDoesNotTriggerSurveyIfTabIsNull() {
        setTestSurveyConfigForTrigger(SENTIMENT_SURVEY_TRIGGER_ID, new String[0], new String[0]);
        MockTab startTab = new MockTab(0, mProfile);
        mActivityTabProvider.set(startTab);
        PrivacySandboxSurveyController controller =
                PrivacySandboxSurveyController.initialize(
                        mTabModelSelector,
                        mActivityLifecycleDispatcher,
                        mActivity,
                        mMessageDispatcher,
                        mActivityTabProvider,
                        mProfile);
        MockTab firstNtpTab = new MockTab(1, mProfile);
        firstNtpTab.setUrl(new GURL(UrlConstants.NTP_URL));
        mActivityTabProvider.set(firstNtpTab);
        // Record a null tab, normally if we see a 2nd NTP we will attempt to trigger a survey,
        // however we should no-op if we see a null tab.
        mActivityTabProvider.set(null);
        verify(mSurveyClient, times(0)).showSurvey(any(), any(), any(), any());
        controller.destroy();
    }

    @Test
    public void surveyControllerFetchesTopicsBit() {
        PrivacySandboxSurveyController controller =
                PrivacySandboxSurveyController.initialize(
                        mTabModelSelector,
                        mActivityLifecycleDispatcher,
                        mActivity,
                        mMessageDispatcher,
                        mActivityTabProvider,
                        mProfile);
        when(mPrefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED)).thenReturn(true);
        Assert.assertTrue(controller.getSentimentSurveyPsb().get("Topics enabled"));
        when(mPrefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED)).thenReturn(false);
        Assert.assertFalse(controller.getSentimentSurveyPsb().get("Topics enabled"));
        controller.destroy();
    }

    @Test
    public void surveyControllerFetchesProtectedAudienceBit() {
        PrivacySandboxSurveyController controller =
                PrivacySandboxSurveyController.initialize(
                        mTabModelSelector,
                        mActivityLifecycleDispatcher,
                        mActivity,
                        mMessageDispatcher,
                        mActivityTabProvider,
                        mProfile);
        when(mPrefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_FLEDGE_ENABLED)).thenReturn(true);
        Assert.assertTrue(controller.getSentimentSurveyPsb().get("Protected audience enabled"));
        when(mPrefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_FLEDGE_ENABLED)).thenReturn(false);
        Assert.assertFalse(controller.getSentimentSurveyPsb().get("Protected audience enabled"));
        controller.destroy();
    }

    @Test
    public void surveyControllerFetchesMeasurementBit() {
        PrivacySandboxSurveyController controller =
                PrivacySandboxSurveyController.initialize(
                        mTabModelSelector,
                        mActivityLifecycleDispatcher,
                        mActivity,
                        mMessageDispatcher,
                        mActivityTabProvider,
                        mProfile);
        when(mPrefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED))
                .thenReturn(true);
        Assert.assertTrue(controller.getSentimentSurveyPsb().get("Measurement enabled"));
        when(mPrefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_AD_MEASUREMENT_ENABLED))
                .thenReturn(false);
        Assert.assertFalse(controller.getSentimentSurveyPsb().get("Measurement enabled"));
        controller.destroy();
    }

    @Test
    public void surveyControllerFetchesLoggedInBit() {
        PrivacySandboxSurveyController controller =
                PrivacySandboxSurveyController.initialize(
                        mTabModelSelector,
                        mActivityLifecycleDispatcher,
                        mActivity,
                        mMessageDispatcher,
                        mActivityTabProvider,
                        mProfile);
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        Assert.assertTrue(controller.getSentimentSurveyPsb().get("Signed in"));
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
        Assert.assertFalse(controller.getSentimentSurveyPsb().get("Signed in"));
        controller.destroy();
    }
}
