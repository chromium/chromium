// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
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
import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchService.EligibilityFailureReason;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.GmsAvailabilityException;
import org.chromium.components.signin.identitymanager.IdentityManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for AssistantVoiceSearchService.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class, ShadowRecordHistogram.class})
@Features.EnableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
@CommandLineFlags.Add(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)
public class AssistantVoiceSearchServiceUnitTest {
    private static final int AGSA_VERSION_NUMBER = 11007;
    private static final List<Account> FAKE_ACCOUNTS_1 = Arrays.asList(Mockito.mock(Account.class));
    private static final List<Account> FAKE_ACCOUNTS_2 =
            Arrays.asList(Mockito.mock(Account.class), Mockito.mock(Account.class));

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
    @Mock
    IdentityManager mIdentityManager;
    @Mock
    AccountManagerFacade mAccountManagerFacade;

    AssistantVoiceSearchService mAssistantVoiceSearchService;
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
    public void setUp() throws Exception {
        ShadowRecordHistogram.reset();
        SysUtils.resetForTesting();
        MockitoAnnotations.initMocks(this);
        DeferredStartupHandler.setInstanceForTests(new TestDeferredStartupHandler());
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();

        mContext = Mockito.spy(Robolectric.buildActivity(Activity.class).setup().get());

        doReturn(mPackageManager).when(mContext).getPackageManager();
        doReturn(true).when(mExternalAuthUtils).isChromeGoogleSigned();
        doReturn(true).when(mExternalAuthUtils).isGoogleSigned(IntentHandler.PACKAGE_GSA);
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(true).when(mGsaState).isGsaInstalled();
        doReturn(false).when(mGsaState).isAgsaVersionBelowMinimum(any(), any());
        doReturn(AGSA_VERSION_NUMBER).when(mGsaState).parseAgsaMajorMinorVersionAsInteger(any());
        doReturn(true).when(mGsaState).canAgsaHandleIntent(any());
        doReturn(true).when(mIdentityManager).hasPrimaryAccount();
        doReturn(FAKE_ACCOUNTS_1).when(mAccountManagerFacade).getGoogleAccounts();
        doReturn(true).when(mAccountManagerFacade).isCachePopulated();
        mSharedPreferencesManager.writeBoolean(ASSISTANT_VOICE_SEARCH_ENABLED, true);

        mAssistantVoiceSearchService = new AssistantVoiceSearchService(mContext, mExternalAuthUtils,
                mTemplateUrlService, mGsaState, null, mSharedPreferencesManager, mIdentityManager,
                mAccountManagerFacade);
    }

