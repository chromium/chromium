// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.blink.mojom.ContactIconBlob;
import org.chromium.chrome.R;
import org.chromium.payments.mojom.PaymentAddress;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A class to keep track of the metadata associated with a contact.
 */
public class ContactDetails implements Comparable<ContactDetails> {
    // The identifier for the information from the signed in user. Must not be a valid id in the
    // context of the Android Contacts list.
    public static final String SELF_CONTACT_ID = "-1";

    /**
     * A container class for delivering contact details in abbreviated form
     * (where only the first email and phone numbers are returned and the rest
     * is indicated with "+n more" strings).
     */
    public static class AbbreviatedContactDetails {
        public String primaryEmail;
        public String overflowEmailCount;
        public String primaryTelephoneNumber;
        public String overflowTelephoneNumberCount;
        public String primaryAddress;
        public String overflowAddressCount;
    }

    // The unique id for the contact.
    private final String mId;

    // The display name for this contact.
    private final String mDisplayName;

    // The list of emails registered for this contact.
    private final List<String> mEmails;

    // The list of phone numbers registered for this contact.
    private final List<String> mPhoneNumbers;

    // The list of addresses registered for this contact.
    private final List<PaymentAddress> mAddresses;

    // The list of icons registered for this contact.
    private final List<ContactIconBlob> mIcons;

    // Keeps track of whether this is the contact detail for the owner of the device.
    private boolean mIsSelf;

    // The avatar icon for the owner of the device. Non-null only if the ContactDetails representing
    // the owner were synthesized (not when a pre-existing contact tile was moved to the top).
    @Nullable
    private Drawable mSelfIcon;

    /**
     * The ContactDetails constructor.
     * @param id The unique identifier of this contact.
     * @param displayName The display name of this contact.
     * @param emails The emails registered for this contact.
     * @param phoneNumbers The phone numbers registered for this contact.
     * @param addresses The addresses registered for this contact.
     */
    public ContactDetails(String id, String displayName, List<String> emails,
            List<String> phoneNumbers, List<PaymentAddress> addresses) {
        mDisplayName = displayName != null ? displayName : "";
        mEmails = emails != null ? emails : new ArrayList<String>();
        mPhoneNumbers = phoneNumbers != null ? phoneNumbers : new ArrayList<String>();
        mAddresses = addresses != null ? addresses : new ArrayList<PaymentAddress>();
        mIcons = new ArrayList<>();

        mId = id;
    }

    public List<String> getDisplayNames() {
        return Arrays.asList(mDisplayName);
    }

    public List<String> getEmails() {
        return mEmails;
    }

    public List<String> getPhoneNumbers() {
        return mPhoneNumbers;
    }

    public List<PaymentAddress> getAddresses() {
        return mAddresses;
    }

    public List<ContactIconBlob> getIcons() {
        return mIcons;
    }

    public String getDisplayName() {
        return mDisplayName;
    }

    public String getId() {
        return mId;
    }

    public void setIcon(ContactIconBlob icon) {
        assert mIcons.isEmpty();
        mIcons.add(icon);
    }

    /**
     * Marks whether object is representing the owner of the device.
     * @param value True if this is the contact details for the owner. False otherwise.
     */
    public void setIsSelf(boolean value) {
        mIsSelf = value;
    }

    /**
     * Returns true if this contact detail is representing the owner of the device.
     */
    public boolean isSelf() {
        return mIsSelf;
    }

    /**
     * Sets the icon representing the owner of the device.
     */
    public void setSelfIcon(Drawable icon) {
        mSelfIcon = icon;
    }

    /**
     * Fetch the cached icon for this contact. Returns null if this is not the 'self' contact, all
     * other contact avatars should be retrieved through the {@link FetchIconWorkerTask}.
     */
    @Nullable
    public Drawable getSelfIcon() {
        return mSelfIcon;
    }

    /**
     * Accessor for the abbreviated display name (first letter of first name and first letter of
     * last name).
     * @return The display name, abbreviated to two characters.
     */
    public String getDisplayNameAbbreviation() {
        // Display the two letter abbreviation of the display name.
        String displayChars = "";
        if (mDisplayName.length() > 0) {
            displayChars += mDisplayName.charAt(0);
            String[] parts = mDisplayName.split(" ");
            if (parts.length > 1) {
                displayChars += parts[parts.length - 1].charAt(0);
            }
        }

        return displayChars;
    }

