// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchConsentUi.CONSENT_OUTCOME_HISTOGRAM;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED;

import android.support.test.runner.lifecycle.Stage;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantPreferenceFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchConsentUi.ConsentOutcome;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/** Tests for AssistantVoiceSearchConsentDialog */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
// TODO(wylieb): Batch these tests.
public class AssistantVoiceSearchConsentUiTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public DisableAnimationsTestRule mDisableAnimationsTestRule = new DisableAnimationsTestRule();

    final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    @Mock
    Callback<Boolean> mCallback;

    AssistantVoiceSearchConsentUi mAssistantVoiceSearchConsentUi;
    BottomSheetController mBottomSheetController;
    BottomSheetTestSupport mBottomSheetTestSupport;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeTabbedActivity cta = mActivityTestRule.getActivity();
            mBottomSheetController =
                    cta.getRootUiCoordinatorForTesting().getBottomSheetController();
            mBottomSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
            mAssistantVoiceSearchConsentUi = createConsentUi();
        });
    }

    @After
    public void tearDown() {
        mSharedPreferencesManager.removeKey(ASSISTANT_VOICE_SEARCH_ENABLED);
    }

    private void showConsentUi() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAssistantVoiceSearchConsentUi.show(mCallback);
            mBottomSheetTestSupport.endAllAnimations();
        });
    }

    private AssistantVoiceSearchConsentUi createConsentUi() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ChromeTabbedActivity cta = mActivityTestRule.getActivity();
            return new AssistantVoiceSearchConsentUi(cta.getWindowAndroid(), cta,
                    mSharedPreferencesManager,
                    ()
                            -> AutofillAssistantPreferenceFragment.launchSettings(cta),
                    mBottomSheetController);
        });
    }

    @Test
    @MediumTest
    public void testNoBottomSheetControllerAvailable() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantVoiceSearchConsentUi.show(
                    cta.getWindowAndroid(), mSharedPreferencesManager, () -> {}, null, mCallback);
        });
        Mockito.verify(mCallback, Mockito.timeout(1000)).onResult(false);
    }

    // Helper method that accepts consent via button taps and verifies expected state.
    private void verifyAcceptingConsent() {
        showConsentUi();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClickUtils.clickButton(mAssistantVoiceSearchConsentUi.getContentView().findViewById(
                    R.id.button_primary));
            mBottomSheetTestSupport.endAllAnimations();
        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mSharedPreferencesManager.readBoolean(
                                       ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false),
                    is(true));
        });

        Mockito.verify(mCallback, Mockito.timeout(1000)).onResult(true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        CONSENT_OUTCOME_HISTOGRAM, ConsentOutcome.ACCEPTED_VIA_BUTTON));
    }

    @Test
    @MediumTest
    public void testDialogInteractivity_AcceptButton() {
        verifyAcceptingConsent();
    }

    @Test
    @MediumTest
    public void testDialogInteractivity_RejectButton() {
        showConsentUi();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClickUtils.clickButton(mAssistantVoiceSearchConsentUi.getContentView().findViewById(
                    R.id.button_secondary));
            mBottomSheetTestSupport.endAllAnimations();
        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mSharedPreferencesManager.readBoolean(
                                       ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ true),
                    is(false));
        });

        Mockito.verify(mCallback, Mockito.timeout(1000)).onResult(false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        CONSENT_OUTCOME_HISTOGRAM, ConsentOutcome.REJECTED_VIA_BUTTON));
    }

    @Test
    @MediumTest
    public void testDialogInteractivity_LearnMoreButton() {
        showConsentUi();

        SettingsActivity activity = ApplicationTestUtils.waitForActivityWithClass(
                SettingsActivity.class, Stage.RESUMED, () -> {
                    ClickUtils.clickButton(
                            mAssistantVoiceSearchConsentUi.getContentView().findViewById(
                                    R.id.avs_consent_ui_learn_more));
                    mBottomSheetTestSupport.endAllAnimations();
                });

        onView(withText(mActivityTestRule.getActivity().getResources().getString(
                       R.string.avs_setting_category_title)))
                .check(matches(isDisplayed()));
        Mockito.verify(mCallback, Mockito.times(0)).onResult(/* meaningless value */ true);
        activity.finish();
    }

    // Helper method for test cases covering dimissing the dialog.
    private void verifyBackingOffConsent(Runnable backOffMethod, boolean expectConsentValueSet,
            int expectedHistogramCount, int expectedConsentOutcome) {
        showConsentUi();

        TestThreadUtils.runOnUiThreadBlocking(backOffMethod);

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mSharedPreferencesManager.contains(ASSISTANT_VOICE_SEARCH_ENABLED),
                    is(expectConsentValueSet));
            if (expectConsentValueSet) {
                Criteria.checkThat(mSharedPreferencesManager.readBoolean(
                                           ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ true),
                        is(false));
            }
        });

        Mockito.verify(mCallback).onResult(false);
        Assert.assertEquals(expectedHistogramCount,
                RecordHistogram.getHistogramValueCountForTesting(
                        CONSENT_OUTCOME_HISTOGRAM, expectedConsentOutcome));

        Mockito.reset(mCallback);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.ASSISTANT_CONSENT_V2)
    public void testDialogInteractivity_BackButton() {
        verifyBackingOffConsent(mBottomSheetTestSupport::handleBackPress,
                /*expectConsentValueSet=*/true,
                /*expectedHistogramCount*/ 1, ConsentOutcome.REJECTED_VIA_BACK_BUTTON_PRESS);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.ASSISTANT_CONSENT_V2)
    public void testDialogInteractivity_ScrimTap() {
        verifyBackingOffConsent(mBottomSheetTestSupport::forceClickOutsideTheSheet,
                /*expectConsentValueSet=*/true,
                /*expectedHistogramCount*/ 1, ConsentOutcome.REJECTED_VIA_SCRIM_TAP);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ASSISTANT_CONSENT_V2)
    public void testDialogInteractivity_ScrimTapIgnored() {
        verifyBackingOffConsent(mBottomSheetTestSupport::forceClickOutsideTheSheet,
                /*expectConsentValueSet=*/false,
                /*expectedHistogramCount=*/1, ConsentOutcome.CANCELED_VIA_SCRIM_TAP);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ASSISTANT_CONSENT_V2)
    public void testDialogInteractivity_BackButtonIgnored() {
        verifyBackingOffConsent(mBottomSheetTestSupport::handleBackPress,
                /*expectConsentValueSet=*/false,
                /*expectedHistogramCount=*/1, ConsentOutcome.CANCELED_VIA_BACK_BUTTON_PRESS);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ASSISTANT_CONSENT_V2)
    public void testDialogInteractivity_AcceptingConsentAfterDismissal() {
        verifyBackingOffConsent(mBottomSheetTestSupport::handleBackPress,
                /*expectConsentValueSet=*/false,
                /*expectedHistogramCount=*/1, ConsentOutcome.CANCELED_VIA_BACK_BUTTON_PRESS);

        // Successful showing of the consent calls destroy(). Need to recreate the new
        // instance to set up the state again.
        mAssistantVoiceSearchConsentUi = createConsentUi();
        verifyAcceptingConsent();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.ASSISTANT_CONSENT_V2 + "<Study",
            "force-fieldtrials=Study/Group", "force-fieldtrial-params=Study.Group:count/3"})
    @Feature({"AssistantConsentV2"})
    public void
    testDialogInteractivity_TapsCounter() {
        int max_taps_ignored = 3;
        for (int i = 0; i < max_taps_ignored; i++) {
            verifyBackingOffConsent(mBottomSheetTestSupport::handleBackPress,
                    /*expectConsentValueSet=*/false,
                    /*expectedHistogramCount=*/i + 1,
                    ConsentOutcome.CANCELED_VIA_BACK_BUTTON_PRESS);
            // Successful showing of the consent calls destroy(). Need to recreate the new
            // instance to set up the state again.
            mAssistantVoiceSearchConsentUi = createConsentUi();
        }

        // But the max_taps_ignored+1-th will be treated as a rejection.
        verifyBackingOffConsent(mBottomSheetTestSupport::forceClickOutsideTheSheet,
                /*expectConsentValueSet=*/true,
                /*expectedHistogramCount=*/1, ConsentOutcome.REJECTED_VIA_SCRIM_TAP);
    }
}
