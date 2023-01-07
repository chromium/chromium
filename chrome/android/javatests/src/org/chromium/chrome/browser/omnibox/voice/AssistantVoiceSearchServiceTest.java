// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchService.EligibilityFailureReason;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.externalauth.ExternalAuthUtils;

/** Tests for AssistantVoiceSearchService */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=" + ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH + "<Study",
        "force-fieldtrials=Study/Group"})
@Restriction({RESTRICTION_TYPE_NON_LOW_END_DEVICE})
public class AssistantVoiceSearchServiceTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock
    private GSAState mGsaState;
    @Mock
    private ExternalAuthUtils mExternalAuthUtils;

    @Before
    public void setUp() throws Exception {
        SharedPreferencesManager.getInstance().writeBoolean(ASSISTANT_VOICE_SEARCH_ENABLED, true);

        doReturn(false).when(mGsaState).isAgsaVersionBelowMinimum(anyString(), anyString());
        doReturn(true).when(mGsaState).canAgsaHandleIntent(anyObject());
        doReturn(true).when(mGsaState).isGsaInstalled();
        GSAState.setInstanceForTesting(mGsaState);

        doReturn(false).when(mExternalAuthUtils).isGoogleSigned(anyString());
        doReturn(false).when(mExternalAuthUtils).isChromeGoogleSigned();
        doReturn(true).when(mExternalAuthUtils).canUseGooglePlayServices();
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);

        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    public void testStartupHistograms() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        CriteriaHelper.pollUiThread(() -> {
            // Not eligible for Assistant voice search due to apps being unsigned and no signed-in
            // user.
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            AssistantVoiceSearchService.USER_ELIGIBILITY_HISTOGRAM
                                    + AssistantVoiceSearchService.STARTUP_HISTOGRAM_SUFFIX,
                            /* false */ 0));
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            AssistantVoiceSearchService.USER_ELIGIBILITY_HISTOGRAM
                            + AssistantVoiceSearchService.STARTUP_HISTOGRAM_SUFFIX));

            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM
                                    + AssistantVoiceSearchService.STARTUP_HISTOGRAM_SUFFIX,
                            EligibilityFailureReason.CHROME_NOT_GOOGLE_SIGNED));
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM
                                    + AssistantVoiceSearchService.STARTUP_HISTOGRAM_SUFFIX,
                            EligibilityFailureReason.AGSA_NOT_GOOGLE_SIGNED));
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM
                                    + AssistantVoiceSearchService.STARTUP_HISTOGRAM_SUFFIX,
                            EligibilityFailureReason.NO_CHROME_ACCOUNT));
            Assert.assertEquals(3,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM
                            + AssistantVoiceSearchService.STARTUP_HISTOGRAM_SUFFIX));
        });
    }

    @Test
    @MediumTest
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.ASSISTANT_NON_PERSONALIZED_VOICE_SEARCH})
    public void testStartupHistograms_NonPersonalizedRecognition() {
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);

        CriteriaHelper.pollUiThread(() -> {
            // Not eligible for Assistant voice search due to apps being unsigned.
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            AssistantVoiceSearchService.USER_ELIGIBILITY_HISTOGRAM
                                    + AssistantVoiceSearchService.STARTUP_HISTOGRAM_SUFFIX,
                            /* false */ 0));
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            AssistantVoiceSearchService.USER_ELIGIBILITY_HISTOGRAM
                            + AssistantVoiceSearchService.STARTUP_HISTOGRAM_SUFFIX));

            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM
                                    + AssistantVoiceSearchService.STARTUP_HISTOGRAM_SUFFIX,
                            EligibilityFailureReason.CHROME_NOT_GOOGLE_SIGNED));
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM
                                    + AssistantVoiceSearchService.STARTUP_HISTOGRAM_SUFFIX,
                            EligibilityFailureReason.AGSA_NOT_GOOGLE_SIGNED));
            Assert.assertEquals(0,
                    RecordHistogram.getHistogramValueCountForTesting(
                            AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM
                                    + AssistantVoiceSearchService.STARTUP_HISTOGRAM_SUFFIX,
                            EligibilityFailureReason.NO_CHROME_ACCOUNT));
            Assert.assertEquals(2,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            AssistantVoiceSearchService.USER_ELIGIBILITY_FAILURE_REASON_HISTOGRAM
                            + AssistantVoiceSearchService.STARTUP_HISTOGRAM_SUFFIX));
        });
    }
}
