// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.payments.ui.ContactDetailsSection;
import org.chromium.chrome.browser.payments.ui.SectionInformation;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.EditableOption;

import java.util.ArrayList;
import java.util.List;

/** Tests for the PaymentRequest ContactDetailsSection class. */
@RunWith(BaseJUnit4ClassRunner.class)
public class PaymentRequestContactDetailsSectionUnitTest {
    @Rule public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private ContactEditor mContactEditor;
    private ContactDetailsSection mContactDetailsSection;

    private void createContactDetailsSectionWithProfiles(
            List<AutofillProfile> autofillProfiles,
            boolean requestPayerName,
            boolean requestPayerPhone,
            boolean requestPayerEmail) {
        mContactEditor =
                new ContactEditor(
                        requestPayerName,
                        requestPayerPhone,
                        requestPayerEmail,
                        /* saveToDisk= */ true,
                        AutofillTestHelper.getPersonalDataManagerForLastUsedProfile());
        mContactDetailsSection =
                new ContactDetailsSection(
                        ApplicationProvider.getApplicationContext(),
                        autofillProfiles,
                        mContactEditor,
                        null);
    }

    /** Tests the creation of the contact list, with most complete first. */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsCreated_MostCompleteFirst() {
        List<AutofillProfile> profiles = new ArrayList<>();
        // Name, phone and email are all different. First entry is incomplete.
        profiles.add(
                AutofillProfile.builder()
                        .setGUID("guid-1")
                        .setFullName("John Major")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .setPhoneNumber("") /* no phone number */
                        .setEmailAddress("jm@example.test")
                        .build());
        profiles.add(
                AutofillProfile.builder()
                        .setGUID("guid-2")
                        .setFullName("Jane Doe")
                        .setCompanyName("Edge corp.")
                        .setStreetAddress("123 Main")
                        .setRegion("Washington")
                        .setLocality("Lake City")
                        .setPostalCode("10110")
                        .setCountryCode("US")
                        .setPhoneNumber("555-212-1212")
                        .setEmailAddress("jane@example.test")
                        .build());

        createContactDetailsSectionWithProfiles(
                profiles,
                /* requestPayerName= */ true,
                /* requestPayerPhone= */ true,
                /* requestPayerEmail= */ true);

        List<EditableOption> items = mContactDetailsSection.getItems();
        Assert.assertEquals(2, items.size());
        Assert.assertEquals(0, mContactDetailsSection.getSelectedItemIndex());
        // Most complete item is going to be at the top.
        Assert.assertEquals("Jane Doe", items.get(0).getLabel());
        Assert.assertEquals("555-212-1212", items.get(0).getSublabel());
        Assert.assertEquals("jane@example.test", items.get(0).getTertiaryLabel());
        Assert.assertEquals(null, items.get(0).getEditMessage());

        Assert.assertEquals("John Major", items.get(1).getLabel());
        Assert.assertEquals("jm@example.test", items.get(1).getSublabel());
        Assert.assertEquals(null, items.get(1).getTertiaryLabel());
        Assert.assertEquals("Phone number required", items.get(1).getEditMessage());
    }

    /** Tests the creation of the contact list, with all profiles being complete. */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsCreated_AllComplete() {
        List<AutofillProfile> profiles = new ArrayList<>();
        // Name, phone and email are all different. All entries complete.
        profiles.add(
                AutofillProfile.builder()
                        .setGUID("guid-1")
                        .setFullName("John Major")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .setPhoneNumber("514-555-1212")
                        .setEmailAddress("jm@example.test")
                        .build());
        profiles.add(
                AutofillProfile.builder()
                        .setGUID("guid-2")
                        .setFullName("Jane Doe")
                        .setCompanyName("Edge corp.")
                        .setStreetAddress("123 Main")
                        .setRegion("Washington")
                        .setLocality("Lake City")
                        .setPostalCode("10110")
                        .setCountryCode("US")
                        .setPhoneNumber("555-212-1212")
                        .setEmailAddress("jane@example.test")
                        .build());

        createContactDetailsSectionWithProfiles(
                profiles,
                /* requestPayerName= */ true,
                /* requestPayerPhone= */ true,
                /* requestPayerEmail= */ true);

        List<EditableOption> items = mContactDetailsSection.getItems();
        Assert.assertEquals(2, items.size());
        Assert.assertEquals(0, mContactDetailsSection.getSelectedItemIndex());
        // Since all are complete, the first profile in the list comes up first in the section.
        Assert.assertEquals("John Major", items.get(0).getLabel());
        Assert.assertEquals("514-555-1212", items.get(0).getSublabel());
        Assert.assertEquals("jm@example.test", items.get(0).getTertiaryLabel());
        Assert.assertEquals(null, items.get(0).getEditMessage());

        Assert.assertEquals("Jane Doe", items.get(1).getLabel());
        Assert.assertEquals("555-212-1212", items.get(1).getSublabel());
        Assert.assertEquals("jane@example.test", items.get(1).getTertiaryLabel());
        Assert.assertEquals(null, items.get(1).getEditMessage());
    }

