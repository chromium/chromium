// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.mockito.ArgumentMatchers.any;

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
import org.chromium.chrome.browser.omnibox.voice.AssistantVoiceSearchConsentController.ConsentOutcome;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Tests for AssistantVoiceSearchConsentDialog */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.OMNIBOX_ASSISTANT_VOICE_SEARCH)
// TODO(wylieb): Batch these tests.
public class AssistantVoiceSearchConsentControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    @Mock
    Callback<Boolean> mCallback;

    AssistantVoiceSearchConsentController mController;
    AssistantVoiceSearchConsentUi mConsentUi;
    AssistantVoiceSearchConsentUi.Observer mObserver;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeTabbedActivity cta = mActivityTestRule.getActivity();
            mController = createController();
        });
    }

    @After
    public void tearDown() {
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED);
    }

    private void showConsentUi() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mController.show(); });
    }

    private AssistantVoiceSearchConsentController createController() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            ChromeTabbedActivity cta = mActivityTestRule.getActivity();
            mConsentUi = new AssistantVoiceSearchConsentUi() {
                @Override
                public void show(AssistantVoiceSearchConsentUi.Observer observer) {
                    mObserver = observer;
                }
                @Override
                public void dismiss() {
                    mObserver = null;
                }
            };
            return new AssistantVoiceSearchConsentController(cta.getWindowAndroid(),
                    mSharedPreferencesManager, mCallback,
                    () -> AutofillAssistantPreferenceFragment.launchSettings(cta), mConsentUi);
        });
    }

    @Test
    @MediumTest
    public void testNoBottomSheetControllerAvailable() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantVoiceSearchConsentController.show(
                    cta.getWindowAndroid(), mSharedPreferencesManager, () -> {}, null, mCallback);
        });
        Mockito.verify(mCallback, Mockito.timeout(1000)).onResult(false);
    }

    // Helper method that accepts consent via button taps and verifies expected state.
    private void verifyAcceptingConsent() {
        showConsentUi();

        TestThreadUtils.runOnUiThreadBlocking(() -> { mObserver.onConsentAccepted(); });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mSharedPreferencesManager.readBoolean(
                                       ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED,
                                       /* default= */ false),
                    is(true));
        });

        Mockito.verify(mCallback, Mockito.timeout(1000)).onResult(true);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchConsentController.CONSENT_OUTCOME_HISTOGRAM,
                        ConsentOutcome.ACCEPTED_VIA_UI));
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

        TestThreadUtils.runOnUiThreadBlocking(() -> { mObserver.onConsentRejected(); });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mSharedPreferencesManager.readBoolean(
                                       ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED,
                                       /* default= */ true),
                    is(false));
        });

        Mockito.verify(mCallback, Mockito.timeout(1000)).onResult(false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchConsentController.CONSENT_OUTCOME_HISTOGRAM,
                        ConsentOutcome.REJECTED_VIA_UI));
    }

    @Test
    @MediumTest
    public void testDialogInteractivity_NonUserCancel() {
        showConsentUi();

        TestThreadUtils.runOnUiThreadBlocking(() -> { mObserver.onNonUserCancel(); });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mSharedPreferencesManager.contains(
                                       ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED),
                    is(false));
        });

        Mockito.verify(mCallback, Mockito.timeout(1000)).onResult(false);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchConsentController.CONSENT_OUTCOME_HISTOGRAM,
                        ConsentOutcome.NON_USER_CANCEL));
    }

    @Test
    @MediumTest
    public void testDialogInteractivity_LearnMoreButton() {
        showConsentUi();

        SettingsActivity activity = ApplicationTestUtils.waitForActivityWithClass(
                SettingsActivity.class, Stage.RESUMED, () -> { mObserver.onLearnMoreClicked(); });

        onView(withText(mActivityTestRule.getActivity().getResources().getString(
                       R.string.avs_setting_category_title)))
                .check(matches(isDisplayed()));
        activity.finish();

        Mockito.verify(mCallback, Mockito.times(0)).onResult(any());
    }

    @Test
    @MediumTest
    public void testDialogInteractivity_AcceptViaSettings() {
        showConsentUi();

        SettingsActivity activity = ApplicationTestUtils.waitForActivityWithClass(
                SettingsActivity.class, Stage.RESUMED, () -> { mObserver.onLearnMoreClicked(); });

        onView(withText(mActivityTestRule.getActivity().getResources().getString(
                       R.string.avs_setting_category_title)))
                .check(matches(isDisplayed()));
        Mockito.verify(mCallback, Mockito.times(0)).onResult(/* meaningless value */ true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSharedPreferencesManager.writeBoolean(
                    ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED, true);
        });
        activity.finish();
        Mockito.verify(mCallback, Mockito.timeout(1000)).onResult(true);
        // The observer is reset when dismiss() is called.
        Assert.assertNull(mObserver);
    }

    // Helper method for test cases covering dimissing the dialog.
    private void verifyBackingOffConsent(Runnable backOffMethod, boolean expectConsentValueSet,
            int expectedHistogramCount, int expectedConsentOutcome) {
        showConsentUi();

        TestThreadUtils.runOnUiThreadBlocking(backOffMethod);

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mSharedPreferencesManager.contains(
                                       ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED),
                    is(expectConsentValueSet));
            if (expectConsentValueSet) {
                Criteria.checkThat(mSharedPreferencesManager.readBoolean(
                                           ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED,
                                           /* default= */ true),
                        is(false));
            }
        });

        Mockito.verify(mCallback).onResult(false);
        Assert.assertEquals(expectedHistogramCount,
                RecordHistogram.getHistogramValueCountForTesting(
                        AssistantVoiceSearchConsentController.CONSENT_OUTCOME_HISTOGRAM,
                        expectedConsentOutcome));

        Mockito.reset(mCallback);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.ASSISTANT_CONSENT_V2)
    public void testDialogInteractivity_RejectViaCancel() {
        verifyBackingOffConsent(()
                                        -> { mObserver.onConsentCanceled(); },
                /*expectConsentValueSet=*/true,
                /*expectedHistogramCount*/ 1, ConsentOutcome.REJECTED_VIA_DISMISS);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ASSISTANT_CONSENT_V2)
    public void testDialogInteractivity_CancelIgnored() {
        verifyBackingOffConsent(()
                                        -> { mObserver.onConsentCanceled(); },
                /*expectConsentValueSet=*/false,
                /*expectedHistogramCount=*/1, ConsentOutcome.CANCELED_VIA_UI);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ASSISTANT_CONSENT_V2)
    public void testDialogInteractivity_AcceptingConsentAfterDismissal() {
        verifyBackingOffConsent(()
                                        -> { mObserver.onConsentCanceled(); },
                /*expectConsentValueSet=*/false,
                /*expectedHistogramCount=*/1, ConsentOutcome.CANCELED_VIA_UI);

        // Successful showing of the consent calls destroy(). Need to recreate the new
        // instance to set up the state again.
        mController = createController();
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
            verifyBackingOffConsent(()
                                            -> { mObserver.onConsentCanceled(); },
                    /*expectConsentValueSet=*/false,
                    /*expectedHistogramCount=*/i + 1, ConsentOutcome.CANCELED_VIA_UI);
            // Successful showing of the consent calls destroy(). Need to recreate the new
            // instance to set up the state again.
            mController = createController();
        }

        // But the max_taps_ignored+1-th will be treated as a rejection.
        verifyBackingOffConsent(()
                                        -> { mObserver.onConsentCanceled(); },
                /*expectConsentValueSet=*/true,
                /*expectedHistogramCount=*/1, ConsentOutcome.REJECTED_VIA_DISMISS);
    }
}
