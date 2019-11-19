// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.test.ChromeBrowserTestRule;

import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for Chrome on Android's usage of the PersonalDataManager API.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PersonalDataManagerTest {
    @Rule
    public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private AutofillTestHelper mHelper;

    @Before
    public void setUp() {
        mHelper = new AutofillTestHelper();
    }

    private AutofillProfile createTestProfile() {
        return new AutofillProfile("" /* guid */, "" /* origin */, "John Major", "Acme Inc.",
                "123 Main", "California", "Los Angeles", "", "90210", "", "US", "555 123-4567",
                "jm@example.com", "");
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testAddAndEditProfiles() throws TimeoutException {
        AutofillProfile profile = new AutofillProfile("" /* guid */, "" /* origin */, "John Smith",
                "Acme Inc.", "1 Main\nApt A", "CA", "San Francisco", "", "94102", "", "US",
                "4158889999", "john@acme.inc", "");
        String profileOneGUID = mHelper.setProfile(profile);
        Assert.assertEquals(1, mHelper.getNumberOfProfilesForSettings());

        AutofillProfile profile2 = new AutofillProfile("" /* guid */, "" /* origin */,
                "John Hackock", "Acme Inc.", "1 Main\nApt A", "CA", "San Francisco", "", "94102",
                "", "US", "4158889999", "john@acme.inc", "");
        String profileTwoGUID = mHelper.setProfile(profile2);
        Assert.assertEquals(2, mHelper.getNumberOfProfilesForSettings());

        profile.setGUID(profileOneGUID);
        profile.setCountryCode("CA");
        mHelper.setProfile(profile);
        Assert.assertEquals(
                "Should still have only two profiles", 2, mHelper.getNumberOfProfilesForSettings());

        AutofillProfile storedProfile = mHelper.getProfile(profileOneGUID);
        Assert.assertEquals(profileOneGUID, storedProfile.getGUID());
        Assert.assertEquals("", storedProfile.getOrigin());
        Assert.assertEquals("CA", storedProfile.getCountryCode());
        Assert.assertEquals("San Francisco", storedProfile.getLocality());
        Assert.assertNotNull(mHelper.getProfile(profileTwoGUID));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testUpdateLanguageCodeInProfile() throws TimeoutException {
        AutofillProfile profile = new AutofillProfile("" /* guid */, "" /* origin */, "John Smith",
                "Acme Inc.", "1 Main\nApt A", "CA", "San Francisco", "", "94102", "", "US",
                "4158889999", "john@acme.inc", "fr");
        Assert.assertEquals("fr", profile.getLanguageCode());
        String profileOneGUID = mHelper.setProfile(profile);
        Assert.assertEquals(1, mHelper.getNumberOfProfilesForSettings());

        AutofillProfile storedProfile = mHelper.getProfile(profileOneGUID);
        Assert.assertEquals(profileOneGUID, storedProfile.getGUID());
        Assert.assertEquals("fr", storedProfile.getLanguageCode());
        Assert.assertEquals("US", storedProfile.getCountryCode());

        profile.setGUID(profileOneGUID);
        profile.setLanguageCode("en");
        mHelper.setProfile(profile);

        AutofillProfile storedProfile2 = mHelper.getProfile(profileOneGUID);
        Assert.assertEquals(profileOneGUID, storedProfile2.getGUID());
        Assert.assertEquals("en", storedProfile2.getLanguageCode());
        Assert.assertEquals("US", storedProfile2.getCountryCode());
        Assert.assertEquals("San Francisco", storedProfile2.getLocality());
        Assert.assertEquals("", storedProfile2.getOrigin());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testAddAndDeleteProfile() throws TimeoutException {
        String profileOneGUID = mHelper.setProfile(createTestProfile());
        Assert.assertEquals(1, mHelper.getNumberOfProfilesForSettings());

        mHelper.deleteProfile(profileOneGUID);
        Assert.assertEquals(0, mHelper.getNumberOfProfilesForSettings());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testAddAndEditCreditCards() throws TimeoutException {
        CreditCard card = new CreditCard(
                "" /* guid */, "" /* origin */, "Visa", "1234123412341234", "", "5", "2020");
        String cardOneGUID = mHelper.setCreditCard(card);
        Assert.assertEquals(1, mHelper.getNumberOfCreditCardsForSettings());

        CreditCard card2 = new CreditCard("" /* guid */, "" /* origin */, "American Express",
                "1234123412341234", "", "8", "2020");
        String cardTwoGUID = mHelper.setCreditCard(card2);
        Assert.assertEquals(2, mHelper.getNumberOfCreditCardsForSettings());

        card.setGUID(cardOneGUID);
        card.setMonth("10");
        card.setNumber("4012888888881881");
        mHelper.setCreditCard(card);
        Assert.assertEquals(
                "Should still have only two cards", 2, mHelper.getNumberOfCreditCardsForSettings());

        CreditCard storedCard = mHelper.getCreditCard(cardOneGUID);
        Assert.assertEquals(cardOneGUID, storedCard.getGUID());
        Assert.assertEquals("", storedCard.getOrigin());
        Assert.assertEquals("Visa", storedCard.getName());
        Assert.assertEquals("10", storedCard.getMonth());
        Assert.assertEquals("4012888888881881", storedCard.getNumber());
        // \u0020\...\u2060 is four dots ellipsis, \u202A is the Left-To-Right Embedding (LTE) mark,
        // \u202C is the Pop Directional Formatting (PDF) mark. Expected string with form
        // 'Visa  <LRE>****1881<PDF>'.
        Assert.assertEquals(
                "Visa\u0020\u0020\u202A\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060\u2022"
                        + "\u2060\u2006\u2060\u2022\u2060\u2006\u20601881\u202C",
                storedCard.getObfuscatedNumber());
        Assert.assertNotNull(mHelper.getCreditCard(cardTwoGUID));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testAddAndDeleteCreditCard() throws TimeoutException {
        CreditCard card = new CreditCard(
                "" /* guid */, "Chrome settings" /* origin */,
                "Visa", "1234123412341234", "", "5", "2020");
        String cardOneGUID = mHelper.setCreditCard(card);
        Assert.assertEquals(1, mHelper.getNumberOfCreditCardsForSettings());

        mHelper.deleteCreditCard(cardOneGUID);
        Assert.assertEquals(0, mHelper.getNumberOfCreditCardsForSettings());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testRespectCountryCodes() throws TimeoutException {
        // The constructor should accept country names and ISO 3166-1-alpha-2 country codes.
        // getCountryCode() should return a country code.
        AutofillProfile profile1 = new AutofillProfile("" /* guid */, "" /* origin */, "John Smith",
                "Acme Inc.", "1 Main\nApt A", "Quebec", "Montreal", "", "H3B 2Y5", "", "Canada",
                "514-670-1234", "john@acme.inc", "");
        String profileGuid1 = mHelper.setProfile(profile1);

        AutofillProfile profile2 = new AutofillProfile("" /* guid */, "" /* origin */, "Greg Smith",
                "Ucme Inc.", "123 Bush\nApt 125", "Quebec", "Montreal", "", "H3B 2Y5", "", "CA",
                "514-670-4321", "greg@ucme.inc", "");
        String profileGuid2 = mHelper.setProfile(profile2);

        Assert.assertEquals(2, mHelper.getNumberOfProfilesForSettings());

        AutofillProfile storedProfile1 = mHelper.getProfile(profileGuid1);
        Assert.assertEquals("CA", storedProfile1.getCountryCode());

        AutofillProfile storedProfile2 = mHelper.getProfile(profileGuid2);
        Assert.assertEquals("CA", storedProfile2.getCountryCode());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testMultilineStreetAddress() throws TimeoutException {
        final String streetAddress1 = "Chez Mireille COPEAU Appartment. 2\n"
                + "Entree A Batiment Jonquille\n"
                + "25 RUE DE L'EGLISE";
        final String streetAddress2 = streetAddress1 + "\n"
                + "Fourth floor\n"
                + "The red bell";
        AutofillProfile profile =
                new AutofillProfile("" /* guid */, "" /* origin */, "Monsieur Jean DELHOURME",
                        "Acme Inc.", streetAddress1, "Tahiti", "Mahina", "Orofara", "98709",
                        "CEDEX 98703", "French Polynesia", "44.71.53", "john@acme.inc", "");
        String profileGuid1 = mHelper.setProfile(profile);
        Assert.assertEquals(1, mHelper.getNumberOfProfilesForSettings());
        AutofillProfile storedProfile1 = mHelper.getProfile(profileGuid1);
        Assert.assertEquals("PF", storedProfile1.getCountryCode());
        Assert.assertEquals("Monsieur Jean DELHOURME", storedProfile1.getFullName());
        Assert.assertEquals(streetAddress1, storedProfile1.getStreetAddress());
        Assert.assertEquals("Tahiti", storedProfile1.getRegion());
        Assert.assertEquals("Mahina", storedProfile1.getLocality());
        Assert.assertEquals("Orofara", storedProfile1.getDependentLocality());
        Assert.assertEquals("98709", storedProfile1.getPostalCode());
        Assert.assertEquals("CEDEX 98703", storedProfile1.getSortingCode());
        Assert.assertEquals("44.71.53", storedProfile1.getPhoneNumber());
        Assert.assertEquals("john@acme.inc", storedProfile1.getEmailAddress());

        profile.setStreetAddress(streetAddress2);
        String profileGuid2 = mHelper.setProfile(profile);
        Assert.assertEquals(2, mHelper.getNumberOfProfilesForSettings());
        AutofillProfile storedProfile2 = mHelper.getProfile(profileGuid2);
        Assert.assertEquals(streetAddress2, storedProfile2.getStreetAddress());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testLabels() throws TimeoutException {
        AutofillProfile profile1 = new AutofillProfile("" /* guid */, "" /* origin */, "John Major",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "US",
                "555 123-4567", "jm@example.com", "");
        // An almost identical profile.
        AutofillProfile profile2 = new AutofillProfile("" /* guid */, "" /* origin */, "John Major",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "US",
                "555 123-4567", "jm-work@example.com", "");
        // A different profile.
        AutofillProfile profile3 = new AutofillProfile("" /* guid */, "" /* origin */,
                "Jasper Lundgren", "", "1500 Second Ave", "California", "Hollywood", "", "90068",
                "", "US", "555 123-9876", "jasperl@example.com", "");
        // A profile where a lot of stuff is missing.
        AutofillProfile profile4 = new AutofillProfile("" /* guid */, "" /* origin */,
                "Joe Sergeant", "", "", "Texas", "Fort Worth", "", "", "", "US", "", "", "");

        mHelper.setProfile(profile1);
        mHelper.setProfile(profile2);
        mHelper.setProfile(profile3);
        mHelper.setProfile(profile4);

        List<String> expectedLabels = new LinkedList<String>();
        expectedLabels.add("123 Main, jm@example.com");
        expectedLabels.add("123 Main, jm-work@example.com");
        expectedLabels.add("1500 Second Ave, 90068");
        expectedLabels.add("Fort Worth, Texas");

        List<AutofillProfile> profiles = mHelper.getProfilesForSettings();
        Assert.assertEquals(expectedLabels.size(), profiles.size());
        for (int i = 0; i < profiles.size(); ++i) {
            String label = profiles.get(i).getLabel();
            int idx = expectedLabels.indexOf(label);
            Assert.assertFalse("Found unexpected label [" + label + "]", -1 == idx);
            expectedLabels.remove(idx);
        }
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testProfilesFrecency() throws TimeoutException {
        // Create 3 profiles.
        AutofillProfile profile1 = new AutofillProfile("" /* guid */, "" /* origin */, "John Major",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "US",
                "555 123-4567", "jm@example.com", "");
        AutofillProfile profile2 = new AutofillProfile("" /* guid */, "" /* origin */, "John Major",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "US",
                "555 123-4567", "jm-work@example.com", "");
        AutofillProfile profile3 = new AutofillProfile("" /* guid */, "" /* origin */,
                "Jasper Lundgren", "", "1500 Second Ave", "California", "Hollywood", "", "90068",
                "", "US", "555 123-9876", "jasperl@example.com", "");

        String guid1 = mHelper.setProfile(profile1);
        String guid2 = mHelper.setProfile(profile2);
        String guid3 = mHelper.setProfile(profile3);

        // The first profile has a lower use count than the two other profiles. It also has an older
        // use date that the second profile and the same use date as the third. It should be last.
        mHelper.setProfileUseStatsForTesting(guid1, 3, 5000);
        // The second profile has the same use count as the third but a more recent use date. It
        // also has a bigger use count that the first profile. It should be first.
        mHelper.setProfileUseStatsForTesting(guid2, 6, 5001);
        // The third profile has the same use count as the second but an older use date. It also has
        // a bigger use count that the first profile. It should be second.
        mHelper.setProfileUseStatsForTesting(guid3, 6, 5000);

        List<AutofillProfile> profiles =
                mHelper.getProfilesToSuggest(false /* includeNameInLabel */);
        Assert.assertEquals(3, profiles.size());
        Assert.assertTrue(
                "Profile2 should be ranked first", guid2.equals(profiles.get(0).getGUID()));
        Assert.assertTrue(
                "Profile3 should be ranked second", guid3.equals(profiles.get(1).getGUID()));
        Assert.assertTrue(
                "Profile1 should be ranked third", guid1.equals(profiles.get(2).getGUID()));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testCreditCardsFrecency() throws TimeoutException {
        // Create 3 credit cards.
        CreditCard card1 = new CreditCard(
                "" /* guid */, "" /* origin */, "Visa", "1234123412341234", "", "5", "2020");

        CreditCard card2 = new CreditCard("" /* guid */, "http://www.example.com" /* origin */,
                "American Express", "1234123412341234", "", "8", "2020");

        CreditCard card3 = new CreditCard("" /* guid */, "http://www.example.com" /* origin */,
                "Mastercard", "1234123412341234", "", "11", "2020");

        String guid1 = mHelper.setCreditCard(card1);
        String guid2 = mHelper.setCreditCard(card2);
        String guid3 = mHelper.setCreditCard(card3);

        // The first card has a lower use count than the two other cards. It also has an older
        // use date that the second card and the same use date as the third. It should be last.
        mHelper.setCreditCardUseStatsForTesting(guid1, 3, 5000);
        // The second card has the same use count as the third but a more recent use date. It also
        // has a bigger use count that the first card. It should be first.
        mHelper.setCreditCardUseStatsForTesting(guid2, 6, 5001);
        // The third card has the same use count as the second but an older use date. It also has a
        // bigger use count that the first card. It should be second.
        mHelper.setCreditCardUseStatsForTesting(guid3, 6, 5000);

        List<CreditCard> cards = mHelper.getCreditCardsToSuggest();
        Assert.assertEquals(3, cards.size());
        Assert.assertTrue("Card2 should be ranked first", guid2.equals(cards.get(0).getGUID()));
        Assert.assertTrue("Card3 should be ranked second", guid3.equals(cards.get(1).getGUID()));
        Assert.assertTrue("Card1 should be ranked third", guid1.equals(cards.get(2).getGUID()));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testCreditCardsDeduping() throws TimeoutException {
        // Create a local card and an identical server card.
        CreditCard card1 = new CreditCard("" /* guid */, "" /* origin */, true /* isLocal */,
                false /* isCached */, "John Doe", "1234123412341234", "", "5", "2020", "Visa",
                0 /* issuerIconDrawableId */, CardType.UNKNOWN, "" /* billingAddressId */,
                "" /* serverId */);

        CreditCard card2 = new CreditCard("" /* guid */, "" /* origin */, false /* isLocal */,
                false /* isCached */, "John Doe", "1234123412341234", "", "5", "2020", "Visa",
                0 /* issuerIconDrawableId */, CardType.UNKNOWN, "" /* billingAddressId */,
                "" /* serverId */);

        mHelper.setCreditCard(card1);
        mHelper.addServerCreditCard(card2);

        // Only one card should be suggested to the user since the two are identical.
        Assert.assertEquals(1, mHelper.getNumberOfCreditCardsToSuggest());

        // Both cards should be seen in the settings even if they are identical.
        Assert.assertEquals(2, mHelper.getNumberOfCreditCardsForSettings());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testProfileUseStatsSettingAndGetting() throws TimeoutException {
        String guid = mHelper.setProfile(createTestProfile());

        // Make sure the profile does not have the specific use stats form the start.
        Assert.assertTrue(1234 != mHelper.getProfileUseCountForTesting(guid));
        Assert.assertTrue(1234 != mHelper.getProfileUseDateForTesting(guid));

        // Set specific use stats for the profile.
        mHelper.setProfileUseStatsForTesting(guid, 1234, 1234);

        // Make sure the specific use stats were set for the profile.
        Assert.assertEquals(1234, mHelper.getProfileUseCountForTesting(guid));
        Assert.assertEquals(1234, mHelper.getProfileUseDateForTesting(guid));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testCreditCardUseStatsSettingAndGetting() throws TimeoutException {
        String guid = mHelper.setCreditCard(new CreditCard("" /* guid */, "" /* origin */,
                true /* isLocal */, false /* isCached */, "John Doe", "1234123412341234", "", "5",
                "2020", "Visa", 0 /* issuerIconDrawableId */, CardType.UNKNOWN,
                "" /* billingAddressId */, "" /* serverId */));

        // Make sure the credit card does not have the specific use stats form the start.
        Assert.assertTrue(1234 != mHelper.getCreditCardUseCountForTesting(guid));
        Assert.assertTrue(1234 != mHelper.getCreditCardUseDateForTesting(guid));

        // Set specific use stats for the credit card.
        mHelper.setCreditCardUseStatsForTesting(guid, 1234, 1234);

        // Make sure the specific use stats were set for the credit card.
        Assert.assertEquals(1234, mHelper.getCreditCardUseCountForTesting(guid));
        Assert.assertEquals(1234, mHelper.getCreditCardUseDateForTesting(guid));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testRecordAndLogProfileUse() throws TimeoutException {
        String guid = mHelper.setProfile(createTestProfile());

        // Set specific use stats for the profile.
        mHelper.setProfileUseStatsForTesting(guid, 1234, 1234);

        // Get the current date value just before the call to record and log.
        long timeBeforeRecord = mHelper.getCurrentDateForTesting();

        // Record and log use of the profile.
        mHelper.recordAndLogProfileUse(guid);

        // Get the current date value just after the call to record and log.
        long timeAfterRecord = mHelper.getCurrentDateForTesting();

        // Make sure the use stats of the profile were updated.
        Assert.assertEquals(1235, mHelper.getProfileUseCountForTesting(guid));
        Assert.assertTrue(timeBeforeRecord <= mHelper.getProfileUseDateForTesting(guid));
        Assert.assertTrue(timeAfterRecord >= mHelper.getProfileUseDateForTesting(guid));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testRecordAndLogCreditCardUse() throws TimeoutException {
        String guid = mHelper.setCreditCard(new CreditCard("" /* guid */, "" /* origin */,
                true /* isLocal */, false /* isCached */, "John Doe", "1234123412341234", "", "5",
                "2020", "Visa", 0 /* issuerIconDrawableId */, CardType.UNKNOWN,
                "" /* billingAddressId */, "" /* serverId */));

        // Set specific use stats for the credit card.
        mHelper.setCreditCardUseStatsForTesting(guid, 1234, 1234);

        // Get the current date value just before the call to record and log.
        long timeBeforeRecord = mHelper.getCurrentDateForTesting();

        // Record and log use of the credit card.
        mHelper.recordAndLogCreditCardUse(guid);

        // Get the current date value just after the call to record and log.
        long timeAfterRecord = mHelper.getCurrentDateForTesting();

        // Make sure the use stats of the credit card were updated.
        Assert.assertEquals(1235, mHelper.getCreditCardUseCountForTesting(guid));
        Assert.assertTrue(timeBeforeRecord <= mHelper.getCreditCardUseDateForTesting(guid));
        Assert.assertTrue(timeAfterRecord >= mHelper.getCreditCardUseDateForTesting(guid));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testGetProfilesToSuggest_NoName() throws TimeoutException {
        mHelper.setProfile(createTestProfile());

        List<AutofillProfile> profiles =
                mHelper.getProfilesToSuggest(false /* includeNameInLabel */);
        Assert.assertEquals("Acme Inc., 123 Main, Los Angeles, California 90210, United States",
                profiles.get(0).getLabel());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @RetryOnFailure
    public void testGetProfilesToSuggest_WithName() throws TimeoutException {
        mHelper.setProfile(createTestProfile());

        List<AutofillProfile> profiles =
                mHelper.getProfilesToSuggest(true /* includeNameInLabel */);
        Assert.assertEquals("John Major, Acme Inc., 123 Main, Los Angeles, California 90210, "
                        + "United States",
                profiles.get(0).getLabel());
    }
}
