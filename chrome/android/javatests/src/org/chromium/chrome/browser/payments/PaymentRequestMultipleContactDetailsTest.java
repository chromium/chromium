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
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;

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

    private static final AutofillProfile INCOMPLETE_PROFILE_NO_PHONE =
            AutofillProfile.builder()
                    .setFullName("Bart Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setEmailAddress("bart@simpson.com")
                    .build();

    private static final AutofillProfile INCOMPLETE_PROFILE_NO_EMAIL =
            AutofillProfile.builder()
                    .setFullName("Homer Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setPhoneNumber("555 123-4567")
                    .build();

    private static final AutofillProfile COMPLETE_PROFILE_1 =
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
                    .build();

    private static final AutofillProfile COMPLETE_PROFILE_2 =
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
                    .build();

    private static final AutofillProfile INCOMPLETE_PROFILE_NO_PHONE_OR_EMAIL =
            AutofillProfile.builder()
                    .setFullName("Marge Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .build();

    private static final AutofillProfile INCOMPLETE_PROFILE_NO_NAME =
            AutofillProfile.builder()
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setPhoneNumber("555 123-4567")
                    .setEmailAddress("marge@simpson.com")
                    .build();

    // These profiles are used to test the dedupe of subset suggestions. They are based on
    // The Lisa Simpson profile.

    private static final AutofillProfile DUPLICATE_PROFILE_WITH_NO_NAME =
            AutofillProfile.builder()
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setPhoneNumber("555 123-4567")
                    .setEmailAddress("lisa@simpson.com")
                    .build();

    private static final AutofillProfile DUPLICATE_PROFILE_WITH_NO_PHONE =
            AutofillProfile.builder()
                    .setFullName("Lisa Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setEmailAddress("lisa@simpson.com")
                    .build();

    private static final AutofillProfile DUPLICATE_PROFILE_WITH_NO_EMAIL =
            AutofillProfile.builder()
                    .setFullName("Lisa Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .setPhoneNumber("555 123-4567")
                    .build();

    private static final AutofillProfile DUPLICATE_PROFILE_WITH_NO_PHONE_OR_EMAIL =
            AutofillProfile.builder()
                    .setFullName("Lisa Simpson")
                    .setCompanyName("Acme Inc.")
                    .setStreetAddress("123 Main")
                    .setRegion("California")
                    .setLocality("Los Angeles")
                    .setPostalCode("90210")
                    .setCountryCode("US")
                    .build();

    // This profile is a duplicate except that its email address is a superset of the original
    // profile (that is, the original profile's email is a suffix of this one).
    private static final AutofillProfile DUPLICATE_PROFILE_WITH_SUPERSET_EMAIL =
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
                    .build();

    private static final AutofillProfile DUPLICATE_PROFILE_WITH_MISMATCHED_CAPITALIZATION =
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
                    .build();

    @Before
    public void setUp() {
        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    private void setUpAutofillProfiles(
            AutofillProfile[] profiles, int[] counts, int[] daysSinceLastUsed)
            throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();

        // Add the profiles.
        ArrayList<String> guids = new ArrayList<>();
        for (int i = 0; i < profiles.length; i++) {
            guids.add(helper.setProfile(profiles[i]));
        }

        // Set up the profile use stats.
        for (int i = 0; i < guids.size(); i++) {
            helper.setProfileUseStatsForTesting(guids.get(i), counts[i], daysSinceLastUsed[i]);
        }
    }

    /**
     * Make sure the contact details suggestions are in the correct order and that only the top 4
     * are shown. They should be ordered by frecency and complete contact details should be
     * suggested first.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testContactDetailsSuggestionOrdering() throws TimeoutException {
        // Set the use stats such that the profiles are in descending frecency score.
        AutofillProfile[] profiles =
                new AutofillProfile[] {
                    INCOMPLETE_PROFILE_NO_PHONE,
                    INCOMPLETE_PROFILE_NO_EMAIL,
                    COMPLETE_PROFILE_1,
                    COMPLETE_PROFILE_2,
                    INCOMPLETE_PROFILE_NO_PHONE_OR_EMAIL
                };
        int[] counts = new int[] {20, 15, 10, 5, 1};
        int[] daysSinceLastUsed = new int[] {5, 5, 5, 5, 5};

        setUpAutofillProfiles(profiles, counts, daysSinceLastUsed);

        // The complete profiles should still come first, despite having a lower frecency score. The
        // profile without either a phone or email should have been sorted to last, and is then not
        // shown because we have more than PaymentUiService.SUGGESTIONS_LIMIT profiles.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods:'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
        Assert.assertEquals(
                "Lisa Simpson\n555 123-4567\nlisa@simpson.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));
        Assert.assertEquals(
                "Maggie Simpson\n555 123-4567\nmaggie@simpson.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(1));
        Assert.assertEquals(
                "Bart Simpson\nbart@simpson.com\nPhone number required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(2));
        Assert.assertEquals(
                "Homer Simpson\n555 123-4567\nEmail required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(3));
    }

    /**
     * Make sure the information required message has been displayed for incomplete contact details
     * correctly.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testContactDetailsEditRequiredMessage() throws TimeoutException {
        AutofillProfile[] profiles =
                new AutofillProfile[] {
                    INCOMPLETE_PROFILE_NO_PHONE,
                    INCOMPLETE_PROFILE_NO_EMAIL,
                    INCOMPLETE_PROFILE_NO_PHONE_OR_EMAIL,
                    INCOMPLETE_PROFILE_NO_NAME
                };
        int[] counts = new int[] {15, 10, 5, 1};
        int[] daysSinceLastUsed = new int[] {5, 5, 5, 5};

        setUpAutofillProfiles(profiles, counts, daysSinceLastUsed);

        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods:'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(4, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
        Assert.assertEquals(
                "Bart Simpson\nbart@simpson.com\nPhone number required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));
        Assert.assertEquals(
                "Homer Simpson\n555 123-4567\nEmail required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(1));
        Assert.assertEquals(
                "555 123-4567\nmarge@simpson.com\nName required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(2));
        Assert.assertEquals(
                "Marge Simpson\nMore information required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(3));
    }

    /** Makes sure that suggestions that are subsets of other fields (empty values) are deduped. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testContactDetailsDedupe_EmptyFields() throws TimeoutException {
        // Add the original profile and a bunch of similar profiles with missing fields.
        // Make sure the original profile is suggested last, to test that the suggestions are
        // sorted by completeness.
        AutofillProfile[] profiles =
                new AutofillProfile[] {
                    COMPLETE_PROFILE_1,
                    DUPLICATE_PROFILE_WITH_NO_NAME,
                    DUPLICATE_PROFILE_WITH_NO_PHONE,
                    DUPLICATE_PROFILE_WITH_NO_EMAIL,
                    DUPLICATE_PROFILE_WITH_NO_PHONE_OR_EMAIL,
                };
        int[] counts = new int[] {1, 20, 15, 10, 5};
        int[] daysSinceLastUsed = new int[] {4, 1, 2, 3, 4};

        setUpAutofillProfiles(profiles, counts, daysSinceLastUsed);

        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods:'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // Only the original profile with all the fields should be suggested.
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
        Assert.assertEquals(
                "Lisa Simpson\n555 123-4567\nlisa@simpson.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));
    }

    /**
     * Makes sure that suggestions where some fields values are equal but with different case are
     * deduped.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testContactDetailsDedupe_Capitalization() throws TimeoutException {
        // Add the original profile and the one where the the name is not capitalized.
        // Make sure the original profile is suggested first (no particular reason).
        AutofillProfile[] profiles =
                new AutofillProfile[] {
                    COMPLETE_PROFILE_1, DUPLICATE_PROFILE_WITH_MISMATCHED_CAPITALIZATION,
                };
        int[] counts = new int[] {15, 5};
        int[] daysSinceLastUsed = new int[] {2, 5};

        setUpAutofillProfiles(profiles, counts, daysSinceLastUsed);

        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods:'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
        Assert.assertEquals(
                "Lisa Simpson\n555 123-4567\nlisa@simpson.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));
    }

    /**
     * Makes sure that suggestions where some fields values are subsets of the other are not
     * deduped.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testContactDetailsDontDedupe_FieldSubset() throws TimeoutException {
        // Add the original profile and the one where the email is a superset of the original.
        // Make sure the one with the superset is suggested first, because to test the subset one
        // needs to be added after.
        AutofillProfile[] profiles =
                new AutofillProfile[] {
                    COMPLETE_PROFILE_1, DUPLICATE_PROFILE_WITH_SUPERSET_EMAIL,
                };
        int[] counts = new int[] {15, 25};
        int[] daysSinceLastUsed = new int[] {7, 5};

        setUpAutofillProfiles(profiles, counts, daysSinceLastUsed);

        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods:'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(2, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
        Assert.assertEquals(
                "Lisa Simpson\n555 123-4567\nfakelisa@simpson.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));
        Assert.assertEquals(
                "Lisa Simpson\n555 123-4567\nlisa@simpson.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(1));
    }
}
