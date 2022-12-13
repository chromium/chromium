// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requires shipping address to calculate shipping
 * and user that has 5 addresses stored in autofill settings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestDynamicShippingMultipleAddressesTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_dynamic_shipping_test.html");

    private static final AutofillProfile[] AUTOFILL_PROFILES = {
            // Incomplete profile_0 (missing phone number)
            new AutofillProfile("" /* guid */, "https://www.example.test" /* origin */,
                    "" /* honorific prefix */, "Bart Simpson", "Acme Inc.", "123 Main",
                    "California", "Los Angeles", "", "90210", "", "US", "", "bart@simpson.com", ""),

            // Incomplete profile_1 (missing street address).
            new AutofillProfile("" /* guid */, "https://www.example.test" /* origin */,
                    "" /* honorific prefix */, "Homer Simpson", "Acme Inc.", "", "California",
                    "Los Angeles", "", "90210", "", "US", "555 123-4567", "homer@simpson.com", ""),

            // Complete profile_2.
            new AutofillProfile("" /* guid */, "https://www.example.test" /* origin */,
                    "" /* honorific prefix */, "Lisa Simpson", "Acme Inc.", "123 Main",
                    "California", "Los Angeles", "", "90210", "", "US", "555 123-4567",
                    "lisa@simpson.com", ""),

            // Complete profile_3 in another country.
            new AutofillProfile("" /* guid */, "https://www.example.test" /* origin */,
                    "" /* honorific prefix */, "Maggie Simpson", "Acme Inc.", "123 Main",
                    "California", "Los Angeles", "", "90210", "", "Uzbekistan", "555 123-4567",
                    "maggie@simpson.com", ""),

            // Incomplete profile_4 (invalid address, missing city name).
            new AutofillProfile("" /* guid */, "https://www.example.test" /* origin */,
                    "" /* honorific prefix */, "Marge Simpson", "Acme Inc.", "123 Main",
                    "California", "", "", "90210", "", "US", "555 123-4567", "marge@simpson.com",
                    ""),

            // Incomplete profile_5 (missing recipient name).
            new AutofillProfile("" /* guid */, "https://www.example.test" /* origin */,
                    "" /* honorific prefix */, "", "Acme Inc.", "123 Main", "California",
                    "Los Angeles", "", "90210", "", "US", "555 123-4567", "lisa@simpson.com", ""),

            // Incomplete profile_6 (need more information: name and address both missing/invalid).
            new AutofillProfile("" /* guid */, "https://www.example.test" /* origin */,
                    "" /* honorific prefix */, "", "Acme Inc.", "123 Main", "California", "", "",
                    "90210", "", "US", "555 123-4567", "lisa@simpson.com", ""),

            // Incomplete profile_7 (missing phone number, different from AutofillProfile[0])
            new AutofillProfile("" /* guid */, "https://www.example.test" /* origin */,
                    "" /* honorific prefix */, "John Smith", "Acme Inc.", "123 Main", "California",
                    "Los Angeles", "", "90210", "", "US", "", "bart@simpson.com", ""),
    };

    private AutofillProfile[] mProfilesToAdd;
    private int[] mCountsToSet;
    private int[] mDatesToSet;

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();

        // Add the profiles.
        ArrayList<String> guids = new ArrayList<>();
        for (int i = 0; i < mProfilesToAdd.length; i++) {
            // The user has a shipping address on disk.
            String billingAddressId = helper.setProfile(mProfilesToAdd[i]);
            guids.add(billingAddressId);
            helper.setCreditCard(new CreditCard("", "https://example.test", true, true, "Jon Doe",
                    "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                    billingAddressId, "" /* serverId */));
        }

        // Set up the profile use stats.
        for (int i = 0; i < guids.size(); i++) {
            helper.setProfileUseStatsForTesting(guids.get(i), mCountsToSet[i], mDatesToSet[i]);
        }
    }

    /**
     * Make sure the address suggestions are in the correct order and that only the top 4
     * suggestions are shown. Complete profiles come first and ordered by frecency. Incomplete
     * profiles are ordered by their completeness score.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testShippingAddressSuggestionOrdering() throws TimeoutException {
        // Create two complete and two incomplete profiles. Values are set so that complete profiles
        // are ordered by frecency, incomplete profiles are sorted by their completeness score.
        mProfilesToAdd = new AutofillProfile[] {
                AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[2], AUTOFILL_PROFILES[3],
                AUTOFILL_PROFILES[4]};
        mCountsToSet = new int[] {20, 15, 10, 25};
        mDatesToSet = new int[] {5000, 5000, 5000, 5000};

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfShippingAddressSuggestions());
        int i = 0;
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Lisa Simpson"));
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Maggie Simpson"));
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Bart Simpson"));
        // Even though Profile[4] (missing address) has higher frecency than Profile[0] (missing
        // phone number), it ranks lower than Profile[0] since its completeness score is lower.
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Marge Simpson"));
    }

    /**
     * Make sure that equally incomplete profiles are ordered by their frecency.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testEquallyIncompleteSuggestionsOrdering() throws TimeoutException {
        // Create two profiles both with missing phone numbers.
        mProfilesToAdd = new AutofillProfile[] {AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[7]};
        mCountsToSet = new int[] {20, 30};
        mDatesToSet = new int[] {5000, 5000};

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfShippingAddressSuggestions());
        int i = 0;
        // Incomplete profile with higher frecency comes first.
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "John Smith"));
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Bart Simpson"));
    }

    /**
     * Make sure that a maximum of four profiles are shown to the user.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testShippingAddressSuggestionLimit() throws TimeoutException {
        // Create five profiles that can be suggested to the user.
        mProfilesToAdd = new AutofillProfile[] {
                AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[2], AUTOFILL_PROFILES[3],
                AUTOFILL_PROFILES[4], AUTOFILL_PROFILES[5]};
        mCountsToSet = new int[] {20, 15, 10, 5, 2, 1};
        mDatesToSet = new int[] {5000, 5000, 5000, 5000, 2, 1};

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        // Only four profiles should be suggested to the user.
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfShippingAddressSuggestions());
        int i = 0;
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Lisa Simpson"));
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Maggie Simpson"));
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Bart Simpson"));
        // Profiles[5] is suggested as the last option since it is missing recipient name and ranks
        // above Profile[4] with missing address.
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i).contains(
                "Los Angeles"));
        Assert.assertFalse(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i).contains(
                "Marge Simpson"));
    }

    /**
     * Make sure that only profiles with a street address are suggested to the user.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testShippingAddressSuggestion_OnlyIncludeProfilesWithStreetAddress()
            throws TimeoutException {
        // Create two complete profiles and two incomplete profiles, one of which has no street
        // address.
        mProfilesToAdd = new AutofillProfile[] {
                AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[1], AUTOFILL_PROFILES[2],
                AUTOFILL_PROFILES[3]};
        mCountsToSet = new int[] {15, 10, 5, 1};
        mDatesToSet = new int[] {5000, 5000, 5000, 1};

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        // Only 3 profiles should be suggested, the two complete ones and the incomplete one that
        // has a street address.
        Assert.assertEquals(3, mPaymentRequestTestRule.getNumberOfShippingAddressSuggestions());
        int i = 0;
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Lisa Simpson"));
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Maggie Simpson"));
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Bart Simpson"));
    }

    /**
     * Select a shipping address that the website refuses to accept, which should force the dialog
     * to show an error.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testShippingAddresNotAcceptedByMerchant() throws TimeoutException {
        // Add a profile that is not accepted by the website.
        mProfilesToAdd = new AutofillProfile[] {AUTOFILL_PROFILES[3]};
        mCountsToSet = new int[] {5};
        mDatesToSet = new int[] {5000};

        // Click on the unacceptable shipping address.
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(0).contains(
                AUTOFILL_PROFILES[3].getFullName()));
        mPaymentRequestTestRule.clickOnShippingAddressSuggestionOptionAndWait(
                0, mPaymentRequestTestRule.getSelectionChecked());

        // The string should reflect the error sent from the merchant.
        CharSequence actualString =
                mPaymentRequestTestRule.getShippingAddressOptionRowAtIndex(0).getLabelText();
        Assert.assertEquals("We do not ship to this address", actualString);
    }

    /**
     * Make sure the information required message has been displayed for incomplete profile
     * correctly.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testShippingAddressEditRequiredMessage() throws TimeoutException {
        // Create four incomplete profiles with different missing information. Profiles will be
        // sorted based on their missing fields.
        mProfilesToAdd = new AutofillProfile[] {AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[4],
                AUTOFILL_PROFILES[5], AUTOFILL_PROFILES[6]};
        mCountsToSet = new int[] {15, 10, 5, 25};
        mDatesToSet = new int[] {5000, 5000, 5000, 5000};

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Incomplete addresses are sorted by completeness score.
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfShippingAddressSuggestions());
        int i = 0;
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Phone number required"));
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Name required"));
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "Enter a valid address"));
        Assert.assertTrue(mPaymentRequestTestRule.getShippingAddressSuggestionLabel(i++).contains(
                "More information required"));
    }
}