    /** Tests the creation of the contact list, not requesting the missing value. */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsCreated_NotRequestingMissingValue() {
        List<AutofillProfile> profiles = new ArrayList<>();
        // Entry is incomplete but it will not matter.
        profiles.add(
                AutofillProfile.builder()
                        .setGUID("guid-1")
                        .setFullName("John Major")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .setPhoneNumber("") /* no phone number */
                        .setEmailAddress("jm@example.test")
                        .build());

        createContactDetailsSectionWithProfiles(
                profiles,
                /* requestPayerName= */ true,
                /* requestPayerPhone= */ false,
                /* requestPayerEmail= */ true);

        List<EditableOption> items = mContactDetailsSection.getItems();
        Assert.assertEquals(1, items.size());
        Assert.assertEquals(0, mContactDetailsSection.getSelectedItemIndex());
        // Since the phone number was not request, there is no error message.
        Assert.assertEquals("John Major", items.get(0).getLabel());
        Assert.assertEquals("jm@example.test", items.get(0).getSublabel());
        Assert.assertEquals(null, items.get(0).getTertiaryLabel());
        Assert.assertEquals(null, items.get(0).getEditMessage());
    }

    /** Tests the update of the contact list with complete data. */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsUpdated_WithCompleteAddress() {
        List<AutofillProfile> profiles = new ArrayList<>();
        // First entry is complete.
        profiles.add(
                AutofillProfile.builder()
                        .setGUID("guid-1")
                        .setFullName("John Major")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .setPhoneNumber("514-555-1212")
                        .setEmailAddress("jm@example.test")
                        .build());
        createContactDetailsSectionWithProfiles(
                profiles,
                /* requestPayerName= */ true,
                /* requestPayerPhone= */ true,
                /* requestPayerEmail= */ true);

        List<EditableOption> items = mContactDetailsSection.getItems();
        Assert.assertEquals(1, items.size());
        Assert.assertEquals(0, mContactDetailsSection.getSelectedItemIndex());
        // Only item shows up as expected.
        Assert.assertEquals("John Major", items.get(0).getLabel());
        Assert.assertEquals("514-555-1212", items.get(0).getSublabel());
        Assert.assertEquals("jm@example.test", items.get(0).getTertiaryLabel());
        Assert.assertEquals(null, items.get(0).getEditMessage());

        // We update the contact list with a new, complete address.
        AutofillProfile newProfile =
                AutofillProfile.builder()
                        .setGUID("guid-2")
                        .setFullName("Jane Doe")
                        .setCompanyName("Edge corp.")
                        .setStreetAddress("123 Main")
                        .setRegion("Washington")
                        .setLocality("Lake City")
                        .setPostalCode("10110")
                        .setCountryCode("US")
                        .setPhoneNumber("555-212-1212")
                        .setEmailAddress("jane@example.test")
                        .build();
        mContactDetailsSection.addOrUpdateWithAutofillAddress(
                new AutofillAddress(
                        ApplicationProvider.getApplicationContext(),
                        newProfile,
                        AutofillTestHelper.getPersonalDataManagerForLastUsedProfile()));

        // We now expect the new item to be last.
        items = mContactDetailsSection.getItems();
        Assert.assertEquals(2, items.size());
        Assert.assertEquals(0, mContactDetailsSection.getSelectedItemIndex());

        Assert.assertEquals("John Major", items.get(0).getLabel());
        Assert.assertEquals("514-555-1212", items.get(0).getSublabel());
        Assert.assertEquals("jm@example.test", items.get(0).getTertiaryLabel());
        Assert.assertEquals(null, items.get(0).getEditMessage());

        Assert.assertEquals("Jane Doe", items.get(1).getLabel());
        Assert.assertEquals("555-212-1212", items.get(1).getSublabel());
        Assert.assertEquals("jane@example.test", items.get(1).getTertiaryLabel());
        Assert.assertEquals(null, items.get(1).getEditMessage());
    }