    private String ensureSingleLine(String address) {
        String returnValue = address.replaceAll("\n\n", "\n");
        // The string might have multiple consecutive new-lines, which means \n\n\n -> \n\n, so
        // we'll perform the conversion until we've caught them all.
        while (returnValue.length() < address.length()) {
            address = returnValue;
            returnValue = address.replaceAll("\n\n", "\n");
        }

        return returnValue.replaceAll("\n", ", ");
    }

    /**
     * Accessor for the list of contact details (emails and phone numbers). Returned as strings
     * separated by newline).
     * @param includeAddresses Whether to include addresses in the returned results.
     * @param includeEmails Whether to include emails in the returned results.
     * @param includeTels Whether to include telephones in the returned results.
     * @return A string containing all the contact details registered for this contact.
     */
    public String getContactDetailsAsString(
            boolean includeAddresses, boolean includeEmails, boolean includeTels) {
        int count = 0;
        StringBuilder builder = new StringBuilder();
        if (includeAddresses) {
            for (PaymentAddress address : mAddresses) {
                if (count++ > 0) {
                    builder.append("\n");
                }
                builder.append(ensureSingleLine(address.addressLine[0]));
            }
        }
        if (includeEmails) {
            for (String email : mEmails) {
                if (count++ > 0) {
                    builder.append("\n");
                }
                builder.append(email);
            }
        }
        if (includeTels) {
            for (String phoneNumber : mPhoneNumbers) {
                if (count++ > 0) {
                    builder.append("\n");
                }
                builder.append(phoneNumber);
            }
        }

        return builder.toString();
    }

    /**
     * Accessor for the list of contact details (emails and phone numbers).
     * @param includeAddresses Whether to include addresses in the returned results.
     * @param includeEmails Whether to include emails in the returned results.
     * @param includeTels Whether to include telephones in the returned results.
     * @param resources The resources to use for fetching the string. Must be provided.
     * @return The contact details registered for this contact.
     */
    public AbbreviatedContactDetails getAbbreviatedContactDetails(boolean includeAddresses,
            boolean includeEmails, boolean includeTels, @Nullable Resources resources) {
        AbbreviatedContactDetails results = new AbbreviatedContactDetails();

        results.overflowAddressCount = "";
        if (!includeAddresses || mAddresses.size() == 0) {
            results.primaryAddress = "";
        } else {
            results.primaryAddress = ensureSingleLine(mAddresses.get(0).addressLine[0]);
            int totalAddresses = mAddresses.size();
            if (totalAddresses > 1) {
                int hiddenAddresses = totalAddresses - 1;
                results.overflowAddressCount = resources.getQuantityString(
                        R.plurals.contacts_picker_more_details, hiddenAddresses, hiddenAddresses);
            }
        }

        results.overflowEmailCount = "";
        if (!includeEmails || mEmails.size() == 0) {
            results.primaryEmail = "";
        } else {
            results.primaryEmail = mEmails.get(0);
            int totalAddresses = mEmails.size();
            if (totalAddresses > 1) {
                int hiddenAddresses = totalAddresses - 1;
                results.overflowEmailCount = resources.getQuantityString(
                        R.plurals.contacts_picker_more_details, hiddenAddresses, hiddenAddresses);
            }
        }

        results.overflowTelephoneNumberCount = "";
        if (!includeTels || mPhoneNumbers.size() == 0) {
            results.primaryTelephoneNumber = "";
        } else {
            results.primaryTelephoneNumber = mPhoneNumbers.get(0);
            int totalNumbers = mPhoneNumbers.size();
            if (totalNumbers > 1) {
                int hiddenNumbers = totalNumbers - 1;
                results.overflowTelephoneNumberCount = resources.getQuantityString(
                        R.plurals.contacts_picker_more_details, hiddenNumbers, hiddenNumbers);
            }
        }

        return results;
    }

    /**
     * A comparison function (results in a full name ascending sorting).
     * @param other The other ContactDetails object to compare it with.
     * @return 0, 1, or -1, depending on which is bigger.
     */
    @Override
    public int compareTo(ContactDetails other) {
        return other.mDisplayName.compareTo(mDisplayName);
    }

    @Override
    public int hashCode() {
        Object[] values = {mId, mDisplayName};
        return Arrays.hashCode(values);
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (object == null) return false;
        if (object == this) return true;
        if (!(object instanceof ContactDetails)) return false;

        ContactDetails otherInfo = (ContactDetails) object;
        return mId.equals(otherInfo.mId);
    }
}
