// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.components.policy.test.annotations.Policies;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Instrumentation Tests for AutofillPaymentMethodsFragment: Card Benefits */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class AutofillPaymentMethodsFragmentCardBenefitsTest {
    @Rule public final AutofillTestRule mRule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<AutofillPaymentMethodsFragment>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(AutofillPaymentMethodsFragment.class);

    private AutofillTestHelper mAutofillTestHelper;

    /** Provides parameters for testing card benefit preference with all flag combinations. */
    public static class CardBenefitsPreferenceTestParams implements ParameterProvider {
        private static final List<ParameterSet> sCardBenefitsPreferenceTestParams =
                Arrays.asList(
                        new ParameterSet()
                                .value(true, true, true)
                                .name("AmexFlagIsEnabled_BmoFlagIsEnabled_CurinosFlagIsEnabled"),
                        new ParameterSet()
                                .value(true, true, false)
                                .name("AmexFlagIsEnabled_BmoFlagIsEnabled_CurinosFlagIsDisabled"),
                        new ParameterSet()
                                .value(true, false, true)
                                .name("AmexFlagIsEnabled_BmoFlagIsDisabled_CurinosFlagIsEnabled"),
                        new ParameterSet()
                                .value(true, false, false)
                                .name("AmexFlagIsEnabled_BmoFlagIsDisabled_CurinosFlagIsDisabled"),
                        new ParameterSet()
                                .value(false, true, true)
                                .name("AmexFlagIsDisabled_BmoFlagIsEnabled_CurinosFlagIsEnabled"),
                        new ParameterSet()
                                .value(false, true, false)
                                .name("AmexFlagIsDisabled_BmoFlagIsEnabled_CurinosFlagIsDisabled"),
                        new ParameterSet()
                                .value(false, false, true)
                                .name("AmexFlagIsDisabled_BmoFlagIsDisabled_CurinosFlagIsEnabled"));

        @Override
        public List<ParameterSet> getParameters() {
            return sCardBenefitsPreferenceTestParams;
        }
    }

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
    }

    @After
    public void tearDown() throws TimeoutException {
        mSettingsActivityTestRule.getActivity().finish();
        mAutofillTestHelper.clearAllDataForTesting();
    }

    @Test
    @MediumTest
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
        ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_BMO,
        ChromeFeatureList.AUTOFILL_ENABLE_FLAT_RATE_CARD_BENEFITS_FROM_CURINOS
    })
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "true")})
    public void testCardBenefitsPref_whenFlagsAreOffAndAutofillIsEnabled_notShown()
            throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardBenefitsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_CARD_BENEFITS);
        assertThat(cardBenefitsPref).isNull();
    }

    // Test to verify that the card benefit preference is not displayed when autofill credit
    // card is disabled, across all combinations of the flags
    // AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
    // AUTOFILL_ENABLE_CARD_BENEFITS_FOR_BMO, and
    // AUTOFILL_ENABLE_FLAT_RATE_CARD_BENEFITS_FROM_CURINOS.
    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(CardBenefitsPreferenceTestParams.class)
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "false")})
    public void testCardBenefitsPref_whenAutofillIsDisabled_notShown(
            boolean isAmexFlagEnabled, boolean isBmoFlagEnabled, boolean isCurinosFlagEnabled)
            throws Exception {
        setCardBenefitsFlags(isAmexFlagEnabled, isBmoFlagEnabled, isCurinosFlagEnabled);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardBenefitsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_CARD_BENEFITS);
        assertThat(cardBenefitsPref).isNull();
    }

    // Test to verify that the card benefit preference is displayed when autofill credit
    // card is enabled, across all combinations of the flags
    // AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
    // AUTOFILL_ENABLE_CARD_BENEFITS_FOR_BMO, and
    // AUTOFILL_ENABLE_FLAT_RATE_CARD_BENEFITS_FROM_CURINOS.
    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(CardBenefitsPreferenceTestParams.class)
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "true")})
    public void testCardBenefitsPref_whenAutofillIsEnabled_shown(
            boolean isAmexFlagEnabled, boolean isBmoFlagEnabled, boolean isCurinosFlagEnabled)
            throws Exception {
        setCardBenefitsFlags(isAmexFlagEnabled, isBmoFlagEnabled, isCurinosFlagEnabled);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardBenefitsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_CARD_BENEFITS);

        assertEquals(
                cardBenefitsPref.getTitle(),
                activity.getString(R.string.autofill_settings_page_card_benefits_label));
        assertEquals(
                cardBenefitsPref.getSummary(),
                activity.getString(
                        R.string.autofill_settings_page_card_benefits_preference_summary));
    }

    // Test to verify that clicking the card benefit preference opens the credit card benefits
    // fragment, when all benefit flags are enabled.
    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
        ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_BMO,
        ChromeFeatureList.AUTOFILL_ENABLE_FLAT_RATE_CARD_BENEFITS_FROM_CURINOS
    })
    // TODO(crbug.com/435263284): Use parameterized tests with all flag combinations. Currently,
    // using parameterized tests results in flaky test failures only on android-x64-rel targets.
    public void testCardBenefitsPref_whenClicked_opensAutofillCardBenefitsFragment_allFlagsEnabled()
            throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardBenefitsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_CARD_BENEFITS);

        ThreadUtils.runOnUiThreadBlocking(cardBenefitsPref::performClick);
        mRule.waitForFragmentToBeShown();

        // Verify that the card benefits fragment is opened.
        assertTrue(mRule.getLastestShownFragment() instanceof AutofillCardBenefitsFragment);
    }

    // Test to verify that clicking the card benefit preference opens the credit card benefits
    // fragment, when all benefit flags are disabled.
    @Test
    @MediumTest
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
        ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_BMO,
        ChromeFeatureList.AUTOFILL_ENABLE_FLAT_RATE_CARD_BENEFITS_FROM_CURINOS
    })
    // TODO(crbug.com/435263284): Use parameterized tests with all flag combinations. Currently,
    // using parameterized tests results in flaky test failures only on android-x64-rel targets.
    public void
            testCardBenefitsPref_whenClicked_opensAutofillCardBenefitsFragment_allFlagsDisabled()
                    throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardBenefitsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_CARD_BENEFITS);

        // If all flags are disabled then the card benefits preference will not be displayed and we
        // cannot get to the the card benefits fragment.
        assertThat(cardBenefitsPref).isNull();
    }

    private static void setCardBenefitsFlags(
            boolean isAmexFlagEnabled, boolean isBmoFlagEnabled, boolean isCurinosFlagEnabled) {
        FeatureOverrides.newBuilder()
                .flag(
                        ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
                        isAmexFlagEnabled)
                .flag(ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_BMO, isBmoFlagEnabled)
                .flag(
                        ChromeFeatureList.AUTOFILL_ENABLE_FLAT_RATE_CARD_BENEFITS_FROM_CURINOS,
                        isCurinosFlagEnabled)
                .apply();
    }

    private static PreferenceScreen getPreferenceScreen(SettingsActivity activity) {
        return ((AutofillPaymentMethodsFragment) activity.getMainFragment()).getPreferenceScreen();
    }
}
