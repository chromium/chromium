// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createLocalCreditCard;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.IbanRecordType;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.VerificationStatus;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.components.image_fetcher.test.TestImageFetcher;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for Chrome on Android's usage of the PersonalDataManager API. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PersonalDataManagerTest {
    private static final Bitmap TEST_CARD_ART_IMAGE =
            Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);
    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private AutofillTestHelper mHelper;

    @Before
    public void setUp() {
        mHelper = new AutofillTestHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AutofillTestHelper.getPersonalDataManagerForLastUsedProfile()
                                .setImageFetcherForTesting(
                                        new TestImageFetcher(TEST_CARD_ART_IMAGE)));
    }

    @After
    public void tearDown() throws TimeoutException {
        mHelper.clearAllDataForTesting();
    }

    private AutofillProfile createTestProfile() {
        return AutofillProfile.builder()
                .setFullName("John Major")
                .setCompanyName("Acme Inc.")
                .setStreetAddress("123 Main")
                .setRegion("California")
                .setLocality("Los Angeles")
                .setPostalCode("90210")
                .setCountryCode("US")
                .setPhoneNumber("555 123-4567")
                .setEmailAddress("jm@example.com")
                .build();
    }

    private static Matcher<Iban> ibanMatcher(
            final @IbanRecordType int recordType, final String nickname) {
        return new TypeSafeMatcher<Iban>() {
            @Override
            protected boolean matchesSafely(Iban iban) {
                return iban.getRecordType() == recordType && iban.getNickname().equals(nickname);
            }

            @Override
            public void describeTo(Description description) {
                description
                        .appendText("an Iban with recordType ")
                        .appendValue(recordType)
                        .appendText(" and nickname ")
                        .appendValue(nickname);
            }
        };
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndEditProfiles() throws TimeoutException {
        AutofillProfile profile =
                AutofillProfile.builder()
                        .setFullName("John Smith")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("1 Main\nApt A")
                        .setRegion("CA")
                        .setLocality("San Francisco")
                        .setPostalCode("94102")
                        .setCountryCode("US")
                        .setPhoneNumber("4158889999")
                        .setEmailAddress("john@acme.inc")
                        .build();
        String profileOneGUID = mHelper.setProfile(profile);
        Assert.assertEquals(1, mHelper.getNumberOfProfilesForSettings());

        AutofillProfile profile2 =
                AutofillProfile.builder()
                        .setFullName("John Hackock")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("1 Main\nApt A")
                        .setRegion("CA")
                        .setLocality("San Francisco")
                        .setPostalCode("94102")
                        .setCountryCode("US")
                        .setPhoneNumber("4158889999")
                        .setEmailAddress("john@acme.inc")
                        .build();
        String profileTwoGUID = mHelper.setProfile(profile2);
        Assert.assertEquals(2, mHelper.getNumberOfProfilesForSettings());

        profile.setGUID(profileOneGUID);
        profile.setCountryCode("CA");
        mHelper.setProfile(profile);
        Assert.assertEquals(
                "Should still have only two profiles", 2, mHelper.getNumberOfProfilesForSettings());

        AutofillProfile storedProfile = mHelper.getProfile(profileOneGUID);
        Assert.assertEquals(profileOneGUID, storedProfile.getGUID());
        Assert.assertEquals("CA", storedProfile.getCountryCode());
        Assert.assertEquals("San Francisco", storedProfile.getLocality());
        Assert.assertNotNull(mHelper.getProfile(profileTwoGUID));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testUpdateLanguageCodeInProfile() throws TimeoutException {
        AutofillProfile profile =
                AutofillProfile.builder()
                        .setFullName("John Smith")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("1 Main\nApt A")
                        .setRegion("CA")
                        .setLocality("San Francisco")
                        .setPostalCode("94102")
                        .setCountryCode("US")
                        .setPhoneNumber("4158889999")
                        .setEmailAddress("john@acme.inc")
                        .setLanguageCode("fr")
                        .build();
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
        CreditCard cardWithoutNickname =
                createLocalCreditCard(
                        "Visa", "1234123412341234", "5", AutofillTestHelper.nextYear());
        String nickname = "test nickname";
        CreditCard cardWithNickname =
                createLocalCreditCard(
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
    public void testAddCreditCardWithCardArtUrl_imageDownloaded() throws TimeoutException {
        AutofillUiUtils.CardIconSpecs cardIconSpecsLarge =
                AutofillUiUtils.CardIconSpecs.create(
                        ContextUtils.getApplicationContext(), ImageSize.LARGE);
        AutofillUiUtils.CardIconSpecs cardIconSpecsSmall =
                AutofillUiUtils.CardIconSpecs.create(
                        ContextUtils.getApplicationContext(), ImageSize.LARGE);
        GURL cardArtUrl = new GURL("http://google.com/test.png");
        CreditCard cardWithCardArtUrl =
                new CreditCard(
                        /* guid= */ "serverGuid",
                        /* origin= */ "",
                        /* isLocal= */ false,
                        "John Doe Server",
                        "41111111111111111",
                        /* obfuscatedCardNumber= */ "",
                        "3",
                        "2019",
                        "Visa",
                        /* issuerIconDrawableId= */ 0,
                        /* billingAddressId= */ "",
                        /* serverId= */ "serverId");
        cardWithCardArtUrl.setCardArtUrl(cardArtUrl);

        // Adding a server card triggers card art image fetching for all server credit cards.
        mHelper.addServerCreditCard(cardWithCardArtUrl);

        // Verify card art images are fetched in both small and large sizes.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                                            TEST_CARD_ART_IMAGE,
                                            cardIconSpecsLarge,
                                            /* addRoundedCornersAndGreyBorder= */ ChromeFeatureList
                                                    .isEnabled(
                                                            ChromeFeatureList
                                                                    .AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES))
                                    .sameAs(
                                            AutofillTestHelper
                                                    .getPersonalDataManagerForLastUsedProfile()
                                                    .getCustomImageForAutofillSuggestionIfAvailable(
                                                            cardArtUrl, cardIconSpecsLarge)
                                                    .get()));
                    assertTrue(
                            AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                                            TEST_CARD_ART_IMAGE,
                                            cardIconSpecsSmall,
                                            /* addRoundedCornersAndGreyBorder= */ ChromeFeatureList
                                                    .isEnabled(
                                                            ChromeFeatureList
                                                                    .AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES))
                                    .sameAs(
                                            AutofillTestHelper
                                                    .getPersonalDataManagerForLastUsedProfile()
                                                    .getCustomImageForAutofillSuggestionIfAvailable(
                                                            cardArtUrl, cardIconSpecsSmall)
                                                    .get()));
                });
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_CARD_ART_SERVER_SIDE_STRETCHING)
    public void testCreditCardArtUrlIsFormattedWithImageSpecs_serverSideStretchingDisabled()
            throws TimeoutException {
        GURL capitalOneIconUrl = new GURL(AutofillUiUtils.CAPITAL_ONE_ICON_URL);
        GURL cardArtUrl = new GURL("http://google.com/test");
        int widthPixels = 32;
        int heightPixels = 20;

        // The URL should be updated as `cardArtUrl=w{width}-h{height}`.
        assertThat(
                        AutofillUiUtils.getCreditCardIconUrlWithParams(
                                capitalOneIconUrl, widthPixels, heightPixels))
                .isEqualTo(
                        new GURL(
                                capitalOneIconUrl.getSpec()
                                        + "=w"
                                        + widthPixels
                                        + "-h"
                                        + heightPixels));
        assertThat(
                        AutofillUiUtils.getCreditCardIconUrlWithParams(
                                cardArtUrl, widthPixels, heightPixels))
                .isEqualTo(
                        new GURL(cardArtUrl.getSpec() + "=w" + widthPixels + "-h" + heightPixels));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_CARD_ART_SERVER_SIDE_STRETCHING)
    public void testCreditCardArtUrlIsFormattedWithImageSpecs_serverSideStretchingEnabled()
            throws TimeoutException {
        GURL capitalOneIconUrl = new GURL(AutofillUiUtils.CAPITAL_ONE_ICON_URL);
        GURL cardArtUrl = new GURL("http://google.com/test");
        int widthPixels = 32;
        int heightPixels = 20;

        // The URL should be updated as `cardArtUrl=w{width}-h{height}-s`.
        assertThat(
                        AutofillUiUtils.getCreditCardIconUrlWithParams(
                                capitalOneIconUrl, widthPixels, heightPixels))
                .isEqualTo(
                        new GURL(
                                capitalOneIconUrl.getSpec()
                                        + "=w"
                                        + widthPixels
                                        + "-h"
                                        + heightPixels
                                        + "-s"));
        assertThat(
                        AutofillUiUtils.getCreditCardIconUrlWithParams(
                                cardArtUrl, widthPixels, heightPixels))
                .isEqualTo(
                        new GURL(
                                cardArtUrl.getSpec()
                                        + "=w"
                                        + widthPixels
                                        + "-h"
                                        + heightPixels
                                        + "-s"));
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
        AutofillProfile profile1 =
                AutofillProfile.builder()
                        .setFullName("John Smith")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("1 Main\nApt A")
                        .setRegion("Quebec")
                        .setLocality("Montreal")
                        .setPostalCode("H3B 2Y5")
                        .setCountryCode("Canada")
                        .setPhoneNumber("514-670-1234")
                        .setEmailAddress("john@acme.inc")
                        .build();
        String profileGuid1 = mHelper.setProfile(profile1);

        AutofillProfile profile2 =
                AutofillProfile.builder()
                        .setFullName("Greg Smith")
                        .setCompanyName("Ucme Inc.")
                        .setStreetAddress("123 Bush\nApt 125")
                        .setRegion("Quebec")
                        .setLocality("Montreal")
                        .setPostalCode("H3B 2Y5")
                        .setCountryCode("CA")
                        .setPhoneNumber("514-670-4321")
                        .setEmailAddress("greg@ucme.inc")
                        .build();
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
        AutofillProfile profileWithDifferentStatuses =
                AutofillProfile.builder()
                        .setGUID("")
                        .setFullName("John Smith", VerificationStatus.PARSED)
                        .setCompanyName("", VerificationStatus.NO_STATUS)
                        .setStreetAddress("1 Main\nApt A", VerificationStatus.FORMATTED)
                        .setRegion("Quebec", VerificationStatus.OBSERVED)
                        .setLocality("Montreal", VerificationStatus.USER_VERIFIED)
                        .setDependentLocality("", VerificationStatus.NO_STATUS)
                        .setPostalCode("H3B 2Y5", VerificationStatus.SERVER_PARSED)
                        .setSortingCode("", VerificationStatus.NO_STATUS)
                        .setCountryCode("Canada", VerificationStatus.USER_VERIFIED)
                        .setPhoneNumber("", VerificationStatus.NO_STATUS)
                        .setEmailAddress(/* email= */ "", VerificationStatus.NO_STATUS)
                        .setLanguageCode("")
                        .build();
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
        AutofillProfile profile = AutofillProfile.builder().build();
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
        final String streetAddress1 =
                "Chez Mireille COPEAU Appartment. 2\n"
                        + "Entree A Batiment Jonquille\n"
                        + "25 RUE DE L'EGLISE";
        final String streetAddress2 = streetAddress1 + "\n" + "Fourth floor\n" + "The red bell";
        AutofillProfile profile =
                AutofillProfile.builder()
                        .setFullName("Monsieur Jean DELHOURME")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress(streetAddress1)
                        .setRegion("Tahiti")
                        .setLocality("Mahina")
                        .setDependentLocality("Orofara")
                        .setPostalCode("98709")
                        .setSortingCode("CEDEX 98703")
                        .setCountryCode("French Polynesia")
                        .setPhoneNumber("44.71.53")
                        .setEmailAddress("john@acme.inc")
                        .build();
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
        AutofillProfile profile1 =
                AutofillProfile.builder()
                        .setFullName("John Major")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .setPhoneNumber("555 123-4567")
                        .setEmailAddress("jm@example.com")
                        .build();
        // An almost identical profile.
        AutofillProfile profile2 =
                AutofillProfile.builder()
                        .setFullName("John Major")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .setPhoneNumber("555 123-4567")
                        .setEmailAddress("jm-work@example.com")
                        .build();
        // A different profile.
        AutofillProfile profile3 =
                AutofillProfile.builder()
                        .setFullName("Jasper Lundgren")
                        .setStreetAddress("1500 Second Ave")
                        .setRegion("California")
                        .setLocality("Hollywood")
                        .setPostalCode("90068")
                        .setCountryCode("US")
                        .setPhoneNumber("555 123-9876")
                        .setEmailAddress("jasperl@example.com")
                        .build();
        // A profile where a lot of stuff is missing.
        AutofillProfile profile4 =
                AutofillProfile.builder()
                        .setFullName("Joe Sergeant")
                        .setRegion("Texas")
                        .setLocality("Fort Worth")
                        .setCountryCode("US")
                        .build();

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
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_RANKING_FORMULA_ADDRESS_PROFILES)
    public void testProfilesFrecency() throws TimeoutException {
        // Create 3 profiles.
        AutofillProfile profile1 =
                AutofillProfile.builder()
                        .setFullName("John Major")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .setPhoneNumber("555 123-4567")
                        .setEmailAddress("jm@example.com")
                        .build();
        AutofillProfile profile2 =
                AutofillProfile.builder()
                        .setFullName("John Major")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .setPhoneNumber("555 123-4567")
                        .setEmailAddress("jm-work@example.com")
                        .build();
        AutofillProfile profile3 =
                AutofillProfile.builder()
                        .setFullName("Jasper Lundgren")
                        .setStreetAddress("1500 Second Ave")
                        .setRegion("California")
                        .setLocality("Hollywood")
                        .setPostalCode("90068")
                        .setCountryCode("US")
                        .setPhoneNumber("555 123-9876")
                        .setEmailAddress("jasperl@example.com")
                        .build();

        String guid1 = mHelper.setProfile(profile1);
        String guid2 = mHelper.setProfile(profile2);
        String guid3 = mHelper.setProfile(profile3);

        // The first profile has the lowest use count but has most recently been used, making it
        // ranked first.
        mHelper.setProfileUseStatsForTesting(guid1, 6, 1);
        // The second profile has the median use count and use date, and with these values it is
        // ranked third.
        mHelper.setProfileUseStatsForTesting(guid2, 25, 10);
        // The third profile has the highest use count and is the profile with the farthest last
        // use date. Because of its very high use count, it is still ranked second.
        mHelper.setProfileUseStatsForTesting(guid3, 100, 20);

        List<AutofillProfile> profiles =
                mHelper.getProfilesToSuggest(/* includeNameInLabel= */ false);
        Assert.assertEquals(3, profiles.size());
        Assert.assertTrue(
                "Profile1 should be ranked first", guid1.equals(profiles.get(0).getGUID()));
        Assert.assertTrue(
                "Profile3 should be ranked second", guid3.equals(profiles.get(1).getGUID()));
        Assert.assertTrue(
                "Profile2 should be ranked third", guid2.equals(profiles.get(2).getGUID()));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_RANKING_FORMULA_CREDIT_CARDS)
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

        // The first credit card has the lowest use count but has most recently been used, making it
        // ranked first.
        mHelper.setCreditCardUseStatsForTesting(guid1, 6, 1);
        // The second credit card has the median use count and use date, and with these values it is
        // ranked third.
        mHelper.setCreditCardUseStatsForTesting(guid2, 25, 10);
        // The third credit card has the highest use count and is the credit card with the farthest
        // last use date. Because of its very high use count, it is still ranked second.
        mHelper.setCreditCardUseStatsForTesting(guid3, 100, 20);

        List<CreditCard> cards = mHelper.getCreditCardsToSuggest();
        Assert.assertEquals(3, cards.size());
        Assert.assertTrue("Card1 should be ranked first", guid1.equals(cards.get(0).getGUID()));
        Assert.assertTrue("Card3 should be ranked second", guid3.equals(cards.get(1).getGUID()));
        Assert.assertTrue("Card2 should be ranked third", guid2.equals(cards.get(2).getGUID()));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_RANKING_FORMULA_ADDRESS_PROFILES)
    public void testProfileRanking() throws TimeoutException {
        // Create 3 profiles.
        AutofillProfile profile1 = AutofillProfile.builder().setFullName("John Major").build();
        AutofillProfile profile2 = AutofillProfile.builder().setFullName("Josh Larkin").build();
        AutofillProfile profile3 = AutofillProfile.builder().setFullName("Jasper Lundgren").build();

        String guid1 = mHelper.setProfile(profile1);
        String guid2 = mHelper.setProfile(profile2);
        String guid3 = mHelper.setProfile(profile3);

        // The first profile has the lowest use count but has most recently been used, making it
        // ranked second.
        mHelper.setProfileUseStatsForTesting(guid1, 6, 1);
        // The second profile has the median use count and use date, and with these values it is
        // ranked first.
        mHelper.setProfileUseStatsForTesting(guid2, 25, 10);
        // The third profile has the highest use count and is the profile with the farthest last
        // use date. Because of its very far last use date, it's ranked third.
        mHelper.setProfileUseStatsForTesting(guid3, 100, 20);

        List<AutofillProfile> profiles =
                mHelper.getProfilesToSuggest(/* includeNameInLabel= */ false);
        Assert.assertEquals(3, profiles.size());
        Assert.assertTrue(
                "Profile2 should be ranked first", guid2.equals(profiles.get(0).getGUID()));
        Assert.assertTrue(
                "Profile1 should be ranked second", guid1.equals(profiles.get(1).getGUID()));
        Assert.assertTrue(
                "Profile3 should be ranked third", guid3.equals(profiles.get(2).getGUID()));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_RANKING_FORMULA_CREDIT_CARDS)
    public void testCreditCardRanking() throws TimeoutException {
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

        // The first credit card has the lowest use count but has most recently been used, making it
        // ranked second.
        mHelper.setCreditCardUseStatsForTesting(guid1, 6, 1);
        // The second credit card has the median use count and use date, and with these values it is
        // ranked first.
        mHelper.setCreditCardUseStatsForTesting(guid2, 25, 10);
        // The third credit card has the highest use count and is the profile with the farthest last
        // use date. Because of its very far last use date, it's ranked third.
        mHelper.setCreditCardUseStatsForTesting(guid3, 100, 20);

        List<CreditCard> cards = mHelper.getCreditCardsToSuggest();
        Assert.assertEquals(3, cards.size());
        Assert.assertTrue("Card2 should be ranked first", guid2.equals(cards.get(0).getGUID()));
        Assert.assertTrue("Card1 should be ranked second", guid1.equals(cards.get(1).getGUID()));
        Assert.assertTrue("Card3 should be ranked third", guid3.equals(cards.get(2).getGUID()));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testCreditCardsDeduping() throws TimeoutException {
        // Create a local card and an identical server card.
        CreditCard card1 =
                new CreditCard(
                        /* guid= */ "",
                        /* origin= */ "",
                        /* isLocal= */ true,
                        "John Doe",
                        "1234123412341234",
                        "",
                        "5",
                        "2020",
                        "Visa",
                        /* issuerIconDrawableId= */ 0,
                        /* billingAddressId= */ "",
                        /* serverId= */ "");

        CreditCard card2 =
                new CreditCard(
                        /* guid= */ "",
                        /* origin= */ "",
                        /* isLocal= */ false,
                        "John Doe",
                        "1234123412341234",
                        "",
                        "5",
                        "2020",
                        "Visa",
                        /* issuerIconDrawableId= */ 0,
                        /* billingAddressId= */ "",
                        /* serverId= */ "");

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
        Assert.assertEquals(
                mHelper.getDateNDaysAgoForTesting(1234), mHelper.getProfileUseDateForTesting(guid));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testCreditCardUseStatsSettingAndGetting() throws TimeoutException {
        String guid =
                mHelper.setCreditCard(
                        new CreditCard(
                                /* guid= */ "",
                                /* origin= */ "",
                                /* isLocal= */ true,
                                "John Doe",
                                "1234123412341234",
                                "",
                                "5",
                                "2020",
                                "Visa",
                                /* issuerIconDrawableId= */ 0,
                                /* billingAddressId= */ "",
                                /* serverId= */ ""));

        // Make sure the credit card does not have the specific use stats form the start.
        Assert.assertTrue(1234 != mHelper.getCreditCardUseCountForTesting(guid));
        Assert.assertTrue(1234 != mHelper.getCreditCardUseDateForTesting(guid));

        // Set specific use stats for the credit card.
        mHelper.setCreditCardUseStatsForTesting(guid, 1234, 1234);

        // Make sure the specific use stats were set for the credit card.
        Assert.assertEquals(1234, mHelper.getCreditCardUseCountForTesting(guid));
        Assert.assertEquals(
                mHelper.getDateNDaysAgoForTesting(1234),
                mHelper.getCreditCardUseDateForTesting(guid));
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
        String guid =
                mHelper.setCreditCard(
                        new CreditCard(
                                /* guid= */ "",
                                /* origin= */ "",
                                /* isLocal= */ true,
                                "John Doe",
                                "1234123412341234",
                                "",
                                "5",
                                "2020",
                                "Visa",
                                /* issuerIconDrawableId= */ 0,
                                /* billingAddressId= */ "",
                                /* serverId= */ ""));

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
                mHelper.getProfilesToSuggest(/* includeNameInLabel= */ false);
        Assert.assertEquals(
                "Acme Inc., 123 Main, Los Angeles, California 90210, United States",
                profiles.get(0).getLabel());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testGetProfilesToSuggest_WithName() throws TimeoutException {
        mHelper.setProfile(createTestProfile());

        List<AutofillProfile> profiles =
                mHelper.getProfilesToSuggest(/* includeNameInLabel= */ true);
        Assert.assertEquals(
                "John Major, Acme Inc., 123 Main, Los Angeles, California 90210, "
                        + "United States",
                profiles.get(0).getLabel());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testClearAllData() throws TimeoutException {
        CreditCard localCard =
                new CreditCard(
                        /* guid= */ "",
                        /* origin= */ "",
                        /* isLocal= */ true,
                        "John Doe",
                        "1234123412341234",
                        "",
                        "5",
                        "2020",
                        "Visa",
                        /* issuerIconDrawableId= */ 0,
                        /* billingAddressId= */ "",
                        /* serverId= */ "");
        CreditCard serverCard =
                new CreditCard(
                        /* guid= */ "serverGuid",
                        /* origin= */ "",
                        /* isLocal= */ false,
                        "John Doe Server",
                        "41111111111111111",
                        "",
                        "3",
                        "2019",
                        "Visa",
                        /* issuerIconDrawableId= */ 0,
                        /* billingAddressId= */ "",
                        /* serverId= */ "serverId");
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
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES)
    public void testGetCardIcon_customIconUrlAvailable_customIconReturned()
            throws TimeoutException {
        Context context = ContextUtils.getApplicationContext();
        AutofillUiUtils.CardIconSpecs cardIconSpecs =
                AutofillUiUtils.CardIconSpecs.create(context, ImageSize.LARGE);
        GURL cardArtUrl = new GURL("http://google.com/test.png");
        CreditCard cardWithCardArtUrl =
                new CreditCard(
                        /* guid= */ "serverGuid",
                        /* origin= */ "",
                        /* isLocal= */ false,
                        "John Doe Server",
                        "41111111111111111",
                        /* obfuscatedCardNumber= */ "",
                        "3",
                        "2019",
                        "MasterCard",
                        /* issuerIconDrawableId= */ R.drawable.mc_card,
                        /* billingAddressId= */ "",
                        /* serverId= */ "serverId");
        cardWithCardArtUrl.setCardArtUrl(cardArtUrl);

        // Adding a server card triggers card art image fetching for all server credit cards.
        mHelper.addServerCreditCard(cardWithCardArtUrl);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // The custom icon is already cached, and gets returned.
                    assertTrue(
                            AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                                            TEST_CARD_ART_IMAGE,
                                            cardIconSpecs,
                                            /* addRoundedCornersAndGreyBorder= */ true)
                                    .sameAs(
                                            ((BitmapDrawable)
                                                            AutofillUiUtils.getCardIcon(
                                                                    context,
                                                                    AutofillTestHelper
                                                                            .getPersonalDataManagerForLastUsedProfile(),
                                                                    new GURL(
                                                                            "http://google.com/test.png"),
                                                                    R.drawable.mc_card,
                                                                    ImageSize.LARGE,
                                                                    /* showCustomIcon= */ true))
                                                    .getBitmap()));
                });
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testGetCardIcon_customIconUrlUnavailable_defaultIconReturned()
            throws TimeoutException {
        Context context = ContextUtils.getApplicationContext();
        CreditCard cardWithoutCardArtUrl =
                new CreditCard(
                        /* guid= */ "serverGuid",
                        /* origin= */ "",
                        /* isLocal= */ false,
                        "John Doe Server",
                        "41111111111111111",
                        /* obfuscatedCardNumber= */ "",
                        "3",
                        "2019",
                        "MasterCard",
                        /* issuerIconDrawableId= */ R.drawable.mc_card,
                        /* billingAddressId= */ "",
                        /* serverId= */ "serverId");

        // Adding a server card triggers card art image fetching for all server credit cards.
        mHelper.addServerCreditCard(cardWithoutCardArtUrl);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // In the absence of custom icon URL, the default icon is returned.
                    assertTrue(
                            ((BitmapDrawable)
                                            AppCompatResources.getDrawable(
                                                    context, R.drawable.mc_card))
                                    .getBitmap()
                                    .sameAs(
                                            ((BitmapDrawable)
                                                            AutofillUiUtils.getCardIcon(
                                                                    context,
                                                                    AutofillTestHelper
                                                                            .getPersonalDataManagerForLastUsedProfile(),
                                                                    new GURL(""),
                                                                    R.drawable.mc_card,
                                                                    ImageSize.LARGE,
                                                                    true))
                                                    .getBitmap()));
                });
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testGetCardIcon_customIconUrlAndDefaultIconIdUnavailable_nothingReturned()
            throws TimeoutException {
        CreditCard cardWithoutDefaultIconIdAndCardArtUrl =
                new CreditCard(
                        /* guid= */ "serverGuid",
                        /* origin= */ "",
                        /* isLocal= */ false,
                        "John Doe Server",
                        "41111111111111111",
                        /* obfuscatedCardNumber= */ "",
                        "3",
                        "2019",
                        "",
                        /* issuerIconDrawableId= */ 0,
                        /* billingAddressId= */ "",
                        /* serverId= */ "serverId");

        // Adding a server card triggers card art image fetching for all server credit cards.
        mHelper.addServerCreditCard(cardWithoutDefaultIconIdAndCardArtUrl);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // If neither the custom icon nor the default icon is available, null is
                    // returned.
                    assertEquals(
                            null,
                            AutofillUiUtils.getCardIcon(
                                    ContextUtils.getApplicationContext(),
                                    AutofillTestHelper.getPersonalDataManagerForLastUsedProfile(),
                                    new GURL(""),
                                    0,
                                    ImageSize.LARGE,
                                    true));
                });
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void
            testGetCustomImageForAutofillSuggestionIfAvailable_recordImageFetchingResult_success()
                    throws TimeoutException {
        GURL cardArtUrl = new GURL("http://google.com/test.png");
        AutofillUiUtils.CardIconSpecs cardIconSpecs =
                AutofillUiUtils.CardIconSpecs.create(
                        ContextUtils.getApplicationContext(), ImageSize.LARGE);

        HistogramWatcher expectedHistogram =
                HistogramWatcher.newSingleRecordWatcher("Autofill.ImageFetcher.Result", true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillTestHelper.getPersonalDataManagerForLastUsedProfile()
                            .getCustomImageForAutofillSuggestionIfAvailable(
                                    cardArtUrl, cardIconSpecs);
                    expectedHistogram.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void
            testGetCustomImageForAutofillSuggestionIfAvailable_recordImageFetchingResult_failure()
                    throws TimeoutException {
        GURL cardArtUrl = new GURL("http://google.com/test.png");
        AutofillUiUtils.CardIconSpecs cardIconSpecs =
                AutofillUiUtils.CardIconSpecs.create(
                        ContextUtils.getApplicationContext(), ImageSize.LARGE);

        HistogramWatcher expectedHistogram =
                HistogramWatcher.newSingleRecordWatcher("Autofill.ImageFetcher.Result", false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillTestHelper.getPersonalDataManagerForLastUsedProfile()
                            .setImageFetcherForTesting(new TestImageFetcher(null));
                    AutofillTestHelper.getPersonalDataManagerForLastUsedProfile()
                            .getCustomImageForAutofillSuggestionIfAvailable(
                                    cardArtUrl, cardIconSpecs);
                    expectedHistogram.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testAddIban() throws TimeoutException {
        Iban iban =
                new Iban.Builder()
                        .setLabel("")
                        .setNickname("My IBAN")
                        .setRecordType(IbanRecordType.UNKNOWN)
                        .setValue("FR76 3000 6000 0112 3456 7890 189")
                        .build();
        String ibanGuid = mHelper.addOrUpdateLocalIban(iban);

        Iban storedLocalIban = mHelper.getIban(ibanGuid);
        Assert.assertEquals("My IBAN", storedLocalIban.getNickname());
        Assert.assertEquals("FR7630006000011234567890189", storedLocalIban.getValue());
        Assert.assertEquals(IbanRecordType.LOCAL_IBAN, storedLocalIban.getRecordType());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndEditIban() throws TimeoutException {
        // Test "add IBAN" workflow.
        Iban iban =
                new Iban.Builder()
                        .setLabel("")
                        .setNickname("My IBAN")
                        .setRecordType(IbanRecordType.UNKNOWN)
                        .setValue("FR76 3000 6000 0112 3456 7890 189")
                        .build();
        String ibanGuid = mHelper.addOrUpdateLocalIban(iban);

        Iban storedLocalIban = mHelper.getIban(ibanGuid);
        Assert.assertEquals("My IBAN", storedLocalIban.getNickname());
        Assert.assertEquals("FR7630006000011234567890189", storedLocalIban.getValue());
        Assert.assertEquals(IbanRecordType.LOCAL_IBAN, storedLocalIban.getRecordType());

        // Test "edit IBAN" workflow.
        storedLocalIban.updateNickname("My alternative IBAN");
        storedLocalIban.updateValue("DE91 1000 0000 0123 4567 89");
        mHelper.addOrUpdateLocalIban(storedLocalIban);

        Iban updatedLocalIban = mHelper.getIban(ibanGuid);
        Assert.assertEquals("My alternative IBAN", updatedLocalIban.getNickname());
        Assert.assertEquals("DE91100000000123456789", updatedLocalIban.getValue());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testAddingServerIban() {
        Iban.Builder ibanBuilder =
                new Iban.Builder()
                        .setInstrumentId(123456L)
                        .setLabel("CH •••8009")
                        .setNickname("My IBAN")
                        .setRecordType(IbanRecordType.SERVER_IBAN)
                        .setValue("");

        ibanBuilder.build();
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testGetIbanLabelReturnsObfuscatedIbanValue() throws TimeoutException {
        Iban iban =
                new Iban.Builder()
                        .setLabel("")
                        .setNickname("My IBAN")
                        .setRecordType(IbanRecordType.UNKNOWN)
                        .setValue("CH56 0483 5012 3456 7800 9")
                        .build();
        String ibanGuid = mHelper.addOrUpdateLocalIban(iban);

        Iban storedLocalIban = mHelper.getIban(ibanGuid);
        String dot = "\u2022";
        // \u2022 is Bullet and \u2006 is SIX-PER-EM SPACE (small space between
        // bullets). The expected string is 'CH •••8009'.
        Assert.assertEquals("CH" + "\u2006" + dot.repeat(2) + "8009", storedLocalIban.getLabel());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testGetIbansForSettings() throws TimeoutException {
        Iban ibanOne =
                new Iban.Builder()
                        .setLabel("")
                        .setNickname("My local IBAN")
                        .setRecordType(IbanRecordType.UNKNOWN)
                        .setValue("CH56 0483 5012 3456 7800 9")
                        .build();
        Iban ibanTwo =
                Iban.createServer(
                        /* instrumentId= */ 100L,
                        /* label= */ "CH •••8009",
                        /* nickname= */ "My server IBAN",
                        /* value= */ "");

        mHelper.addOrUpdateLocalIban(ibanOne);
        mHelper.addServerIban(ibanTwo);

        Iban[] actualIbans = mHelper.getIbansForSettings();

        assertThat(
                Arrays.asList(actualIbans),
                containsInAnyOrder(
                        ibanMatcher(IbanRecordType.LOCAL_IBAN, "My local IBAN"),
                        ibanMatcher(IbanRecordType.SERVER_IBAN, "My server IBAN")));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testGetMaskedBankAccounts() throws TimeoutException {
        BankAccount bankAccount1 =
                new BankAccount.Builder()
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setSupportedPaymentRails(new int[] {1})
                                        .build())
                        .setBankName("bank name")
                        .build();
        BankAccount bankAccount2 =
                new BankAccount.Builder()
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(200)
                                        .setNickname("nickname2")
                                        .setSupportedPaymentRails(new int[] {1})
                                        .setDisplayIconUrl(new GURL("http://example.com"))
                                        .build())
                        .setBankName("bank name 2")
                        .build();
        AutofillTestHelper.addMaskedBankAccount(bankAccount1);
        AutofillTestHelper.addMaskedBankAccount(bankAccount2);

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertThat(new BankAccount[] {bankAccount1, bankAccount2})
                                .isEqualTo(
                                        AutofillTestHelper
                                                .getPersonalDataManagerForLastUsedProfile()
                                                .getMaskedBankAccounts()));
    }
}
