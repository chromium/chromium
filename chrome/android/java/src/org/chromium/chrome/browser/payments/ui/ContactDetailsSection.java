// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.ContactEditor;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.Section;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/** The data to show in the contact details section where the user can select something. */
public class ContactDetailsSection extends SectionInformation {
    private final Context mContext;
    private final ContactEditor mContactEditor;
    private final List<AutofillProfile> mProfiles;

    /**
     * Builds a Contact section from a list of AutofillProfile.
     *
     * @param context               Context
     * @param unmodifiableProfiles  The list of profiles to build from.
     * @param contactEditor         The Contact Editor associated with this flow.
     * @param journeyLogger         The JourneyLogger for the current Payment Request.
     */
    public ContactDetailsSection(
            Context context,
            Collection<AutofillProfile> unmodifiableProfiles,
            ContactEditor contactEditor,
            JourneyLogger journeyLogger) {
        // Initially no items are selected, but they are updated later in the constructor.
        super(PaymentRequestUI.DataType.CONTACT_DETAILS, null);

        mContext = context;
        mContactEditor = contactEditor;
        // Copy the profiles from which this section is derived.
        mProfiles = new ArrayList<AutofillProfile>(unmodifiableProfiles);

        // Refresh the contact section items and selection.
        createContactListFromAutofillProfiles(journeyLogger);
    }

    /**
     * Add (or update) an address in the contact details section.
     *
     * @param editedAddress the new or edited address with which to update the contacts section.
     */
    public void addOrUpdateWithAutofillAddress(AutofillAddress editedAddress) {
        if (editedAddress == null) return;

        // If the profile is currently being displayed, update the items in anticipation of the
        // contacts section refresh. The updatedContact can be null when user has added a new
        // shipping address without an email, but the contact info section requires only email
        // address. Null updatedContact should not be added to the mItems list.
        @Nullable
        AutofillContact updatedContact =
                createAutofillContactFromProfile(editedAddress.getProfile());
        if (null == updatedContact) return;

        for (int i = 0; i < mItems.size(); i++) {
            AutofillContact existingContact = (AutofillContact) mItems.get(i);
            if (existingContact
                    .getProfile()
                    .getGUID()
                    .equals(editedAddress.getProfile().getGUID())) {
                // We need to replace |existingContact| with |updatedContact|.
                mItems.remove(i);
                mItems.add(i, updatedContact);
                return;
            }
        }
        // The contact didn't exist. Add the new address to |mItems| to the end of the list, in
        // anticipation of the contacts section refresh.
        mItems.add(updatedContact);

        // The selection is not updated.
    }

    /** Recomputes the list of displayed contacts and possibly updates the selection. */
    private void createContactListFromAutofillProfiles(JourneyLogger journeyLogger) {
        List<AutofillContact> contacts = new ArrayList<>();
        List<AutofillContact> uniqueContacts = new ArrayList<>();

        // Add the profile's valid request values to the editor's autocomplete list and convert
        // relevant profiles to AutofillContacts, to be deduped later.
        for (int i = 0; i < mProfiles.size(); ++i) {
            AutofillContact contact = createAutofillContactFromProfile(mProfiles.get(i));

            // Only create a contact if the profile has relevant information for the merchant.
            if (contact != null) {
                mContactEditor.addPayerNameIfValid(contact.getPayerName());
                mContactEditor.addPhoneNumberIfValid(contact.getPayerPhone());
                mContactEditor.addEmailAddressIfValid(contact.getPayerEmail());

                contacts.add(contact);
            }
        }

        // Order the contacts so the ones that have most of the required information are put first.
        // The sort is stable, so contacts with the same relevance score are sorted by frecency.
        Collections.sort(
                contacts,
                new Comparator<AutofillContact>() {
                    @Override
                    public int compare(AutofillContact a, AutofillContact b) {
                        return b.getRelevanceScore() - a.getRelevanceScore();
                    }
                });

        // This algorithm is quadratic, but since the number of contacts is generally very small
        // ( < 10) a faster but more complicated algorithm would be overkill.
        for (int i = 0; i < contacts.size(); i++) {
            AutofillContact contact = contacts.get(i);

            // Different contacts can have identical info. Do not add the same contact info or a
            // subset of it twice. It's important that the profiles be sorted by the quantity of
            // required info they have.
            boolean isNewSuggestion = true;
            for (int j = 0; j < uniqueContacts.size(); ++j) {
                if (uniqueContacts.get(j).isEqualOrSupersetOf(contact)) {
                    isNewSuggestion = false;
                    break;
                }
            }
            if (isNewSuggestion) uniqueContacts.add(contact);

            // Limit the number of suggestions.
            if (uniqueContacts.size() == PaymentUiService.SUGGESTIONS_LIMIT) break;
        }

        // Automatically select the first address if it is complete.
        int firstCompleteContactIndex = SectionInformation.NO_SELECTION;
        if (!uniqueContacts.isEmpty() && uniqueContacts.get(0).isComplete()) {
            firstCompleteContactIndex = 0;
        }

        // TODO(crbug.com/40530700): Remove this once a journeyLogger is passed in tests.
        if (journeyLogger != null) {
            // Log the number of suggested contact info.
            journeyLogger.setNumberOfSuggestionsShown(
                    Section.CONTACT_INFO,
                    uniqueContacts.size(),
                    firstCompleteContactIndex != SectionInformation.NO_SELECTION);
        }

        updateItemsWithCollection(firstCompleteContactIndex, uniqueContacts);
    }

    private @Nullable AutofillContact createAutofillContactFromProfile(AutofillProfile profile) {
        boolean requestPayerName = mContactEditor.getRequestPayerName();
        boolean requestPayerPhone = mContactEditor.getRequestPayerPhone();
        boolean requestPayerEmail = mContactEditor.getRequestPayerEmail();
        String name =
                requestPayerName && !TextUtils.isEmpty(profile.getFullName())
                        ? profile.getFullName()
                        : null;
        String phone =
                requestPayerPhone && !TextUtils.isEmpty(profile.getPhoneNumber())
                        ? profile.getPhoneNumber()
                        : null;
        String email =
                requestPayerEmail && !TextUtils.isEmpty(profile.getEmailAddress())
                        ? profile.getEmailAddress()
                        : null;

        if (name != null || phone != null || email != null) {
            @ContactEditor.CompletionStatus
            int completionStatus = mContactEditor.checkContactCompletionStatus(name, phone, email);
            return new AutofillContact(
                    mContext,
                    profile,
                    name,
                    phone,
                    email,
                    completionStatus,
                    requestPayerName,
                    requestPayerPhone,
                    requestPayerEmail);
        }
        return null;
    }
}