    /** Tests the update of the contact list with incomplete */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsUpdated_WithNewButIncomplete() {
        List<AutofillProfile> profiles = new ArrayList<>();
        // Name, phone and email are all different. All entries complete.
        profiles.add(
                AutofillProfile.builder()
                        .setGUID("guid-1")
                        .setFullName("John Major")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .setPhoneNumber("514-555-1212")
                        .setEmailAddress("jm@example.test")
                        .build());
        createContactDetailsSectionWithProfiles(
                profiles,
                /* requestPayerName= */ true,
                /* requestPayerPhone= */ true,
                /* requestPayerEmail= */ true);

        List<EditableOption> items = mContactDetailsSection.getItems();
        Assert.assertEquals(1, items.size());
        Assert.assertEquals(0, mContactDetailsSection.getSelectedItemIndex());
        // Only item shows up as expected.
        Assert.assertEquals("John Major", items.get(0).getLabel());
        Assert.assertEquals("514-555-1212", items.get(0).getSublabel());
        Assert.assertEquals("jm@example.test", items.get(0).getTertiaryLabel());
        Assert.assertEquals(null, items.get(0).getEditMessage());

        // We update the contact list with a new address, which has a missing email.
        AutofillProfile newProfile =
                AutofillProfile.builder()
                        .setGUID("guid-2")
                        .setFullName("Jane Doe")
                        .setCompanyName("Edge corp.")
                        .setStreetAddress("123 Main")
                        .setRegion("Washington")
                        .setLocality("Lake City")
                        .setPostalCode("10110")
                        .setCountryCode("US")
                        .setPhoneNumber("555-212-1212")
                        .setEmailAddress("") /* No email */
                        .build();
        mContactDetailsSection.addOrUpdateWithAutofillAddress(
                new AutofillAddress(
                        ApplicationProvider.getApplicationContext(),
                        newProfile,
                        AutofillTestHelper.getPersonalDataManagerForLastUsedProfile()));

        // We now expect the new item, because it is incomplete, to be last.
        items = mContactDetailsSection.getItems();
        Assert.assertEquals(2, items.size());
        Assert.assertEquals(0, mContactDetailsSection.getSelectedItemIndex());

        Assert.assertEquals("John Major", items.get(0).getLabel());
        Assert.assertEquals("514-555-1212", items.get(0).getSublabel());
        Assert.assertEquals("jm@example.test", items.get(0).getTertiaryLabel());
        Assert.assertEquals(null, items.get(0).getEditMessage());

        Assert.assertEquals("Jane Doe", items.get(1).getLabel());
        Assert.assertEquals("555-212-1212", items.get(1).getSublabel());
        Assert.assertEquals(null, items.get(1).getTertiaryLabel());
        Assert.assertEquals("Email required", items.get(1).getEditMessage());
    }

    /** Tests the update of initially empty contact list. */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsUpdated_InitiallyEmptyWithCompleteAddress() {
        List<AutofillProfile> profiles = new ArrayList<>();
        createContactDetailsSectionWithProfiles(
                profiles,
                /* requestPayerName= */ true,
                /* requestPayerPhone= */ true,
                /* requestPayerEmail= */ true);

        List<EditableOption> items = mContactDetailsSection.getItems();
        Assert.assertEquals(0, items.size());
        Assert.assertEquals(
                SectionInformation.NO_SELECTION, mContactDetailsSection.getSelectedItemIndex());

        // We update the contact list with a new, complete address.
        AutofillProfile newProfile =
                AutofillProfile.builder()
                        .setGUID("guid-2")
                        .setFullName("Jane Doe")
                        .setCompanyName("Edge corp.")
                        .setStreetAddress("123 Main")
                        .setRegion("Washington")
                        .setLocality("Lake City")
                        .setPostalCode("10110")
                        .setCountryCode("US")
                        .setPhoneNumber("555-212-1212")
                        .setEmailAddress("jane@example.test")
                        .build();
        mContactDetailsSection.addOrUpdateWithAutofillAddress(
                new AutofillAddress(
                        ApplicationProvider.getApplicationContext(),
                        newProfile,
                        AutofillTestHelper.getPersonalDataManagerForLastUsedProfile()));

        // We now expect the new item to be first. The selection is not changed.
        items = mContactDetailsSection.getItems();
        Assert.assertEquals(1, items.size());
        Assert.assertEquals(
                SectionInformation.NO_SELECTION, mContactDetailsSection.getSelectedItemIndex());

        Assert.assertEquals("Jane Doe", items.get(0).getLabel());
        Assert.assertEquals("555-212-1212", items.get(0).getSublabel());
        Assert.assertEquals("jane@example.test", items.get(0).getTertiaryLabel());
        Assert.assertEquals(null, items.get(0).getEditMessage());
    }