    @After
    public void tearDown() {
        mSharedPreferencesManager.removeKey(ASSISTANT_VOICE_SEARCH_ENABLED);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    @Features.DisableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
    public void canRequestAssistantVoiceSearch_featureDisabled() {
        Assert.assertFalse(mAssistantVoiceSearchService.canRequestAssistantVoiceSearch());
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch() {
        Assert.assertTrue(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());
        List<Integer> reasons = new ArrayList<>();
        boolean eligible = mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                /* returnImmediately= */ false, /* outList= */ reasons);
        Assert.assertEquals(0, reasons.size());
        Assert.assertTrue(eligible);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_DisabledByPref() {
        mSharedPreferencesManager.writeBoolean(ASSISTANT_VOICE_SEARCH_ENABLED, false);
        Assert.assertFalse(mAssistantVoiceSearchService.shouldRequestAssistantVoiceSearch());

        List<Integer> reasons = new ArrayList<>();
        boolean eligible = mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                /* returnImmediately= */ false, /* outList= */ reasons);
        Assert.assertEquals(0, reasons.size());
        Assert.assertTrue(eligible);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_ChromeNotSigned() {
        doReturn(false).when(mExternalAuthUtils).isChromeGoogleSigned();

        List<Integer> reasons = new ArrayList<>();
        boolean eligible = mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                /* returnImmediately= */ false, /* outList= */ reasons);
        Assert.assertEquals(1, reasons.size());
        Assert.assertEquals(
                EligibilityFailureReason.CHROME_NOT_GOOGLE_SIGNED, (int) reasons.get(0));
        Assert.assertFalse(eligible);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testStartVoiceRecognition_StartsAssistantVoiceSearch_AGSANotSigned() {
        doReturn(false).when(mExternalAuthUtils).isGoogleSigned(IntentHandler.PACKAGE_GSA);

        List<Integer> reasons = new ArrayList<>();
        boolean eligible = mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                /* returnImmediately= */ false, /* outList= */ reasons);
        Assert.assertEquals(1, reasons.size());
        Assert.assertEquals(EligibilityFailureReason.AGSA_NOT_GOOGLE_SIGNED, (int) reasons.get(0));
        Assert.assertFalse(eligible);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_VersionTooLow() {
        doReturn(true).when(mGsaState).isAgsaVersionBelowMinimum(any(), any());

        List<Integer> reasons = new ArrayList<>();
        boolean eligible = mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                /* returnImmediately= */ false, /* outList= */ reasons);
        Assert.assertEquals(1, reasons.size());
        Assert.assertEquals(
                EligibilityFailureReason.AGSA_VERSION_BELOW_MINIMUM, (int) reasons.get(0));
        Assert.assertFalse(eligible);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_NonGoogleSearchEngine() {
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mAssistantVoiceSearchService.onTemplateURLServiceChanged();

        List<Integer> reasons = new ArrayList<>();
        boolean eligible = mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                /* returnImmediately= */ false, /* outList= */ reasons);
        Assert.assertEquals(1, reasons.size());
        Assert.assertEquals(
                EligibilityFailureReason.NON_GOOGLE_SEARCH_ENGINE, (int) reasons.get(0));
        Assert.assertFalse(eligible);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_NoChromeAccount() {
        doReturn(false).when(mIdentityManager).hasPrimaryAccount();

        List<Integer> reasons = new ArrayList<>();
        boolean eligible = mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                /* returnImmediately= */ false, /* outList= */ reasons);
        Assert.assertEquals(1, reasons.size());
        Assert.assertEquals(EligibilityFailureReason.NO_CHROME_ACCOUNT, (int) reasons.get(0));
        Assert.assertFalse(eligible);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_LowEndDevice() {
        CommandLine.getInstance().appendSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE);
        SysUtils.resetForTesting();

        List<Integer> reasons = new ArrayList<>();
        boolean eligible = mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                /* returnImmediately= */ false, /* outList= */ reasons);
        Assert.assertEquals(1, reasons.size());
        Assert.assertEquals(EligibilityFailureReason.LOW_END_DEVICE, (int) reasons.get(0));
        Assert.assertFalse(eligible);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_MutlipleAccounts() throws Exception {
        doReturn(FAKE_ACCOUNTS_2).when(mAccountManagerFacade).getGoogleAccounts();

        List<Integer> reasons = new ArrayList<>();
        boolean eligible = mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                /* returnImmediately= */ false, /* outList= */ reasons);
        Assert.assertEquals(1, reasons.size());
        Assert.assertEquals(
                EligibilityFailureReason.MULTIPLE_ACCOUNTS_ON_DEVICE, (int) reasons.get(0));
        Assert.assertFalse(eligible);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_MutlipleAccounts_CheckDisabled() throws Exception {
        mAssistantVoiceSearchService.setMultiAccountCheckEnabledForTesting(false);
        doReturn(FAKE_ACCOUNTS_2).when(mAccountManagerFacade).getGoogleAccounts();

        List<Integer> reasons = new ArrayList<>();
        boolean eligible = mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                /* returnImmediately= */ false, /* outList= */ reasons);
        Assert.assertEquals(0, reasons.size());
        Assert.assertTrue(eligible);
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_MutlipleAccounts_JustAdded() throws Exception {
        mAssistantVoiceSearchService.setColorfulMicEnabledForTesting(true);
        mAssistantVoiceSearchService.onAccountsChanged();

        // Colorful mic should be returned when only 1 account is present.
        Assert.assertNull(mAssistantVoiceSearchService.getMicButtonColorStateList(0, mContext));

        doReturn(FAKE_ACCOUNTS_2).when(mAccountManagerFacade).getGoogleAccounts();
        mAssistantVoiceSearchService.onAccountsChanged();

        // Colorful mic should be returned when only 1 account is present.
        Assert.assertNotNull(mAssistantVoiceSearchService.getMicButtonColorStateList(0, mContext));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testAssistantEligibility_AGSA_not_installed() throws Exception {
        doReturn(false).when(mGsaState).isGsaInstalled();

        List<Integer> reasons = new ArrayList<>();
        boolean eligible = mAssistantVoiceSearchService.isDeviceEligibleForAssistant(
                /* returnImmediately= */ false, /* outList= */ reasons);
        Assert.assertEquals(1, reasons.size());
        Assert.assertEquals(EligibilityFailureReason.AGSA_NOT_INSTALLED, (int) reasons.get(0));
        Assert.assertFalse(eligible);
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

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testReportUserEligibility() {
        mAssistantVoiceSearchService.reportMicPressUserEligibility();
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.USER_ELIGIBILITY_HISTOGRAM, /* eligible= */ 1));
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.AGSA_VERSION_HISTOGRAM, AGSA_VERSION_NUMBER));

        doReturn(true).when(mGsaState).isAgsaVersionBelowMinimum(any(), any());
        doReturn(false).when(mIdentityManager).hasPrimaryAccount();
        mAssistantVoiceSearchService.reportMicPressUserEligibility();
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.USER_ELIGIBILITY_HISTOGRAM, /* eligible= */ 0));
        Assert.assertEquals(2,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.AGSA_VERSION_HISTOGRAM, AGSA_VERSION_NUMBER));

        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                        AssistantVoiceSearchService.EligibilityFailureReason.NO_CHROME_ACCOUNT));
        Assert.assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM,
                        AssistantVoiceSearchService.EligibilityFailureReason.NO_CHROME_ACCOUNT));
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testDoesViolateMultiAccountCheck_throws() throws Exception {
        doThrow(GmsAvailabilityException.class).when(mAccountManagerFacade).getGoogleAccounts();
        Assert.assertTrue(mAssistantVoiceSearchService.doesViolateMultiAccountCheck());
    }

    @Test
    @Feature("OmniboxAssistantVoiceSearch")
    public void testDoesViolateMultiAccountCheck_cacheNotPopuolated() throws Exception {
        doReturn(false).when(mAccountManagerFacade).isCachePopulated();
        Assert.assertTrue(mAssistantVoiceSearchService.doesViolateMultiAccountCheck());
    }
}
