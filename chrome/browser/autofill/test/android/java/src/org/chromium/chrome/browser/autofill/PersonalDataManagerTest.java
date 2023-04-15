// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createLocalCreditCard;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.ValueWithStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.video_tutorials.test.TestImageFetcher;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for Chrome on Android's usage of the PersonalDataManager API.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PersonalDataManagerTest {
    private static final Bitmap TEST_CARD_ART_IMAGE =
            Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);

    @Rule
    public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public final TestRule mFeaturesProcessorRule = new Features.InstrumentationProcessor();

    private AutofillTestHelper mHelper;

    @Before
    public void setUp() {
        mHelper = new AutofillTestHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> PersonalDataManager.getInstance().setImageFetcherForTesting(
                                new TestImageFetcher(TEST_CARD_ART_IMAGE)));
    }

    @After
    public void tearDown() throws TimeoutException {
        mHelper.clearAllDataForTesting();
    }

    private AutofillProfile createTestProfile() {
        return new AutofillProfile("" /* guid */, "" /* origin */, "" /* honorific prefix */,
                "John Major", "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "",
                "US", "555 123-4567", "jm@example.com", "");
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndEditProfiles() throws TimeoutException {
        AutofillProfile profile = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "John Smith", "Acme Inc.", "1 Main\nApt A", "CA",
                "San Francisco", "", "94102", "", "US", "4158889999", "john@acme.inc", "");
        String profileOneGUID = mHelper.setProfile(profile);
        Assert.assertEquals(1, mHelper.getNumberOfProfilesForSettings());

        AutofillProfile profile2 = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "John Hackock", "Acme Inc.", "1 Main\nApt A", "CA",
                "San Francisco", "", "94102", "", "US", "4158889999", "john@acme.inc", "");
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
    public void testUpdateLanguageCodeInProfile() throws TimeoutException {
        AutofillProfile profile = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "John Smith", "Acme Inc.", "1 Main\nApt A", "CA",
                "San Francisco", "", "94102", "", "US", "4158889999", "john@acme.inc", "fr");
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
    public void testAddAndDeleteProfile() throws TimeoutException {
        String profileOneGUID = mHelper.setProfile(createTestProfile());
        Assert.assertEquals(1, mHelper.getNumberOfProfilesForSettings());

        mHelper.deleteProfile(profileOneGUID);
        Assert.assertEquals(0, mHelper.getNumberOfProfilesForSettings());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndEditCreditCards() throws TimeoutException {
        CreditCard card = createLocalCreditCard("Visa", "1234123412341234", "5", "2020");
        String cardOneGUID = mHelper.setCreditCard(card);
        Assert.assertEquals(1, mHelper.getNumberOfCreditCardsForSettings());

        CreditCard card2 =
                createLocalCreditCard("American Express", "1234123412341234", "8", "2020");
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
                storedCard.getNetworkAndLastFourDigits());
        Assert.assertNotNull(mHelper.getCreditCard(cardTwoGUID));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndEditCreditCardNickname() throws TimeoutException {
        CreditCard cardWithoutNickname = createLocalCreditCard(
                "Visa", "1234123412341234", "5", AutofillTestHelper.nextYear());
        String nickname = "test nickname";
        CreditCard cardWithNickname = createLocalCreditCard(
                "American Express", "1234123412341234", "8", AutofillTestHelper.nextYear());
        cardWithNickname.setNickname(nickname);

        String cardWithoutNicknameGuid = mHelper.setCreditCard(cardWithoutNickname);
        String cardWithNicknameGuid = mHelper.setCreditCard(cardWithNickname);

        CreditCard storedCardWithoutNickname = mHelper.getCreditCard(cardWithoutNicknameGuid);
        CreditCard storedCardWithNickname = mHelper.getCreditCard(cardWithNicknameGuid);
        assertThat(storedCardWithoutNickname.getNickname()).isEmpty();
        assertThat(storedCardWithNickname.getNickname()).isEqualTo(nickname);
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testCreditCardWithCardArtUrl_imageDownloaded() throws TimeoutException {
        Context context = ContextUtils.getApplicationContext();

        int widthId = R.dimen.settings_page_card_icon_width;
        int width = context.getResources().getDimensionPixelSize(widthId);
        int heightId = R.dimen.settings_page_card_icon_height;
        int height = context.getResources().getDimensionPixelSize(heightId);
        int cornerRadiusId = R.dimen.card_art_corner_radius;
        float cornerRadius = context.getResources().getDimensionPixelSize(cornerRadiusId);
        GURL cardArtUrl = new GURL("http://google.com/test.png");
        CreditCard cardWithCardArtUrl = new CreditCard(/* guid= */ "serverGuid", /* origin= */ "",
                /* isLocal= */ false, /* isCached= */ false, "John Doe Server", "41111111111111111",
                /* obfuscatedCardNumber= */ "", "3", "2019", "Visa", /* issuerIconDrawableId= */ 0,
                /* billingAddressId= */ "",
                /* serverId= */ "serverId");
        cardWithCardArtUrl.setCardArtUrl(cardArtUrl);

        mHelper.addServerCreditCard(cardWithCardArtUrl);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // On the first attempt, the card arts are only fetched from the server, they're not
            // rendered (crbug.com/1384128).
            assertEquals(null,
                    PersonalDataManager.getInstance()
                            .getCustomImageForAutofillSuggestionIfAvailable(
                                    ContextUtils.getApplicationContext(), cardArtUrl, widthId,
                                    heightId, cornerRadiusId));
            assertTrue(
                    AutofillUiUtils
                            .resizeAndAddRoundedCornersAndGreyBorder(TEST_CARD_ART_IMAGE, width,
                                    height, cornerRadius,
                                    /* addRoundedCornersAndGreyBorder= */
                                    ChromeFeatureList.isEnabled(
                                            ChromeFeatureList
                                                    .AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES))
                            .sameAs(PersonalDataManager.getInstance()
                                            .getCustomImageForAutofillSuggestionIfAvailable(
                                                    ContextUtils.getApplicationContext(),
                                                    cardArtUrl, widthId, heightId,
                                                    cornerRadiusId)));
        });
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testCreditCardArtUrlIsFormattedWithImageSpecs() throws TimeoutException {
        GURL capitalOneIconUrl = new GURL(AutofillUiUtils.CAPITAL_ONE_ICON_URL);
        GURL cardArtUrl = new GURL("http://google.com/test");
        int widthPixels = 32;
        int heightPixels = 20;

        // For virtual card icon, the URL should not be updated. For card art icon, the URL should
        // be updated as `cardArtUrl=w{width}-h{height}`.
        assertThat(AutofillUiUtils.getCreditCardIconUrlWithParams(
                           capitalOneIconUrl, widthPixels, heightPixels))
                .isEqualTo(capitalOneIconUrl);
        assertThat(AutofillUiUtils.getCreditCardIconUrlWithParams(
                           cardArtUrl, widthPixels, heightPixels))
                .isEqualTo(new GURL(new StringBuilder(cardArtUrl.getSpec())
                                            .append("=w")
                                            .append(widthPixels)
                                            .append("-h")
                                            .append(heightPixels)
                                            .toString()));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndDeleteCreditCard() throws TimeoutException {
        CreditCard card = createLocalCreditCard("Visa", "1234123412341234", "5", "2020");
        card.setOrigin("Chrome settings");
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
        AutofillProfile profile1 = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "John Smith", "Acme Inc.", "1 Main\nApt A", "Quebec",
                "Montreal", "", "H3B 2Y5", "", "Canada", "514-670-1234", "john@acme.inc", "");
        String profileGuid1 = mHelper.setProfile(profile1);

        AutofillProfile profile2 = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "Greg Smith", "Ucme Inc.", "123 Bush\nApt 125", "Quebec",
                "Montreal", "", "H3B 2Y5", "", "CA", "514-670-4321", "greg@ucme.inc", "");
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
    public void testRespectVerificationStatuses() throws TimeoutException {
        AutofillProfile profileWithDifferentStatuses = new AutofillProfile("" /* guid */,
                "" /* origin */, true,
                new ValueWithStatus("" /* honorific prefix */, VerificationStatus.NO_STATUS),
                new ValueWithStatus("John Smith", VerificationStatus.PARSED),
                new ValueWithStatus("" /* company */, VerificationStatus.NO_STATUS),
                new ValueWithStatus("1 Main\nApt A", VerificationStatus.FORMATTED),
                new ValueWithStatus("Quebec", VerificationStatus.OBSERVED),
                new ValueWithStatus("Montreal", VerificationStatus.USER_VERIFIED),
                new ValueWithStatus("" /* dependent locality */, VerificationStatus.NO_STATUS),
                new ValueWithStatus("H3B 2Y5", VerificationStatus.SERVER_PARSED),
                new ValueWithStatus("" /* sorting code */, VerificationStatus.NO_STATUS),
                new ValueWithStatus("Canada", VerificationStatus.USER_VERIFIED),
                new ValueWithStatus("" /* phone */, VerificationStatus.NO_STATUS),
                new ValueWithStatus("" /* email */, VerificationStatus.NO_STATUS),
                "" /* language code */);
        String guid = mHelper.setProfile(profileWithDifferentStatuses);
        Assert.assertEquals(1, mHelper.getNumberOfProfilesForSettings());

        AutofillProfile storedProfile = mHelper.getProfile(guid);
        // When converted to C++ and back the verification statuses for name and address components
        // should be preserved.
        Assert.assertEquals(VerificationStatus.PARSED, storedProfile.getFullNameStatus());
        Assert.assertEquals(VerificationStatus.FORMATTED, storedProfile.getStreetAddressStatus());
        Assert.assertEquals(VerificationStatus.OBSERVED, storedProfile.getRegionStatus());
        Assert.assertEquals(VerificationStatus.USER_VERIFIED, storedProfile.getLocalityStatus());
        Assert.assertEquals(VerificationStatus.SERVER_PARSED, storedProfile.getPostalCodeStatus());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testValuesSetInProfileGainUserVerifiedStatus() {
        AutofillProfile profile = new AutofillProfile();
        Assert.assertEquals(VerificationStatus.NO_STATUS, profile.getFullNameStatus());
        Assert.assertEquals(VerificationStatus.NO_STATUS, profile.getStreetAddressStatus());
        Assert.assertEquals(VerificationStatus.NO_STATUS, profile.getLocalityStatus());

        profile.setFullName("Homer Simpson");
        Assert.assertEquals(VerificationStatus.USER_VERIFIED, profile.getFullNameStatus());
        profile.setStreetAddress("123 Main St.");
        Assert.assertEquals(VerificationStatus.USER_VERIFIED, profile.getStreetAddressStatus());
        profile.setLocality("Springfield");
        Assert.assertEquals(VerificationStatus.USER_VERIFIED, profile.getLocalityStatus());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testMultilineStreetAddress() throws TimeoutException {
        final String streetAddress1 = "Chez Mireille COPEAU Appartment. 2\n"
                + "Entree A Batiment Jonquille\n"
                + "25 RUE DE L'EGLISE";
        final String streetAddress2 = streetAddress1 + "\n"
                + "Fourth floor\n"
                + "The red bell";
        AutofillProfile profile = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "Monsieur Jean DELHOURME", "Acme Inc.", streetAddress1,
                "Tahiti", "Mahina", "Orofara", "98709", "CEDEX 98703", "French Polynesia",
                "44.71.53", "john@acme.inc", "");
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
        AutofillProfile profile1 = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "John Major", "Acme Inc.", "123 Main", "California",
                "Los Angeles", "", "90210", "", "US", "555 123-4567", "jm@example.com", "");
        // An almost identical profile.
        AutofillProfile profile2 = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "John Major", "Acme Inc.", "123 Main", "California",
                "Los Angeles", "", "90210", "", "US", "555 123-4567", "jm-work@example.com", "");
        // A different profile.
        AutofillProfile profile3 = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "Jasper Lundgren", "", "1500 Second Ave", "California",
                "Hollywood", "", "90068", "", "US", "555 123-9876", "jasperl@example.com", "");
        // A profile where a lot of stuff is missing.
        AutofillProfile profile4 = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "Joe Sergeant", "", "", "Texas", "Fort Worth", "", "",
                "", "US", "", "", "");

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
        AutofillProfile profile1 = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "John Major", "Acme Inc.", "123 Main", "California",
                "Los Angeles", "", "90210", "", "US", "555 123-4567", "jm@example.com", "");
        AutofillProfile profile2 = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "John Major", "Acme Inc.", "123 Main", "California",
                "Los Angeles", "", "90210", "", "US", "555 123-4567", "jm-work@example.com", "");
        AutofillProfile profile3 = new AutofillProfile("" /* guid */, "" /* origin */,
                "" /* honorific prefix */, "Jasper Lundgren", "", "1500 Second Ave", "California",
                "Hollywood", "", "90068", "", "US", "555 123-9876", "jasperl@example.com", "");

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
        CreditCard card1 = createLocalCreditCard("Visa", "1234123412341234", "5", "2020");

        CreditCard card2 =
                createLocalCreditCard("American Express", "1234123412341234", "8", "2020");
        card2.setOrigin("http://www.example.com");

        CreditCard card3 = createLocalCreditCard("Mastercard", "1234123412341234", "11", "2020");
        card3.setOrigin("http://www.example.com");

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
    public void testCreditCardsDeduping() throws TimeoutException {
        // Create a local card and an identical server card.
        CreditCard card1 = new CreditCard("" /* guid */, "" /* origin */, true /* isLocal */,
                false /* isCached */, "John Doe", "1234123412341234", "", "5", "2020", "Visa",
                0 /* issuerIconDrawableId */, "" /* billingAddressId */, "" /* serverId */);

        CreditCard card2 = new CreditCard("" /* guid */, "" /* origin */, false /* isLocal */,
                false /* isCached */, "John Doe", "1234123412341234", "", "5", "2020", "Visa",
                0 /* issuerIconDrawableId */, "" /* billingAddressId */, "" /* serverId */);

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
    public void testCreditCardUseStatsSettingAndGetting() throws TimeoutException {
        String guid = mHelper.setCreditCard(new CreditCard("" /* guid */, "" /* origin */,
                true /* isLocal */, false /* isCached */, "John Doe", "1234123412341234", "", "5",
                "2020", "Visa", 0 /* issuerIconDrawableId */, "" /* billingAddressId */,
                "" /* serverId */));

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
    public void testRecordAndLogCreditCardUse() throws TimeoutException {
        String guid = mHelper.setCreditCard(new CreditCard("" /* guid */, "" /* origin */,
                true /* isLocal */, false /* isCached */, "John Doe", "1234123412341234", "", "5",
                "2020", "Visa", 0 /* issuerIconDrawableId */, "" /* billingAddressId */,
                "" /* serverId */));

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
    public void testGetProfilesToSuggest_WithName() throws TimeoutException {
        mHelper.setProfile(createTestProfile());

        List<AutofillProfile> profiles =
                mHelper.getProfilesToSuggest(true /* includeNameInLabel */);
        Assert.assertEquals("John Major, Acme Inc., 123 Main, Los Angeles, California 90210, "
                        + "United States",
                profiles.get(0).getLabel());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testClearAllData() throws TimeoutException {
        CreditCard localCard = new CreditCard("" /* guid */, "" /* origin */, true /* isLocal */,
                false /* isCached */, "John Doe", "1234123412341234", "", "5", "2020", "Visa",
                0 /* issuerIconDrawableId */, "" /* billingAddressId */, "" /* serverId */);
        CreditCard serverCard = new CreditCard("serverGuid" /* guid */, "" /* origin */,
                false /* isLocal */, false /* isCached */, "John Doe Server", "41111111111111111",
                "", "3", "2019", "Visa", 0 /* issuerIconDrawableId */, "" /* billingAddressId */,
                "serverId" /* serverId */);
        mHelper.addServerCreditCard(serverCard);
        Assert.assertEquals(1, mHelper.getNumberOfCreditCardsForSettings());

        // Clears all server data.
        mHelper.clearAllDataForTesting();
        Assert.assertEquals(0, mHelper.getNumberOfCreditCardsForSettings());

        mHelper.setProfile(createTestProfile());
        mHelper.setCreditCard(localCard);
        mHelper.addServerCreditCard(serverCard);
        Assert.assertEquals(1, mHelper.getNumberOfProfilesForSettings());
        Assert.assertEquals(2, mHelper.getNumberOfCreditCardsForSettings());

        // Clears all server and local data.
        mHelper.clearAllDataForTesting();
        Assert.assertEquals(0, mHelper.getNumberOfProfilesForSettings());
        Assert.assertEquals(0, mHelper.getNumberOfCreditCardsForSettings());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES)
    public void
    testGetCardIcon_customIconUrlAvailable_customIconCachedOnFirstCallAndReturnedOnSecondCall()
            throws TimeoutException {
        Context context = ContextUtils.getApplicationContext();

        int widthId = R.dimen.settings_page_card_icon_width;
        int width = context.getResources().getDimensionPixelSize(widthId);
        int heightId = R.dimen.settings_page_card_icon_height;
        int height = context.getResources().getDimensionPixelSize(heightId);
        int cornerRadiusId = R.dimen.card_art_corner_radius;
        float cornerRadius = context.getResources().getDimensionPixelSize(cornerRadiusId);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // The first call to get the custom icon only fetches and caches the icon. It returns
            // the default icon.
            assertTrue(
                    ((BitmapDrawable) AppCompatResources.getDrawable(context, R.drawable.mc_card))
                            .getBitmap()
                            .sameAs(((BitmapDrawable) AutofillUiUtils.getCardIcon(context,
                                             new GURL("http://google.com/test.png"),
                                             R.drawable.mc_card, widthId, heightId, cornerRadiusId,
                                             /* showCustomIcon= */ true))
                                            .getBitmap()));

            // The custom icon is already cached, and gets returned.
            assertTrue(AutofillUiUtils
                               .resizeAndAddRoundedCornersAndGreyBorder(TEST_CARD_ART_IMAGE, width,
                                       height, cornerRadius,
                                       /* addRoundedCornersAndGreyBorder= */ true)
                               .sameAs(((BitmapDrawable) AutofillUiUtils.getCardIcon(context,
                                                new GURL("http://google.com/test.png"),
                                                R.drawable.mc_card, widthId, heightId,
                                                cornerRadiusId, /* showCustomIcon= */ true))
                                               .getBitmap()));
        });
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testGetCardIcon_customIconUrlUnavailable_defaultIconReturned()
            throws TimeoutException {
        Context context = ContextUtils.getApplicationContext();
        int widthId = R.dimen.autofill_dropdown_icon_width;
        int heightId = R.dimen.autofill_dropdown_icon_height;
        int cornerRadiusId = R.dimen.card_art_corner_radius;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // In the absence of custom icon URL, the default icon is returned.
            assertTrue(
                    ((BitmapDrawable) AppCompatResources.getDrawable(context, R.drawable.mc_card))
                            .getBitmap()
                            .sameAs(((BitmapDrawable) AutofillUiUtils.getCardIcon(context,
                                             new GURL(""), R.drawable.mc_card, widthId, heightId,
                                             cornerRadiusId, true))
                                            .getBitmap()));

            // Calling it twice just to make sure that there is no caching behavior like it happens
            // in the case of custom icons.
            assertTrue(
                    ((BitmapDrawable) AppCompatResources.getDrawable(context, R.drawable.mc_card))
                            .getBitmap()
                            .sameAs(((BitmapDrawable) AutofillUiUtils.getCardIcon(context,
                                             new GURL(""), R.drawable.mc_card, widthId, heightId,
                                             cornerRadiusId, true))
                                            .getBitmap()));
        });
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testGetCardIcon_customIconUrlAndDefaultIconIdUnavailable_nothingReturned()
            throws TimeoutException {
        Context context = ContextUtils.getApplicationContext();
        int widthId = R.dimen.autofill_dropdown_icon_width;
        int heightId = R.dimen.autofill_dropdown_icon_height;
        int cornerRadiusId = R.dimen.card_art_corner_radius;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // If neither the custom icon nor the default icon is available, null is returned.
            assertEquals(null,
                    AutofillUiUtils.getCardIcon(
                            context, new GURL(""), 0, widthId, heightId, cornerRadiusId, true));
        });
    }
}
