// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createLocalCreditCard;

import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.IbanRecordType;
import org.chromium.components.autofill.VerificationStatus;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for Chrome on Android's usage of the PersonalDataManager API. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PersonalDataManagerTest {
    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private AutofillTestHelper mHelper;

    @Before
    public void setUp() {
        mHelper = new AutofillTestHelper();
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
        return new TypeSafeMatcher<>() {
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
        assertEquals(1, mHelper.getNumberOfProfilesForSettings());
        assertEquals(
                "First name should be set",
                "John",
                mHelper.getProfile(profileOneGUID).getInfo(FieldType.NAME_FIRST));

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
        assertEquals(2, mHelper.getNumberOfProfilesForSettings());

        profile.setGUID(profileOneGUID);
        profile.setCountryCode("CA");
        profile.setFullName("");
        mHelper.setProfile(profile);
        assertEquals(
                "Should still have only two profiles", 2, mHelper.getNumberOfProfilesForSettings());

        AutofillProfile storedProfile = mHelper.getProfile(profileOneGUID);
        assertEquals(profileOneGUID, storedProfile.getGUID());

        // Name full and its children should be cleared.
        assertEquals("", storedProfile.getInfo(FieldType.NAME_FULL));
        assertEquals("", storedProfile.getInfo(FieldType.NAME_FIRST));

        assertEquals("CA", storedProfile.getInfo(FieldType.ADDRESS_HOME_COUNTRY));
        assertEquals("San Francisco", storedProfile.getInfo(FieldType.ADDRESS_HOME_CITY));
        assertNotNull(mHelper.getProfile(profileTwoGUID));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testRecordSeparatorMetricForAddAndEditProfiles() throws TimeoutException {
        AutofillProfile profile =
                AutofillProfile.builder()
                        .setFullName("山本 葵")
                        .setAlternativeFullName("ヤマモト・アオイ")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("1 Main\nApt A")
                        .setRegion("Tokyo")
                        .setLocality("Tokyo")
                        .setPostalCode("94102")
                        .setCountryCode("JP")
                        .setPhoneNumber("4158889999")
                        .setEmailAddress("aoi_yamamoto@acme.inc")
                        .build();

        // Expect histogram to record separator existence in alternative name.
        HistogramWatcher recordSeparatorCountHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Autofill.Settings.EditedAlternativeNameContainsASeparator", true)
                        .build();

        String profileOneGUID = mHelper.setProfile(profile);
        recordSeparatorCountHistogram.assertExpected();
        assertEquals(1, mHelper.getNumberOfProfilesForSettings());

        // Expect histogram to record no separator existence in alternative name.
        recordSeparatorCountHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Autofill.Settings.EditedAlternativeNameContainsASeparator", false)
                        .build();

        profile.setGUID(profileOneGUID);
        profile.setAlternativeFullName("JamesBond");
        profileOneGUID = mHelper.setProfile(profile);

        recordSeparatorCountHistogram.assertExpected();

        // Expect histogram to record separator existence in alternative name again.
        recordSeparatorCountHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Autofill.Settings.EditedAlternativeNameContainsASeparator", true)
                        .build();

        profile.setAlternativeFullName("James NonBond");
        profileOneGUID = mHelper.setProfile(profile);

        recordSeparatorCountHistogram.assertExpected();

        // Expect histogram to not record anything.
        recordSeparatorCountHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Autofill.Settings.EditedAlternativeNameContainsASeparator")
                        .build();

        profile.setAlternativeFullName("");
        profileOneGUID = mHelper.setProfile(profile);

        recordSeparatorCountHistogram.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testRecordSeparatorMetricForAddAndEditProfilesForHiragana()
            throws TimeoutException {
        AutofillProfile profile =
                AutofillProfile.builder()
                        .setFullName("山本 葵")
                        .setAlternativeFullName("やまもと·あおい")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("1 Main\nApt A")
                        .setRegion("Tokyo")
                        .setLocality("Tokyo")
                        .setPostalCode("94102")
                        .setCountryCode("JP")
                        .setPhoneNumber("4158889999")
                        .setEmailAddress("aoi_yamamoto@acme.inc")
                        .build();

        // Expect histogram to record separator existence in alternative name.
        HistogramWatcher recordSeparatorCountHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Autofill.Settings.EditedAlternativeNameContainsASeparator", true)
                        .build();

        String profileOneGUID = mHelper.setProfile(profile);
        recordSeparatorCountHistogram.assertExpected();
        assertEquals(1, mHelper.getNumberOfProfilesForSettings());

        // Expect histogram to record no separator existence in alternative name.
        recordSeparatorCountHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Autofill.Settings.EditedAlternativeNameContainsASeparator", false)
                        .build();

        profile.setGUID(profileOneGUID);
        profile.setAlternativeFullName("やまもとあおい");
        profileOneGUID = mHelper.setProfile(profile);

        recordSeparatorCountHistogram.assertExpected();
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
        assertEquals("fr", profile.getLanguageCode());
        String profileOneGUID = mHelper.setProfile(profile);
        assertEquals(1, mHelper.getNumberOfProfilesForSettings());

        AutofillProfile storedProfile = mHelper.getProfile(profileOneGUID);
        assertEquals(profileOneGUID, storedProfile.getGUID());
        assertEquals("fr", storedProfile.getLanguageCode());
        assertEquals("US", storedProfile.getInfo(FieldType.ADDRESS_HOME_COUNTRY));

        profile.setGUID(profileOneGUID);
        profile.setLanguageCode("en");
        mHelper.setProfile(profile);

        AutofillProfile storedProfile2 = mHelper.getProfile(profileOneGUID);
        assertEquals(profileOneGUID, storedProfile2.getGUID());
        assertEquals("en", storedProfile2.getLanguageCode());
        assertEquals("US", storedProfile2.getInfo(FieldType.ADDRESS_HOME_COUNTRY));
        assertEquals("San Francisco", storedProfile2.getInfo(FieldType.ADDRESS_HOME_CITY));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndDeleteProfile() throws TimeoutException {
        String profileOneGUID = mHelper.setProfile(createTestProfile());
        assertEquals(1, mHelper.getNumberOfProfilesForSettings());

        mHelper.deleteProfile(profileOneGUID);
        assertEquals(0, mHelper.getNumberOfProfilesForSettings());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndEditCreditCards() throws TimeoutException {
        CreditCard card = createLocalCreditCard("Visa", "1234123412341234", "5", "2020");
        String cardOneGUID = mHelper.setCreditCard(card);
        assertEquals(1, mHelper.getNumberOfCreditCardsForSettings());

        CreditCard card2 =
                createLocalCreditCard("American Express", "1234123412341234", "8", "2020");
        String cardTwoGUID = mHelper.setCreditCard(card2);
        assertEquals(2, mHelper.getNumberOfCreditCardsForSettings());

        card.setGUID(cardOneGUID);
        card.setMonth("10");
        card.setNumber("4012888888881881");
        mHelper.setCreditCard(card);
        assertEquals(
                "Should still have only two cards", 2, mHelper.getNumberOfCreditCardsForSettings());

        CreditCard storedCard = mHelper.getCreditCard(cardOneGUID);
        assertEquals(cardOneGUID, storedCard.getGUID());
        assertEquals("", storedCard.getOrigin());
        assertEquals("Visa", storedCard.getName());
        assertEquals("10", storedCard.getMonth());
        assertEquals("4012888888881881", storedCard.getNumber());
        // \u0020\...\u2060 is four dots ellipsis, \u202A is the Left-To-Right Embedding (LTE) mark,
        // \u202C is the Pop Directional Formatting (PDF) mark. Expected string with form
        // 'Visa  <LRE>****1881<PDF>'.
        assertEquals(
                "Visa\u0020\u0020\u202A\u2022\u2060\u2006\u2060\u2022\u2060\u2006\u2060\u2022"
                        + "\u2060\u2006\u2060\u2022\u2060\u2006\u20601881\u202C",
                storedCard.getNetworkAndLastFourDigits());
        assertNotNull(mHelper.getCreditCard(cardTwoGUID));
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
    public void testFifeUrlIsFormattedWithImageSpecs() throws TimeoutException {
        GURL capitalOneIconUrl = new GURL(AutofillUiUtils.CAPITAL_ONE_ICON_URL);
        GURL cardArtUrl = new GURL("http://google.com/test");
        int widthPixels = 32;
        int heightPixels = 20;

        // The URL should be updated as `cardArtUrl=w{width}-h{height}`.
        assertThat(
                        AutofillUiUtils.getFifeIconUrlWithParams(
                                capitalOneIconUrl,
                                widthPixels,
                                heightPixels,
                                /* circleCrop= */ false,
                                /* requestPng= */ false))
                .isEqualTo(
                        new GURL(
                                capitalOneIconUrl.getSpec()
                                        + "=w"
                                        + widthPixels
                                        + "-h"
                                        + heightPixels));
        // The URL should be updated as `cardArtUrl=w{width}-h{height}-cc-rp`.
        assertThat(
                        AutofillUiUtils.getFifeIconUrlWithParams(
                                capitalOneIconUrl,
                                widthPixels,
                                heightPixels,
                                /* circleCrop= */ true,
                                /* requestPng= */ true))
                .isEqualTo(
                        new GURL(
                                capitalOneIconUrl.getSpec()
                                        + "=w"
                                        + widthPixels
                                        + "-h"
                                        + heightPixels
                                        + "-cc"
                                        + "-rp"));
        assertThat(
                        AutofillUiUtils.getFifeIconUrlWithParams(
                                cardArtUrl,
                                widthPixels,
                                heightPixels,
                                /* circleCrop= */ false,
                                /* requestPng= */ true))
                .isEqualTo(
                        new GURL(
                                cardArtUrl.getSpec()
                                        + "=w"
                                        + widthPixels
                                        + "-h"
                                        + heightPixels
                                        + "-rp"));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testAddAndDeleteCreditCard() throws TimeoutException {
        CreditCard card = createLocalCreditCard("Visa", "1234123412341234", "5", "2020");
        card.setOrigin("Chrome settings");
        String cardOneGUID = mHelper.setCreditCard(card);
        assertEquals(1, mHelper.getNumberOfCreditCardsForSettings());

        mHelper.deleteCreditCard(cardOneGUID);
        assertEquals(0, mHelper.getNumberOfCreditCardsForSettings());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testRespectCountryCodes() throws TimeoutException {
        // The constructor should accept country names and ISO 3166-1-alpha-2 country codes.
        // getInfo(FieldType.ADDRESS_HOME_CONTRY) should return a country code.
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

        assertEquals(2, mHelper.getNumberOfProfilesForSettings());

        AutofillProfile storedProfile1 = mHelper.getProfile(profileGuid1);
        assertEquals("CA", storedProfile1.getInfo(FieldType.ADDRESS_HOME_COUNTRY));

        AutofillProfile storedProfile2 = mHelper.getProfile(profileGuid2);
        assertEquals("CA", storedProfile2.getInfo(FieldType.ADDRESS_HOME_COUNTRY));
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
                        .setEmailAddress(/* emailAddress= */ "", VerificationStatus.NO_STATUS)
                        .setLanguageCode("")
                        .build();
        String guid = mHelper.setProfile(profileWithDifferentStatuses);
        assertEquals(1, mHelper.getNumberOfProfilesForSettings());

        AutofillProfile storedProfile = mHelper.getProfile(guid);
        // When converted to C++ and back the verification statuses for name and address components
        // should be preserved.
        assertEquals(VerificationStatus.PARSED, storedProfile.getInfoStatus(FieldType.NAME_FULL));
        assertEquals(
                VerificationStatus.FORMATTED,
                storedProfile.getInfoStatus(FieldType.ADDRESS_HOME_STREET_ADDRESS));
        assertEquals(
                VerificationStatus.OBSERVED,
                storedProfile.getInfoStatus(FieldType.ADDRESS_HOME_STATE));
        assertEquals(
                VerificationStatus.USER_VERIFIED,
                storedProfile.getInfoStatus(FieldType.ADDRESS_HOME_CITY));
        assertEquals(
                VerificationStatus.SERVER_PARSED,
                storedProfile.getInfoStatus(FieldType.ADDRESS_HOME_ZIP));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testValuesSetInProfileGainUserVerifiedStatus() {
        AutofillProfile profile = AutofillProfile.builder().build();
        assertEquals(VerificationStatus.NO_STATUS, profile.getInfoStatus(FieldType.NAME_FULL));
        assertEquals(
                VerificationStatus.NO_STATUS,
                profile.getInfoStatus(FieldType.ADDRESS_HOME_STREET_ADDRESS));
        assertEquals(
                VerificationStatus.NO_STATUS, profile.getInfoStatus(FieldType.ADDRESS_HOME_CITY));

        profile.setFullName("Homer Simpson");
        assertEquals(VerificationStatus.USER_VERIFIED, profile.getInfoStatus(FieldType.NAME_FULL));
        profile.setStreetAddress("123 Main St.");
        assertEquals(
                VerificationStatus.USER_VERIFIED,
                profile.getInfoStatus(FieldType.ADDRESS_HOME_STREET_ADDRESS));
        profile.setLocality("Springfield");
        assertEquals(
                VerificationStatus.USER_VERIFIED,
                profile.getInfoStatus(FieldType.ADDRESS_HOME_CITY));
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
        assertEquals(1, mHelper.getNumberOfProfilesForSettings());
        AutofillProfile storedProfile1 = mHelper.getProfile(profileGuid1);
        assertEquals("PF", storedProfile1.getInfo(FieldType.ADDRESS_HOME_COUNTRY));
        assertEquals("Monsieur Jean DELHOURME", storedProfile1.getInfo(FieldType.NAME_FULL));
        assertEquals(streetAddress1, storedProfile1.getInfo(FieldType.ADDRESS_HOME_STREET_ADDRESS));
        assertEquals("Tahiti", storedProfile1.getInfo(FieldType.ADDRESS_HOME_STATE));
        assertEquals("Mahina", storedProfile1.getInfo(FieldType.ADDRESS_HOME_CITY));
        assertEquals("Orofara", storedProfile1.getInfo(FieldType.ADDRESS_HOME_DEPENDENT_LOCALITY));
        assertEquals("98709", storedProfile1.getInfo(FieldType.ADDRESS_HOME_ZIP));
        assertEquals("CEDEX 98703", storedProfile1.getInfo(FieldType.ADDRESS_HOME_SORTING_CODE));
        assertEquals("44.71.53", storedProfile1.getInfo(FieldType.PHONE_HOME_WHOLE_NUMBER));
        assertEquals("john@acme.inc", storedProfile1.getInfo(FieldType.EMAIL_ADDRESS));

        profile.setStreetAddress(streetAddress2);
        String profileGuid2 = mHelper.setProfile(profile);
        assertEquals(2, mHelper.getNumberOfProfilesForSettings());
        AutofillProfile storedProfile2 = mHelper.getProfile(profileGuid2);
        assertEquals(streetAddress2, storedProfile2.getInfo(FieldType.ADDRESS_HOME_STREET_ADDRESS));
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

        List<String> expectedLabels = new ArrayList<>();
        expectedLabels.add("123 Main, jm@example.com");
        expectedLabels.add("123 Main, jm-work@example.com");
        expectedLabels.add("1500 Second Ave, 90068");
        expectedLabels.add("Fort Worth, Texas");

        List<AutofillProfile> profiles = mHelper.getProfilesForSettings();
        assertEquals(expectedLabels.size(), profiles.size());
        for (int i = 0; i < profiles.size(); ++i) {
            String label = profiles.get(i).getLabel();
            int idx = expectedLabels.indexOf(label);
            assertThat("Found unexpected label [" + label + "]", idx, is(not(-1)));
            expectedLabels.remove(idx);
        }
    }

    @Test
    @SmallTest
    public void testProfileEditorDescription() throws TimeoutException {
        AutofillProfile profile =
                AutofillProfile.builder()
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .build();

        String guid = mHelper.setProfile(profile);
        String profileDescription = mHelper.getProfileDescriptionForEditor(guid);
        assertEquals("123 Main, Los Angeles", profileDescription);
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
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

        List<AutofillProfile> profiles = mHelper.getProfilesToSuggest();
        assertEquals(3, profiles.size());
        assertTrue("Profile1 should be ranked first", guid1.equals(profiles.get(0).getGUID()));
        assertTrue("Profile3 should be ranked second", guid3.equals(profiles.get(1).getGUID()));
        assertTrue("Profile2 should be ranked third", guid2.equals(profiles.get(2).getGUID()));
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

        // The first credit card has the lowest use count but has most recently been used, making it
        // ranked first.
        String guid1 = mHelper.addCreditCardWithUseStatsForTesting(card1, 6, 1);
        // The second credit card has the median use count and use date, and with these
        // values it is ranked third.
        String guid2 = mHelper.addCreditCardWithUseStatsForTesting(card2, 25, 10);
        // The third credit card has the highest use count and is the credit card with the farthest
        // last use date. Because of its very high use count, it is still ranked second.
        String guid3 = mHelper.addCreditCardWithUseStatsForTesting(card3, 100, 20);

        List<CreditCard> cards = mHelper.getCreditCardsToSuggest();
        assertEquals(3, cards.size());
        assertTrue("Card1 should be ranked first", guid1.equals(cards.get(0).getGUID()));
        assertTrue("Card3 should be ranked second", guid3.equals(cards.get(1).getGUID()));
        assertTrue("Card2 should be ranked third", guid2.equals(cards.get(2).getGUID()));
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
        assertEquals(1, mHelper.getNumberOfCreditCardsToSuggest());

        // Both cards should be seen in the settings even if they are identical.
        assertEquals(2, mHelper.getNumberOfCreditCardsForSettings());
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testProfileUseStatsSettingAndGetting() throws TimeoutException {
        String guid = mHelper.setProfile(createTestProfile());

        // Make sure the profile does not have the specific use stats form the start.
        assertThat(mHelper.getProfileUseCountForTesting(guid), is(not(1234)));
        assertThat(mHelper.getProfileUseDateForTesting(guid), is(not(1234)));

        // Set specific use stats for the profile.
        mHelper.setProfileUseStatsForTesting(guid, 1234, 1234);

        // Make sure the specific use stats were set for the profile.
        assertEquals(1234, mHelper.getProfileUseCountForTesting(guid));
        assertEquals(
                mHelper.getDateNDaysAgoForTesting(1234), mHelper.getProfileUseDateForTesting(guid));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testCreditCardWithUseStatsSettingAndGetting() throws TimeoutException {
        // Set a credit card with specific use stats.
        String guid =
                mHelper.addCreditCardWithUseStatsForTesting(
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
                                /* serverId= */ ""),
                        1234,
                        1234);

        // Make sure the specific use stats were set for the credit card.
        assertEquals(1234, mHelper.getCreditCardUseCountForTesting(guid));
        assertEquals(
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
        assertEquals(1235, mHelper.getProfileUseCountForTesting(guid));
        assertTrue(timeBeforeRecord <= mHelper.getProfileUseDateForTesting(guid));
        assertTrue(timeAfterRecord >= mHelper.getProfileUseDateForTesting(guid));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testRecordAndLogCreditCardUse() throws TimeoutException {
        String guid =
                mHelper.addCreditCardWithUseStatsForTesting(
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
                                /* serverId= */ ""),
                        1234,
                        1234);

        // Get the current date value just before the call to record and log.
        long timeBeforeRecord = mHelper.getCurrentDateForTesting();

        // Record and log use of the credit card.
        mHelper.recordAndLogCreditCardUse(guid);

        // Get the current date value just after the call to record and log.
        long timeAfterRecord = mHelper.getCurrentDateForTesting();

        // Make sure the use stats of the credit card were updated.
        assertEquals(1235, mHelper.getCreditCardUseCountForTesting(guid));
        assertTrue(timeBeforeRecord <= mHelper.getCreditCardUseDateForTesting(guid));
        assertTrue(timeAfterRecord >= mHelper.getCreditCardUseDateForTesting(guid));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testGetProfilesToSuggest() throws TimeoutException {
        mHelper.setProfile(createTestProfile());

        List<AutofillProfile> profiles = mHelper.getProfilesToSuggest();
        assertEquals(
                "Acme Inc., 123 Main, Los Angeles, California 90210, United States",
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
        assertEquals(1, mHelper.getNumberOfCreditCardsForSettings());

        // Clears all server data.
        mHelper.clearAllDataForTesting();
        assertEquals(0, mHelper.getNumberOfCreditCardsForSettings());

        mHelper.setProfile(createTestProfile());
        mHelper.setCreditCard(localCard);
        mHelper.addServerCreditCard(serverCard);
        assertEquals(1, mHelper.getNumberOfProfilesForSettings());
        assertEquals(2, mHelper.getNumberOfCreditCardsForSettings());

        // Clears all server and local data.
        mHelper.clearAllDataForTesting();
        assertEquals(0, mHelper.getNumberOfProfilesForSettings());
        assertEquals(0, mHelper.getNumberOfCreditCardsForSettings());
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
        assertEquals("My IBAN", storedLocalIban.getNickname());
        assertEquals("FR7630006000011234567890189", storedLocalIban.getValue());
        assertEquals(IbanRecordType.LOCAL_IBAN, storedLocalIban.getRecordType());
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
        assertEquals("My IBAN", storedLocalIban.getNickname());
        assertEquals("FR7630006000011234567890189", storedLocalIban.getValue());
        assertEquals(IbanRecordType.LOCAL_IBAN, storedLocalIban.getRecordType());

        // Test "edit IBAN" workflow.
        storedLocalIban.updateNickname("My alternative IBAN");
        storedLocalIban.updateValue("DE91 1000 0000 0123 4567 89");
        mHelper.addOrUpdateLocalIban(storedLocalIban);

        Iban updatedLocalIban = mHelper.getIban(ibanGuid);
        assertEquals("My alternative IBAN", updatedLocalIban.getNickname());
        assertEquals("DE91100000000123456789", updatedLocalIban.getValue());
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
        assertEquals("CH" + "\u2006" + dot.repeat(2) + "8009", storedLocalIban.getLabel());
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

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testGetEwallet() throws TimeoutException {
        Ewallet ewallet1 =
                new Ewallet.Builder()
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100)
                                        .setNickname("nickname")
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setIsFidoEnrolled(true)
                                        .build())
                        .setEwalletName("eWallet name 1")
                        .setAccountDisplayName("account display name 1")
                        .build();
        Ewallet ewallet2 =
                new Ewallet.Builder()
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(200)
                                        .setNickname("nickname2")
                                        .setSupportedPaymentRails(new int[] {2})
                                        .setDisplayIconUrl(new GURL("http://example.com"))
                                        .setIsFidoEnrolled(false)
                                        .build())
                        .setEwalletName("eWallet name 2")
                        .setAccountDisplayName("account display name 2")
                        .build();
        AutofillTestHelper.addEwallet(ewallet1);
        AutofillTestHelper.addEwallet(ewallet2);

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertThat(new Ewallet[] {ewallet1, ewallet2})
                                .isEqualTo(
                                        AutofillTestHelper
                                                .getPersonalDataManagerForLastUsedProfile()
                                                .getEwallets()));
    }

    @Test
    @SmallTest
    @Feature({"Autofill"})
    public void testToggleOptInEmitsMetric() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PersonalDataManager pdm =
                            AutofillTestHelper.getPersonalDataManagerForLastUsedProfile();
                    assertTrue(pdm.isAutofillProfileEnabled());

                    HistogramWatcher histogramExpectation =
                            HistogramWatcher.newSingleRecordWatcher(
                                    PersonalDataManager
                                            .AUTOFILL_ADDRESS_OPT_IN_CHANGE_HISTOGRAM_NAME,
                                    PersonalDataManager.AutofillAddressOptInChange.OPT_OUT);
                    pdm.setAutofillProfileEnabled(false);
                    assertFalse(pdm.isAutofillProfileEnabled());
                    histogramExpectation.assertExpected();

                    histogramExpectation =
                            HistogramWatcher.newSingleRecordWatcher(
                                    PersonalDataManager
                                            .AUTOFILL_ADDRESS_OPT_IN_CHANGE_HISTOGRAM_NAME,
                                    PersonalDataManager.AutofillAddressOptInChange.OPT_IN);
                    pdm.setAutofillProfileEnabled(true);
                    assertTrue(pdm.isAutofillProfileEnabled());
                    histogramExpectation.assertExpected();
                });
    }
}
