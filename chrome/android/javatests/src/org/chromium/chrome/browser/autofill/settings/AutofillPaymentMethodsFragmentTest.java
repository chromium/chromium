// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Instrumentation tests for AutofillPaymentMethodsFragment.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillPaymentMethodsFragmentTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public final AutofillTestRule rule = new AutofillTestRule();
    @Rule
    public final SettingsActivityTestRule<AutofillPaymentMethodsFragment>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(AutofillPaymentMethodsFragment.class);

    // Card Issuer values that map to the browser CreditCard.Issuer enum.
    private static final int CARD_ISSUER_UNKNOWN = 0;
    private static final int CARD_ISSUER_GOOGLE = 1;

    private static final CreditCard SAMPLE_CARD_VISA = new CreditCard(/* guid= */ "",
            /* origin= */ "",
            /* isLocal= */ false, /* isCached= */ false, /* name= */ "John Doe",
            /* number= */ "4444333322221111",
            /* obfuscatedNumber= */ "", /* month= */ "5", AutofillTestHelper.nextYear(),
            /* basicCardIssuerNetwork =*/"visa",
            /* issuerIconDrawableId= */ 0, /* billingAddressId= */ "",
            /* serverId= */ "");
    private static final CreditCard SAMPLE_CARD_MASTERCARD =
            new CreditCard(/* guid= */ "", /* origin= */ "",
                    /* isLocal= */ false, /* isCached= */ false, /* name= */ "John Doe",
                    /* number= */ "5454545454545454",
                    /* obfuscatedNumber= */ "", /* month= */ "12", AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "mastercard", /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "");

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
    }

    @Test
    @MediumTest
    public void testTwoCreditCards_displaysTwoServerCards() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_MASTERCARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
        // Verify that the preferences on the initial screen map to Save and Fill toggle + 2 Cards +
        // Add Card button + Payment Apps.
        Assert.assertEquals(5, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    public void testCreditCardWithoutNickname_displayNetworkAndLastFourAsTitle() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_CARD_VISA);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Visa");
        assertThat(title).contains("1111");
    }

    @Test
    @MediumTest
    public void testCreditCardWithNickname_displaysNicknameAndLastFourAsTitle() throws Exception {
        mAutofillTestHelper.addServerCreditCard(
                SAMPLE_CARD_VISA, "Test nickname", CARD_ISSUER_UNKNOWN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Test nickname");
        assertThat(title).contains("1111");
    }

    @Test
    @MediumTest
    public void testCreditCardWithLongNickname_displaysCompleteNicknameAndLastFourAsTitle()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(
                SAMPLE_CARD_VISA, "This is a long nickname", CARD_ISSUER_UNKNOWN);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("This is a long nickname");
        assertThat(title).contains("1111");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_GOOGLE_ISSUED_CARD})
    public void testGoogleIssuedServerCard_displaysGoogleSpecificTitle() throws Exception {
        mAutofillTestHelper.addServerCreditCard(
                SAMPLE_CARD_VISA, /* nickname= */ "", CARD_ISSUER_GOOGLE);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Plex Visa");
        assertThat(title).contains("1111");
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_GOOGLE_ISSUED_CARD})
    public void testGoogleIssuedServerCard_expOff_cardNotDisplayed() throws Exception {
        mAutofillTestHelper.addServerCreditCard(
                SAMPLE_CARD_VISA, /* nickname= */ "", CARD_ISSUER_GOOGLE);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the preferences on the initial screen map to Save and Fill toggle + Add Card
        // button + Payment Apps.
        Assert.assertEquals(3, getPreferenceScreen(activity).getPreferenceCount());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_GOOGLE_ISSUED_CARD})
    public void testGoogleIssuedServerCardWithNickname_displaysNicknameAndLastFourAsTitle()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(
                SAMPLE_CARD_VISA, "Test nickname", CARD_ISSUER_GOOGLE);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference cardPreference = getPreferenceScreen(activity).getPreference(1);
        String title = cardPreference.getTitle().toString();
        assertThat(title).contains("Test nickname");
        assertThat(title).contains("1111");
    }

    private static PreferenceScreen getPreferenceScreen(SettingsActivity activity) {
        return ((AutofillPaymentMethodsFragment) activity.getMainFragment()).getPreferenceScreen();
    }
}
