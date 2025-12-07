// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.telephony.PhoneNumberUtils;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.EditableOption;
import org.chromium.components.autofill.FieldType;
import org.chromium.payments.mojom.PaymentAddress;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.regex.Pattern;

/** The locally stored autofill address. */
@NullMarked
public class AutofillAddress extends EditableOption {
    /** The pattern for a valid region code. */
    private static final String REGION_CODE_PATTERN = "^[A-Z]{2}$";

    // Bit field values are identical to ProfileFields in payments_profile_comparator.h.
    @IntDef({
        CompletionStatus.COMPLETE,
        CompletionStatus.INVALID_RECIPIENT,
        CompletionStatus.INVALID_PHONE_NUMBER,
        CompletionStatus.INVALID_ADDRESS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CompletionStatus {
        /** Can be sent to the merchant as-is without editing first. */
        int COMPLETE = 0;

        /** The recipient is missing. */
        int INVALID_RECIPIENT = 1 << 0;

        /** The phone number is invalid or missing. */
        int INVALID_PHONE_NUMBER = 1 << 1;

        /**
         * The address is invalid. For example, missing state or city name. To have consistent bit
         * field values between Android and Desktop 1 << 2 is reserved for email address.
         */
        int INVALID_ADDRESS = 1 << 3;

        int MAX_VALUE = 1 << 4;
    }

    private @Nullable static Pattern sRegionCodePattern;

    private final Context mContext;
    private final PersonalDataManager mPersonalDataManager;
    private AutofillProfile mProfile;
    private @Nullable String mShippingLabelWithCountry;
    private @Nullable String mShippingLabelWithoutCountry;

    /**
     * Builds the autofill address.
     *
     * @param context The context where this address was created.
     * @param profile The autofill profile containing the address information.
     */
    public AutofillAddress(
            Context context, AutofillProfile profile, PersonalDataManager personalDataManager) {
        super(
                profile.getGUID(),
                profile.getInfo(FieldType.NAME_FULL),
                profile.getLabel(),
                profile.getInfo(FieldType.PHONE_HOME_WHOLE_NUMBER),
                null);
        mContext = context;
        mProfile = profile;
        mPersonalDataManager = personalDataManager;
        mIsEditable = true;
        checkAndUpdateAddressCompleteness();
    }

    /** @return The autofill profile where this address data lives. */
    public AutofillProfile getProfile() {
        return mProfile;
    }

    /**
     * Updates the address and marks it "complete".
     * Called after the user has edited this address.
     * Updates the identifier and labels.
     *
     * @param profile The new profile to use.
     */
    public void completeAddress(AutofillProfile profile) {
        updateAddress(profile);
        assert mIsComplete;
    }

    /**
     * Updates the address and its completeness if needed.
     * Called after the user has edited this address.
     * Updates the identifier and labels.
     *
     * @param profile The new profile to use.
     */
    public void updateAddress(AutofillProfile profile) {
        // Since the profile changed, our cached labels are now out of date. Set them to null so the
        // labels are recomputed next time they are needed.
        mShippingLabelWithCountry = null;
        mShippingLabelWithoutCountry = null;

        mProfile = profile;
        updateIdentifierAndLabels(
                mProfile.getGUID(),
                mProfile.getInfo(FieldType.NAME_FULL),
                mProfile.getLabel(),
                mProfile.getInfo(FieldType.PHONE_HOME_WHOLE_NUMBER));
        checkAndUpdateAddressCompleteness();
    }

    /**
     * Gets the shipping address label which includes the country for the profile associated with
     * this address and sets it as sublabel for this EditableOption.
     */
    public void setShippingAddressLabelWithCountry() {
        assert mProfile != null;

        if (mShippingLabelWithCountry == null) {
            mShippingLabelWithCountry =
                    mPersonalDataManager.getShippingAddressLabelWithCountryForPaymentRequest(
                            mProfile);
        }

        mProfile.setLabel(mShippingLabelWithCountry);
        updateSublabel(assumeNonNull(mProfile.getLabel()));
    }

    /**
     * Gets the shipping address label which does not include the country for the profile associated
     * with this address and sets it as sublabel for this EditableOption.
     */
    public void setShippingAddressLabelWithoutCountry() {
        assert mProfile != null;

        if (mShippingLabelWithoutCountry == null) {
            mShippingLabelWithoutCountry =
                    mPersonalDataManager.getShippingAddressLabelWithoutCountryForPaymentRequest(
                            mProfile);
        }

        mProfile.setLabel(mShippingLabelWithoutCountry);
        updateSublabel(assumeNonNull(mProfile.getLabel()));
    }

    /**
     * Checks whether this address is complete and updates edit message, edit title and complete
     * status.
     */
    private void checkAndUpdateAddressCompleteness() {
        Pair<Integer, Integer> messageResIds =
                getEditMessageAndTitleResIds(
                        checkAddressCompletionStatus(mProfile, mPersonalDataManager));

        mEditMessage =
                messageResIds.first.intValue() == 0
                        ? null
                        : mContext.getString(messageResIds.first);
        mEditTitle =
                messageResIds.second.intValue() == 0
                        ? null
                        : mContext.getString(messageResIds.second);
        mIsComplete = mEditMessage == null;
        mCompletenessScore = calculateCompletenessScore();
    }

    /**
     * Gets the edit message and title resource Ids for the completion status.
     *
     * @param  completionStatus The completion status.
     * @return The resource Ids. The first is the edit message resource Id. The second is the
     *         correspond editor title resource Id.
     */
    public static Pair<Integer, Integer> getEditMessageAndTitleResIds(
            @CompletionStatus int completionStatus) {
        int editMessageResId = 0;
        int editTitleResId = 0;

        switch (completionStatus) {
            case CompletionStatus.COMPLETE:
                editTitleResId = R.string.payments_edit_address;
                break;
            case CompletionStatus.INVALID_RECIPIENT:
                editMessageResId = R.string.payments_recipient_required;
                editTitleResId = R.string.payments_add_recipient;
                break;
            case CompletionStatus.INVALID_PHONE_NUMBER:
                editMessageResId = R.string.payments_phone_number_required;
                editTitleResId = R.string.payments_add_phone_number;
                break;
            case CompletionStatus.INVALID_ADDRESS:
                editMessageResId = R.string.payments_invalid_address;
                editTitleResId = R.string.payments_add_valid_address;
                break;
            default:
                // Multiple bits are set.
                assert completionStatus < CompletionStatus.MAX_VALUE : "Invalid completion status";
                editMessageResId = R.string.payments_more_information_required;
                editTitleResId = R.string.payments_add_more_information;
        }

        return new Pair<>(editMessageResId, editTitleResId);
    }

    /**
     * Checks address completion status in the given profile.
     *
     * <p>If the country code is not set or invalid, but all fields for the default locale's country
     * code are present, then the profile is deemed "complete." AutoflllAddress.toPaymentAddress()
     * will use the default locale to fill in a blank country code before sending the address to the
     * renderer.
     *
     * @param profile The autofill profile containing the address information.
     * @return int The completion status.
     */
    public static @CompletionStatus int checkAddressCompletionStatus(
            AutofillProfile profile, PersonalDataManager personalDataManager) {
        @CompletionStatus int completionStatus = CompletionStatus.COMPLETE;

        if (TextUtils.isEmpty(profile.getInfo(FieldType.NAME_FULL))) {
            completionStatus |= CompletionStatus.INVALID_RECIPIENT;
        }

        if (!PhoneNumberUtils.isGlobalPhoneNumber(
                PhoneNumberUtils.stripSeparators(
                        profile.getInfo(FieldType.PHONE_HOME_WHOLE_NUMBER).toString()))) {
            completionStatus |= CompletionStatus.INVALID_PHONE_NUMBER;
        }

        for (int fieldId :
                AutofillProfileBridge.getRequiredAddressFields(
                        AutofillAddress.getCountryCode(profile, personalDataManager))) {
            if (fieldId == FieldType.NAME_FULL || fieldId == FieldType.ADDRESS_HOME_COUNTRY) {
                continue;
            }
            if (!TextUtils.isEmpty(profile.getInfo(fieldId))) continue;
            completionStatus |= CompletionStatus.INVALID_ADDRESS;
            break;
        }

        return completionStatus;
    }

    /**
     * @return The country code to use, e.g., when constructing an editor for this address.
     */
    public static String getCountryCode(
            @Nullable AutofillProfile profile, PersonalDataManager personalDataManager) {
        if (sRegionCodePattern == null) {
            sRegionCodePattern = Pattern.compile(REGION_CODE_PATTERN);
        }
        if (profile == null) {
            return personalDataManager.getDefaultCountryCodeForNewAddress();
        }
        final String countryCode = profile.getInfo(FieldType.ADDRESS_HOME_COUNTRY);
        return TextUtils.isEmpty(countryCode) || !sRegionCodePattern.matcher(countryCode).matches()
                ? personalDataManager.getDefaultCountryCodeForNewAddress()
                : countryCode;
    }

    /** @return The address for the merchant. */
    public PaymentAddress toPaymentAddress() {
        assert mIsComplete;
        PaymentAddress result = new PaymentAddress();

        result.country = getCountryCode(mProfile, mPersonalDataManager);
        result.addressLine = mProfile.getInfo(FieldType.ADDRESS_HOME_STREET_ADDRESS).split("\n");
        result.region = mProfile.getInfo(FieldType.ADDRESS_HOME_STATE);
        result.city = mProfile.getInfo(FieldType.ADDRESS_HOME_CITY);
        result.dependentLocality = mProfile.getInfo(FieldType.ADDRESS_HOME_DEPENDENT_LOCALITY);
        result.postalCode = mProfile.getInfo(FieldType.ADDRESS_HOME_ZIP);
        result.sortingCode = mProfile.getInfo(FieldType.ADDRESS_HOME_SORTING_CODE);
        result.organization = mProfile.getInfo(FieldType.COMPANY_NAME);
        result.recipient = mProfile.getInfo(FieldType.NAME_FULL);
        result.phone = mProfile.getInfo(FieldType.PHONE_HOME_WHOLE_NUMBER);

        return result;
    }

    private int calculateCompletenessScore() {
        int missingFields = checkAddressCompletionStatus(mProfile, mPersonalDataManager);

        // Count how many are set. The completeness of the address is weighted so as
        // to dominate the other fields.
        int completenessScore = 0;
        if ((missingFields & CompletionStatus.INVALID_RECIPIENT) == 0) completenessScore++;
        if ((missingFields & CompletionStatus.INVALID_PHONE_NUMBER) == 0) completenessScore++;
        if ((missingFields & CompletionStatus.INVALID_ADDRESS) == 0) completenessScore += 10;
        return completenessScore;
    }
}
