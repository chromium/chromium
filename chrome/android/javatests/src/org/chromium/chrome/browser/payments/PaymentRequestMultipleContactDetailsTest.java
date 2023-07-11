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
import org.chromium.chrome.browser.autofill.AutofillProfile;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests contact details and a user that has
 * multiple contact detail options.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestMultipleContactDetailsTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_contact_details_test.html");

    private static final AutofillProfile[] AUTOFILL_PROFILES = {
            // 0 - Incomplete (no phone) profile.
            AutofillProfile.builder()
                    .setFullName("Bart Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setEmailAddress("bart@simpson.com")
                    .build(),

            // 1 - Incomplete (no email) profile.
            AutofillProfile.builder()
                    .setFullName("Homer Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setPhoneNumber("555 123-4567")
                    .build(),

            // 2 - Complete profile.
            AutofillProfile.builder()
                    .setFullName("Lisa Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setPhoneNumber("555 123-4567")
                    .setEmailAddress("lisa@simpson.com")
                    .build(),

            // 3 - Complete profile.
            AutofillProfile.builder()
                    .setFullName("Maggie Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setPhoneNumber("555 123-4567")
                    .setEmailAddress("maggie@simpson.com")
                    .build(),

            // 4 - Incomplete (no phone and email) profile.
            AutofillProfile.builder()
                    .setFullName("Marge Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .build(),

            // 5 - Incomplete (no name) profile.
            AutofillProfile.builder()
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setPhoneNumber("555 123-4567")
                    .setEmailAddress("marge@simpson.com")
                    .build(),

            // These profiles are used to test the dedupe of subset suggestions. They are based on
            // The Lisa Simpson profile.

            // 6 - Same as original, but with no name.
            AutofillProfile.builder()
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setPhoneNumber("555 123-4567")
                    .setEmailAddress("lisa@simpson.com")
                    .build(),

            // 7 - Same as original, but with no phone.
            AutofillProfile.builder()
                    .setFullName("Lisa Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setEmailAddress("lisa@simpson.com")
                    .build(),

            // 8 - Same as original, but with no email.
            AutofillProfile.builder()
                    .setFullName("Lisa Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setPhoneNumber("555 123-4567")
                    .build(),

            // 9 - Same as original, but with no phone and no email.
            AutofillProfile.builder()
                    .setFullName("Lisa Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .build(),

            // 10 - Has an email address that is a superset of the original profile's email.
            AutofillProfile.builder()
                    .setFullName("Lisa Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setPhoneNumber("555 123-4567")
                    .setEmailAddress("fakelisa@simpson.com")
                    .build(),

            // 11 - Has the same name as the original but with no capitalization in the name.
            AutofillProfile.builder()
                    .setFullName("lisa simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setPhoneNumber("555 123-4567")
                    .setEmailAddress("lisa@simpson.com")
                    .build(),

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
            guids.add(helper.setProfile(mProfilesToAdd[i]));
        }

        // Set up the profile use stats.
        for (int i = 0; i < guids.size(); i++) {
            // TODO(crbug.com/1463732): Update Disabled Test Callsites of
            // SetProfileUseStatsForTesting and SetCreditCardUseStatsForTesting since the underlying
            // logic has changed.
            helper.setProfileUseStatsForTesting(guids.get(i), mCountsToSet[i], mDatesToSet[i]);
        }

        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /**
     * Make sure the contact details suggestions are in the correct order and that only the top 4
     * are shown. They should be ordered by frecency and complete contact details should be
     * suggested first.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testContactDetailsSuggestionOrdering() throws TimeoutException {
        // Set the use stats so that profile[0] has the highest frecency score, profile[1] the
        // second highest, profile[2] the third lowest, profile[3] the second lowest and profile[4]
        // the lowest.
        mProfilesToAdd = new AutofillProfile[] {AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[1],
                AUTOFILL_PROFILES[2], AUTOFILL_PROFILES[3], AUTOFILL_PROFILES[4]};
        mCountsToSet = new int[] {20, 15, 10, 5, 1};
        mDatesToSet = new int[] {5000, 5000, 5000, 5000, 1};

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
        Assert.assertEquals("Lisa Simpson\n555 123-4567\nlisa@simpson.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));
        Assert.assertEquals("Maggie Simpson\n555 123-4567\nmaggie@simpson.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(1));
        Assert.assertEquals("Bart Simpson\nbart@simpson.com\nPhone number required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(2));
        Assert.assertEquals("Homer Simpson\n555 123-4567\nEmail required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(3));
    }

    /**
     * Make sure the information required message has been displayed for incomplete contact details
     * correctly.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testContactDetailsEditRequiredMessage() throws TimeoutException {
        mProfilesToAdd = new AutofillProfile[] {AUTOFILL_PROFILES[0], AUTOFILL_PROFILES[1],
                AUTOFILL_PROFILES[4], AUTOFILL_PROFILES[5]};
        mCountsToSet = new int[] {15, 10, 5, 1};
        mDatesToSet = new int[] {5000, 5000, 5000, 5000};

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
        Assert.assertEquals("Bart Simpson\nbart@simpson.com\nPhone number required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));
        Assert.assertEquals("Homer Simpson\n555 123-4567\nEmail required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(1));
        Assert.assertEquals("555 123-4567\nmarge@simpson.com\nName required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(2));
        Assert.assertEquals("Marge Simpson\nMore information required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(3));
    }

    /**
     * Makes sure that suggestions that are subsets of other fields (empty values) are deduped.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    @DisabledTest(message = "https://crbug.com/1182590")
    public void testContactDetailsDedupe_EmptyFields() throws TimeoutException {
        // Add the original profile and a bunch of similar profiles with missing fields.
        // Make sure the original profile is suggested last, to test that the suggestions are
        // sorted by completeness.
        mProfilesToAdd = new AutofillProfile[] {
                AUTOFILL_PROFILES[2],
                AUTOFILL_PROFILES[6],
                AUTOFILL_PROFILES[7],
                AUTOFILL_PROFILES[8],
                AUTOFILL_PROFILES[9],
        };
        mCountsToSet = new int[] {1, 20, 15, 10, 5};
        mDatesToSet = new int[] {1000, 4000, 3000, 2000, 1000};

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Only the original profile with all the fields should be suggested.
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
        Assert.assertEquals("Lisa Simpson\n555 123-4567\nlisa@simpson.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));
    }

    /**
     * Makes sure that suggestions where some fields values are equal but with different case are
     * deduped.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testContactDetailsDedupe_Capitalization() throws TimeoutException {
        // Add the original profile and the one where the the name is not capitalized.
        // Make sure the original profile is suggested first (no particular reason).
        mProfilesToAdd = new AutofillProfile[] {AUTOFILL_PROFILES[2], AUTOFILL_PROFILES[11]};
        mCountsToSet = new int[] {15, 5};
        mDatesToSet = new int[] {5000, 2000};

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
        Assert.assertEquals("Lisa Simpson\n555 123-4567\nlisa@simpson.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));
    }

    /**
     * Makes sure that suggestions where some fields values are subsets of the other are not
     * deduped.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testContactDetailsDontDedupe_FieldSubset() throws TimeoutException {
        // Add the original profile and the one where the email is a superset of the original.
        // Make sure the one with the superset is suggested first, because to test the subset one
        // needs to be added after.
        mProfilesToAdd = new AutofillProfile[] {AUTOFILL_PROFILES[2], AUTOFILL_PROFILES[10]};
        mCountsToSet = new int[] {15, 25};
        mDatesToSet = new int[] {5000, 7000};

        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
        Assert.assertEquals("Lisa Simpson\n555 123-4567\nfakelisa@simpson.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));
        Assert.assertEquals("Lisa Simpson\n555 123-4567\nlisa@simpson.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(1));
    }
}
