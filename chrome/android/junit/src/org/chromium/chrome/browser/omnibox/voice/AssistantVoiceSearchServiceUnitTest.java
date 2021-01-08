// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_LAST_VERSION;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_SUPPORTED;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.drawable.Drawable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseSwitches;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.search_engines.TemplateUrlService;

/**
 * Tests for AssistantVoiceSearchService.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class, ShadowRecordHistogram.class})
@Features.EnableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
@CommandLineFlags.Add(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)
public class AssistantVoiceSearchServiceUnitTest {
    AssistantVoiceSearchService mAssistantVoiceSearchService;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    GSAState mGsaState;
    @Mock
    PackageManager mPackageManager;
    @Mock
    TemplateUrlService mTemplateUrlService;
    @Mock
    ExternalAuthUtils mExternalAuthUtils;

    SharedPreferencesManager mSharedPreferencesManager;
    PackageInfo mPackageInfo;
    Context mContext;

    private static class TestDeferredStartupHandler extends DeferredStartupHandler {
        @Override
        public void addDeferredTask(Runnable deferredTask) {
            deferredTask.run();
        }
    }

    @Before
    public void setUp() throws NameNotFoundException {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        DeferredStartupHandler.setInstanceForTests(new TestDeferredStartupHandler());
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();

        mContext = Mockito.spy(Robolectric.buildActivity(Activity.class).setup().get());

        doReturn(true).when(mExternalAuthUtils).isChromeGoogleSigned();
        doReturn(true).when(mExternalAuthUtils).isGoogleSigned(IntentHandler.PACKAGE_GSA);
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(false).when(mGsaState).isAgsaVersionBelowMinimum(any(), any());
        doReturn(true).when(mGsaState).canAgsaHandleIntent(any());
        doReturn(true).when(mGsaState).agsaSupportsAssistantVoiceSearch();
        doReturn(true).when(mGsaState).doesGsaAccountMatchChrome();
        mSharedPreferencesManager.writeBoolean(ASSISTANT_VOICE_SEARCH_ENABLED, true);

        mAssistantVoiceSearchService = new AssistantVoiceSearchService(mContext, mExternalAuthUtils,
                mTemplateUrlService, mGsaState, null, mSharedPreferencesManager);
    }

    @After
    public void tearDown() {
        mSharedPreferencesManager.removeKey(ASSISTANT_VOICE_SEARCH_SUPPORTED);
        mSharedPreferencesManager.removeKey(ASSISTANT_LAST_VERSION);
        AssistantVoiceSearchService.setAgsaSupportsAssistantVoiceSearchForTesting(null);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch() {
        Assert.assertTrue(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_DisabledByPref() {
        mSharedPreferencesManager.writeBoolean(ASSISTANT_VOICE_SEARCH_ENABLED, false);
        Assert.assertFalse(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_ChromeNotSigned() {
        doReturn(false).when(mExternalAuthUtils).isChromeGoogleSigned();

        Assert.assertFalse(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                        AssistantVoiceSearchService.EligibilityFailureReason
                                .CHROME_NOT_GOOGLE_SIGNED));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_AGSANotSigned() {
        doReturn(false).when(mExternalAuthUtils).isGoogleSigned(IntentHandler.PACKAGE_GSA);

        Assert.assertFalse(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                        AssistantVoiceSearchService.EligibilityFailureReason
                                .AGSA_NOT_GOOGLE_SIGNED));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_AGSARotiChromeNotEnabled() {
        AssistantVoiceSearchService.setAgsaSupportsAssistantVoiceSearchForTesting(false);

        Assert.assertFalse(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                        AssistantVoiceSearchService.EligibilityFailureReason
                                .AGSA_DOESNT_SUPPORT_VOICE_SEARCH));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void
    testStartVoiceRecognition_StartsAssistantVoiceSearch_AGSARotiChromeNotEnabledNotComplete() {
        AssistantVoiceSearchService.setAgsaSupportsAssistantVoiceSearchForTesting(null);

        Assert.assertFalse(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                        AssistantVoiceSearchService.EligibilityFailureReason
                                .AGSA_DOESNT_SUPPORT_VOICE_SEARCH_CHECK_NOT_COMPLETE));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_AccountMismatch() {
        doReturn(false).when(mGsaState).doesGsaAccountMatchChrome();

        Assert.assertFalse(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                        AssistantVoiceSearchService.EligibilityFailureReason.ACCOUNT_MISMATCH));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_TemporaryAccountMismatch() {
        doReturn(false).when(mGsaState).doesGsaAccountMatchChrome();

        Assert.assertFalse(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                        AssistantVoiceSearchService.EligibilityFailureReason.ACCOUNT_MISMATCH));

        doReturn(true).when(mGsaState).doesGsaAccountMatchChrome();
        Assert.assertTrue(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                        AssistantVoiceSearchService.EligibilityFailureReason.ACCOUNT_MISMATCH));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_VersionTooLow() {
        doReturn(true).when(mGsaState).isAgsaVersionBelowMinimum(any(), any());

        Assert.assertFalse(mAssistantVoiceSearchService.isDeviceEligibleForAssistant());
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                        AssistantVoiceSearchService.EligibilityFailureReason
                                .AGSA_VERSION_BELOW_MINIMUM));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void getMicButtonColorStateList_ColorfulMicEnabled() {
        mAssistantVoiceSearchService.setColorfulMicEnabledForTesting(true);
        Assert.assertNull(mAssistantVoiceSearchService.getMicButtonColorStateList(0, mContext));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void getCurrentMicDrawable() {
        Drawable greyMic = mAssistantVoiceSearchService.getCurrentMicDrawable();
        mAssistantVoiceSearchService.setColorfulMicEnabledForTesting(true);
        Drawable colorfulMic = mAssistantVoiceSearchService.getCurrentMicDrawable();

        Assert.assertNotEquals(greyMic, colorfulMic);
    }
}