    /** Tests the update of an existing contact list item. */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsUpdated_UpdateExistingItem() {
        List<AutofillProfile> profiles = new ArrayList<>();
        // This entry is missing an email, which will get added later on.
        profiles.add(
                AutofillProfile.builder()
                        .setGUID("guid-1")
                        .setFullName("John Major")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("123 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .setPhoneNumber("514-555-1212")
                        .setEmailAddress("") /* No email */
                        .build());

        createContactDetailsSectionWithProfiles(
                profiles,
                /* requestPayerName= */ true,
                /* requestPayerPhone= */ true,
                /* requestPayerEmail= */ true);

        List<EditableOption> items = mContactDetailsSection.getItems();
        Assert.assertEquals(1, items.size());
        Assert.assertEquals(
                SectionInformation.NO_SELECTION, mContactDetailsSection.getSelectedItemIndex());
        // Since all are complete, the first profile in the list comes up first in the section.
        Assert.assertEquals("John Major", items.get(0).getLabel());
        Assert.assertEquals("514-555-1212", items.get(0).getSublabel());
        Assert.assertEquals(null, items.get(0).getTertiaryLabel());
        Assert.assertEquals("Email required", items.get(0).getEditMessage());

        // We update the contact list with the same profile GUID, complete this time.
        AutofillProfile newProfile =
                AutofillProfile.builder()
                        .setGUID("guid-1")
                        .setFullName("John Major")
                        .setCompanyName("Acme Inc.")
                        .setStreetAddress("456 Main")
                        .setRegion("California")
                        .setLocality("Los Angeles")
                        .setPostalCode("90210")
                        .setCountryCode("US")
                        .setPhoneNumber("514-555-1212")
                        .setEmailAddress("john@example.test")
                        .build();
        mContactDetailsSection.addOrUpdateWithAutofillAddress(
                new AutofillAddress(
                        ApplicationProvider.getApplicationContext(),
                        newProfile,
                        AutofillTestHelper.getPersonalDataManagerForLastUsedProfile()));

        items = mContactDetailsSection.getItems();
        Assert.assertEquals(1, items.size());
        // The item is now complete, but the selection is not changed.
        Assert.assertEquals(
                SectionInformation.NO_SELECTION, mContactDetailsSection.getSelectedItemIndex());
        Assert.assertEquals("John Major", items.get(0).getLabel());
        Assert.assertEquals("514-555-1212", items.get(0).getSublabel());
        Assert.assertEquals("john@example.test", items.get(0).getTertiaryLabel());
        Assert.assertEquals(null, items.get(0).getEditMessage());
    }

    /** Tests the update of initially empty contacts list with incomplete data. */
    @Test
    @SmallTest
    @Feature({"Payments"})
    public void testContactsListIsUpdated_InitiallyEmptyWithIncompleteAddress() {
        List<AutofillProfile> profiles = new ArrayList<>();
        createContactDetailsSectionWithProfiles(
                profiles,
                /* requestPayerName= */ true,
                /* requestPayerPhone= */ true,
                /* requestPayerEmail= */ true);

        List<EditableOption> items = mContactDetailsSection.getItems();
        Assert.assertEquals(0, items.size());
        Assert.assertEquals(
                SectionInformation.NO_SELECTION, mContactDetailsSection.getSelectedItemIndex());

        // We update the contact list with a new, incomplete address.
        AutofillProfile newProfile =
                AutofillProfile.builder()
                        .setGUID("guid-2")
                        .setFullName("Jane Doe")
                        .setCompanyName("Edge corp.")
                        .setStreetAddress("123 Main")
                        .setRegion("Washington")
                        .setLocality("Lake City")
                        .setPostalCode("10110")
                        .setCountryCode("US")
                        .setPhoneNumber("555-212-1212")
                        .setEmailAddress("") /* no email */
                        .build();
        mContactDetailsSection.addOrUpdateWithAutofillAddress(
                new AutofillAddress(
                        ApplicationProvider.getApplicationContext(),
                        newProfile,
                        AutofillTestHelper.getPersonalDataManagerForLastUsedProfile()));

        // We now expect the new item to be first, but unselected because incomplete.
        items = mContactDetailsSection.getItems();
        Assert.assertEquals(1, items.size());
        Assert.assertEquals(
                SectionInformation.NO_SELECTION, mContactDetailsSection.getSelectedItemIndex());

        Assert.assertEquals("Jane Doe", items.get(0).getLabel());
        Assert.assertEquals("555-212-1212", items.get(0).getSublabel());
        Assert.assertEquals(null, items.get(0).getTertiaryLabel());
        Assert.assertEquals("Email required", items.get(0).getEditMessage());
    }
}
