// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
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

    /** Provides parameters for testing card benefit preference with different flag combinations. */
    public static class CardBenefitsPreferenceTestParams implements ParameterProvider {
        private static List<ParameterSet> sCardBenefitsPreferenceTestParams =
                Arrays.asList(
                        new ParameterSet()
                                .value(true, true)
                                .name("AmexFlagIsEnabledAndCapitalOneFlagIsEnabled"),
                        new ParameterSet()
                                .value(true, false)
                                .name("AmexFlagIsEnabledAndCapitalOneFlagIsDisabled"),
                        new ParameterSet()
                                .value(false, true)
                                .name("AmexFlagIsDisabledAndCapitalOneFlagIsEnabled"));

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
        mAutofillTestHelper.clearAllDataForTesting();
    }

    @Test
    @MediumTest
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
        ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_CAPITAL_ONE
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
    // card is disabled, across various combinations of the flags
    // AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS and
    // AUTOFILL_ENABLE_CARD_BENEFITS_FOR_CAPITAL_ONE.
    // i.e. (True, True), (True, False), (False, True)
    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(CardBenefitsPreferenceTestParams.class)
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "false")})
    public void testCardBenefitsPref_whenAutofillIsDisabled_notShown(
            boolean isAmexFlagEnabled, boolean isCaptialOneFlagEnabled) throws Exception {
        setCardBenefitsFlags(isAmexFlagEnabled, isCaptialOneFlagEnabled);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardBenefitsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_CARD_BENEFITS);
        assertThat(cardBenefitsPref).isNull();
    }

    // Test to verify that the card benefit preference is displayed when autofill credit
    // card is enabled, across various combinations of the flags
    // AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS and
    // AUTOFILL_ENABLE_CARD_BENEFITS_FOR_CAPITAL_ONE.
    // i.e. (True, True), (True, False), (False, True)
    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(CardBenefitsPreferenceTestParams.class)
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "true")})
    public void testCardBenefitsPref_whenAutofillIsEnabled_shown(
            boolean isAmexFlagEnabled, boolean isCaptialOneFlagEnabled) throws Exception {
        setCardBenefitsFlags(isAmexFlagEnabled, isCaptialOneFlagEnabled);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardBenefitsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_CARD_BENEFITS);
        Assert.assertEquals(
                cardBenefitsPref.getTitle(),
                activity.getString(R.string.autofill_settings_page_card_benefits_label));
        Assert.assertEquals(
                cardBenefitsPref.getSummary(),
                activity.getString(
                        R.string.autofill_settings_page_card_benefits_preference_summary));
    }

    // Test to verify that clicking the card benefit preference opens the credit card benefits
    // fragment, across various combinations of the flags
    // AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS and
    // AUTOFILL_ENABLE_CARD_BENEFITS_FOR_CAPITAL_ONE.
    // i.e. (True, True), (True, False), (False, True)
    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(CardBenefitsPreferenceTestParams.class)
    public void testCardBenefitsPref_whenClicked_opensAutofillCardBenefitsFragment(
            boolean isAmexFlagEnabled, boolean isCaptialOneFlagEnabled) throws Exception {
        setCardBenefitsFlags(isAmexFlagEnabled, isCaptialOneFlagEnabled);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        Preference cardBenefitsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_CARD_BENEFITS);

        ThreadUtils.runOnUiThreadBlocking(cardBenefitsPref::performClick);
        mRule.waitForFragmentToBeShown();

        Assert.assertTrue(mRule.getLastestShownFragment() instanceof AutofillCardBenefitsFragment);
    }

    private static void setCardBenefitsFlags(
            boolean isAmexFlagEnabled, boolean isCapOneFlagEnabled) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
                isAmexFlagEnabled);
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_CAPITAL_ONE,
                isCapOneFlagEnabled);
        FeatureList.setTestValues(testValues);
    }

    private static PreferenceScreen getPreferenceScreen(SettingsActivity activity) {
        return ((AutofillPaymentMethodsFragment) activity.getMainFragment()).getPreferenceScreen();
    }
}
