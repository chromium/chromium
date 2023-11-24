// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.EditableOption;
import org.chromium.payments.mojom.PayerDetail;

/** The locally stored contact details. */
public class AutofillContact extends EditableOption {
    private final AutofillProfile mProfile;
    private final Context mContext;
    private int mCompletionStatus;
    private boolean mRequestName;
    private boolean mRequestPhone;
    private boolean mRequestEmail;
    @Nullable private String mPayerName;
    @Nullable private String mPayerPhone;
    @Nullable private String mPayerEmail;

    /**
     * Builds contact details.
     *
     * @param context          The application context.
     * @param profile          The autofill profile where this contact data lives.
     * @param name             The payer name. If not empty, this will be the primary label.
     * @param phone            The phone number. If name is empty, this will be the primary label.
     * @param email            The email address. If name and phone are empty, this will be the
     *                         primary label.
     * @param completionStatus The completion status of this contact.
     * @param requestName      Whether the merchant requests a payer name.
     * @param requestPhone     Whether the merchant requests a payer phone number.
     * @param requestEmail     Whether the merchant requests a payer email address.
     */
    public AutofillContact(
            Context context,
            AutofillProfile profile,
            @Nullable String name,
            @Nullable String phone,
            @Nullable String email,
            @ContactEditor.CompletionStatus int completionStatus,
            boolean requestName,
            boolean requestPhone,
            boolean requestEmail) {
        super(profile.getGUID(), null, null, null, null);
        mContext = context;
        mProfile = profile;
        mRequestName = requestName;
        mRequestPhone = requestPhone;
        mRequestEmail = requestEmail;
        mIsEditable = true;
        setContactInfo(profile.getGUID(), name, phone, email);
        updateCompletionStatus(completionStatus);
    }

    /** @return Payer name. Null if the merchant did not request it or data is incomplete. */
    @Nullable
    public String getPayerName() {
        return mPayerName;
    }

    /** @return Phone number. Null if the merchant did not request it or data is incomplete. */
    @Nullable
    public String getPayerPhone() {
        return mPayerPhone;
    }

    /** @return Email address. Null if the merchant did not request it or data is incomplete. */
    public @Nullable String getPayerEmail() {
        return mPayerEmail;
    }

    /** @return The autofill profile where this contact data lives. */
    public AutofillProfile getProfile() {
        return mProfile;
    }

    /** @return The payer detail information for the merchant. */
    public PayerDetail toPayerDetail() {
        assert mIsComplete;
        PayerDetail result = new PayerDetail();

        result.name = getPayerName();
        result.phone = getPayerPhone();
        result.email = getPayerEmail();

        return result;
    }

    /** @return Whether the contact is complete and ready to be sent to the merchant as-is. */
    @Override
    public boolean isComplete() {
        return mIsComplete;
    }

    /**
     * Updates the profile guid, payer name, email address, and phone number and marks this
     * information "complete." Called after the user has edited this contact information.
     * Update the identifier, label, sublabel, and tertiarylabel.
     *
     * @param guid  The new identifier to use. Should not be null or empty.
     * @param name  The new payer name to use. If not empty, this will be the primary label.
     * @param phone The new phone number to use. If name is empty, this will be the primary label.
     * @param email The new email address to use. If email and phone are empty, this will be the
     *              primary label.
     */
    public void completeContact(
            String guid, @Nullable String name, @Nullable String phone, @Nullable String email) {
        setContactInfo(guid, name, phone, email);
        updateCompletionStatus(ContactEditor.COMPLETE);
    }

    /**
     * Returns whether this contact is equal or a superset of the specified contact considering the
     * information requested by the merchant.
     *
     * @param contact The contact to compare to.
     * @return Whether this contact is equal to or a superset of the other.
     */
    public boolean isEqualOrSupersetOf(AutofillContact contact) {
        assert contact != null;

        // This contact is not equal to or a superset of the other if for a requested field:
        // 1- This contact's field is null and the other's is not.
        // 2- The field values are not equal.
        if (mRequestName) {
            if (mPayerName == null && contact.mPayerName != null) return false;
            if (mPayerName != null
                    && contact.mPayerName != null
                    && !mPayerName.equalsIgnoreCase(contact.mPayerName)) {
                return false;
            }
        }

        if (mRequestPhone) {
            if (mPayerPhone == null && contact.mPayerPhone != null) return false;
            if (mPayerPhone != null
                    && contact.mPayerPhone != null
                    && !TextUtils.equals(mPayerPhone, contact.mPayerPhone)) {
                return false;
            }
        }

        if (mRequestEmail) {
            if (mPayerEmail == null && contact.mPayerEmail != null) return false;
            if (mPayerEmail != null
                    && contact.mPayerEmail != null
                    && !mPayerEmail.equalsIgnoreCase(contact.mPayerEmail)) {
                return false;
            }
        }

        return true;
    }

    /**
     * @return Returns the relevance score of this contact, based on the validity of the information
     * requested by the merchant.
     */
    public int getRelevanceScore() {
        int score = 0;

        if (mRequestName && (mCompletionStatus & ContactEditor.INVALID_NAME) == 0) ++score;
        if (mRequestPhone && (mCompletionStatus & ContactEditor.INVALID_PHONE_NUMBER) == 0) ++score;
        if (mRequestEmail && (mCompletionStatus & ContactEditor.INVALID_EMAIL) == 0) ++score;

        return score;
    }

    private void setContactInfo(
            String guid, @Nullable String name, @Nullable String phone, @Nullable String email) {
        mPayerName = TextUtils.isEmpty(name) ? null : name;
        mPayerPhone = TextUtils.isEmpty(phone) ? null : phone;
        mPayerEmail = TextUtils.isEmpty(email) ? null : email;

        if (mPayerName == null) {
            updateIdentifierAndLabels(
                    guid,
                    mPayerPhone == null ? mPayerEmail : mPayerPhone,
                    mPayerPhone == null ? null : mPayerEmail);
        } else {
            updateIdentifierAndLabels(
                    guid,
                    mPayerName,
                    mPayerPhone == null ? mPayerEmail : mPayerPhone,
                    mPayerPhone == null ? null : mPayerEmail);
        }
    }

    private void updateCompletionStatus(int completionStatus) {
        mCompletionStatus = completionStatus;
        mIsComplete = completionStatus == ContactEditor.COMPLETE;

        switch (completionStatus) {
            case ContactEditor.COMPLETE:
                mEditMessage = null;
                mEditTitle = mContext.getString(R.string.payments_edit_contact_details_label);
                break;
            case ContactEditor.INVALID_NAME:
                mEditMessage = mContext.getString(R.string.payments_name_required);
                mEditTitle = mContext.getString(R.string.payments_add_name);
                break;
            case ContactEditor.INVALID_EMAIL:
                mEditMessage = mContext.getString(R.string.payments_email_required);
                mEditTitle = mContext.getString(R.string.payments_add_email);
                break;
            case ContactEditor.INVALID_PHONE_NUMBER:
                mEditMessage = mContext.getString(R.string.payments_phone_number_required);
                mEditTitle = mContext.getString(R.string.payments_add_phone_number);
                break;
            default:
                // Multiple invalid fields.
                mEditMessage = mContext.getString(R.string.payments_more_information_required);
                mEditTitle = mContext.getString(R.string.payments_add_more_information);
                break;
        }
    }
}
