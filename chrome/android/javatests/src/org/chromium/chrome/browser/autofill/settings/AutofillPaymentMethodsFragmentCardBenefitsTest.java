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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.policy.test.annotations.Policies;

import java.util.concurrent.TimeoutException;

/** Instrumentation Tests for AutofillPaymentMethodsFragment: Card Benefits */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AutofillPaymentMethodsFragmentCardBenefitsTest {
    @Rule public final AutofillTestRule mRule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<AutofillPaymentMethodsFragment>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(AutofillPaymentMethodsFragment.class);

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
    }

    @After
    public void tearDown() throws TimeoutException {
        mSettingsActivityTestRule.getActivity().finish();
        mAutofillTestHelper.clearAllDataForTesting();
    }

    // Test to verify that the card benefit preference is not displayed when autofill credit
    // card is disabled.
    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "false")})
    public void testCardBenefitsPref_whenAutofillIsDisabled_notShown() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardBenefitsPref =
                getPreferenceScreen(activity)
                        .findPreference(AutofillPaymentMethodsFragment.PREF_CARD_BENEFITS);
        assertThat(cardBenefitsPref).isNull();
    }

    // Test to verify that the card benefit preference is displayed when autofill credit
    // card is enabled.
    @Test
    @MediumTest
    @Policies.Add({@Policies.Item(key = "AutofillCreditCardEnabled", string = "true")})
    public void testCardBenefitsPref_whenAutofillIsEnabled_shown() throws Exception {
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
    // fragment.
    @Test
    @MediumTest
    public void testCardBenefitsPref_whenClicked_opensAutofillCardBenefitsFragment()
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

    private static PreferenceScreen getPreferenceScreen(SettingsActivity activity) {
        return ((AutofillPaymentMethodsFragment) activity.getMainFragment()).getPreferenceScreen();
    }
}
