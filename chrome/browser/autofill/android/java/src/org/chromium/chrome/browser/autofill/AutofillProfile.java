// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.autofill.ServerFieldType;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Autofill address information.
 * The creation and/or modification of an AutofillProfile is assumed to involve the user (e.g.
 * data reviewed by the user in the {@link
 * org.chromium.chrome.browser.autofill.settings.AddressEditor}), therefore all new values gain
 * {@link VerificationStatus.USER_VERIFIED} status.
 */
@JNINamespace("autofill")
public class AutofillProfile {
    private String mGUID;
    private boolean mIsLocal;
    private @Source int mSource;
    private Map<Integer, ValueWithStatus> mFields;
    private String mLabel;
    private String mLanguageCode;

    @VisibleForTesting
    static class ValueWithStatus {
        static final ValueWithStatus EMPTY = new ValueWithStatus("", VerificationStatus.NO_STATUS);

        private final String mValue;
        private final @VerificationStatus int mStatus;

        ValueWithStatus(String value, @VerificationStatus int status) {
            mValue = value;
            mStatus = status;
        }

        String getValue() {
            return mValue;
        }

        @VerificationStatus
        int getStatus() {
            return mStatus;
        }
    }

    /**
     * Builder for the {@link AutofillProfile}.
     */
    public static final class Builder {
        private String mGUID = "";
        private boolean mIsLocal = true;
        private @Source int mSource = Source.LOCAL_OR_SYNCABLE;
        private ValueWithStatus mHonorificPrefix = ValueWithStatus.EMPTY;
        private ValueWithStatus mFullName = ValueWithStatus.EMPTY;
        private ValueWithStatus mCompanyName = ValueWithStatus.EMPTY;
        private ValueWithStatus mStreetAddress = ValueWithStatus.EMPTY;
        private ValueWithStatus mRegion = ValueWithStatus.EMPTY;
        private ValueWithStatus mLocality = ValueWithStatus.EMPTY;
        private ValueWithStatus mDependentLocality = ValueWithStatus.EMPTY;
        private ValueWithStatus mPostalCode = ValueWithStatus.EMPTY;
        private ValueWithStatus mSortingCode = ValueWithStatus.EMPTY;
        private ValueWithStatus mCountryCode = ValueWithStatus.EMPTY;
        private ValueWithStatus mPhoneNumber = ValueWithStatus.EMPTY;
        private ValueWithStatus mEmailAddress = ValueWithStatus.EMPTY;
        private String mLabel = "";
        private String mLanguageCode = "";

        public Builder setGUID(String guid) {
            mGUID = guid;
            return this;
        }

        public Builder setIsLocal(boolean isLocal) {
            mIsLocal = isLocal;
            return this;
        }

        public Builder setSource(@Source int source) {
            mSource = source;
            return this;
        }

        public Builder setHonorificPrefix(String honorificPrefix) {
            mHonorificPrefix =
                    new ValueWithStatus(honorificPrefix, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setHonorificPrefix(String honorificPrefix, @VerificationStatus int status) {
            mHonorificPrefix = new ValueWithStatus(honorificPrefix, status);
            return this;
        }

        public Builder setFullName(String fullName) {
            mFullName = new ValueWithStatus(fullName, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setFullName(String fullName, @VerificationStatus int status) {
            mFullName = new ValueWithStatus(fullName, status);
            return this;
        }

        public Builder setCompanyName(String companyName) {
            mCompanyName = new ValueWithStatus(companyName, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setCompanyName(String companyName, @VerificationStatus int status) {
            mCompanyName = new ValueWithStatus(companyName, status);
            return this;
        }

        public Builder setStreetAddress(String streetAddress) {
            mStreetAddress = new ValueWithStatus(streetAddress, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setStreetAddress(String streetAddress, @VerificationStatus int status) {
            mStreetAddress = new ValueWithStatus(streetAddress, status);
            return this;
        }

        public Builder setRegion(String region) {
            mRegion = new ValueWithStatus(region, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setRegion(String region, @VerificationStatus int status) {
            mRegion = new ValueWithStatus(region, status);
            return this;
        }

        public Builder setLocality(String locality) {
            mLocality = new ValueWithStatus(locality, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setLocality(String locality, @VerificationStatus int status) {
            mLocality = new ValueWithStatus(locality, status);
            return this;
        }

        public Builder setDependentLocality(String dependentLocality) {
            mDependentLocality =
                    new ValueWithStatus(dependentLocality, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setDependentLocality(
                String dependentLocality, @VerificationStatus int status) {
            mDependentLocality = new ValueWithStatus(dependentLocality, status);
            return this;
        }

        public Builder setPostalCode(String postalCode) {
            mPostalCode = new ValueWithStatus(postalCode, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setPostalCode(String postalCode, @VerificationStatus int status) {
            mPostalCode = new ValueWithStatus(postalCode, status);
            return this;
        }

        public Builder setSortingCode(String sortingCode) {
            mSortingCode = new ValueWithStatus(sortingCode, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setSortingCode(String sortingCode, @VerificationStatus int status) {
            mSortingCode = new ValueWithStatus(sortingCode, status);
            return this;
        }

        public Builder setCountryCode(String countryCode) {
            mCountryCode = new ValueWithStatus(countryCode, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setCountryCode(String countryCode, @VerificationStatus int status) {
            mCountryCode = new ValueWithStatus(countryCode, status);
            return this;
        }

        public Builder setPhoneNumber(String phoneNumber) {
            mPhoneNumber = new ValueWithStatus(phoneNumber, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setPhoneNumber(String phoneNumber, @VerificationStatus int status) {
            mPhoneNumber = new ValueWithStatus(phoneNumber, status);
            return this;
        }

        public Builder setEmailAddress(String emailAddress) {
            mEmailAddress = new ValueWithStatus(emailAddress, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setEmailAddress(String emailAddress, @VerificationStatus int status) {
            mEmailAddress = new ValueWithStatus(emailAddress, status);
            return this;
        }

        public Builder setLabel(String label) {
            mLabel = label;
            return this;
        }

        public Builder setLanguageCode(String languageCode) {
            mLanguageCode = languageCode;
            return this;
        }

        public AutofillProfile build() {
            return new AutofillProfile(mGUID, mIsLocal, mSource, mHonorificPrefix, mFullName,
                    mCompanyName, mStreetAddress, mRegion, mLocality, mDependentLocality,
                    mPostalCode, mSortingCode, mCountryCode, mPhoneNumber, mEmailAddress,
                    mLanguageCode);
        }
    }

    public static Builder builder() {
        return new Builder();
    }

    @CalledByNative
    private static AutofillProfile create(String guid, boolean isLocal, @Source int source,
            String honorificPrefix, @VerificationStatus int honorificPrefixStatus, String fullName,
            @VerificationStatus int fullNameStatus, String companyName,
            @VerificationStatus int companyNameStatus, String streetAddress,
            @VerificationStatus int streetAddressStatus, String region,
            @VerificationStatus int regionStatus, String locality,
            @VerificationStatus int localityStatus, String dependentLocality,
            @VerificationStatus int dependentLocalityStatus, String postalCode,
            @VerificationStatus int postalCodeStatus, String sortingCode,
            @VerificationStatus int sortingCodeStatus, String countryCode,
            @VerificationStatus int countryCodeStatus, String phoneNumber,
            @VerificationStatus int phoneNumberStatus, String emailAddress,
            @VerificationStatus int emailAddressStatus, String languageCode) {
        return new AutofillProfile(guid, isLocal, source,
                new ValueWithStatus(honorificPrefix, honorificPrefixStatus),
                new ValueWithStatus(fullName, fullNameStatus),
                new ValueWithStatus(companyName, companyNameStatus),
                new ValueWithStatus(streetAddress, streetAddressStatus),
                new ValueWithStatus(region, regionStatus),
                new ValueWithStatus(locality, localityStatus),
                new ValueWithStatus(dependentLocality, dependentLocalityStatus),
                new ValueWithStatus(postalCode, postalCodeStatus),
                new ValueWithStatus(sortingCode, sortingCodeStatus),
                new ValueWithStatus(countryCode, countryCodeStatus),
                new ValueWithStatus(phoneNumber, phoneNumberStatus),
                new ValueWithStatus(emailAddress, emailAddressStatus), languageCode);
    }

    private AutofillProfile(String guid, boolean isLocal, @Source int source,
            ValueWithStatus honorificPrefix, ValueWithStatus fullName, ValueWithStatus companyName,
            ValueWithStatus streetAddress, ValueWithStatus region, ValueWithStatus locality,
            ValueWithStatus dependentLocality, ValueWithStatus postalCode,
            ValueWithStatus sortingCode, ValueWithStatus countryCode, ValueWithStatus phoneNumber,
            ValueWithStatus emailAddress, String languageCode) {
        mGUID = guid;
        mIsLocal = isLocal;
        mSource = source;

        mFields = new HashMap<>();
        mFields.put(ServerFieldType.NAME_HONORIFIC_PREFIX, honorificPrefix);
        mFields.put(ServerFieldType.NAME_FULL, fullName);
        mFields.put(ServerFieldType.COMPANY_NAME, companyName);
        mFields.put(ServerFieldType.ADDRESS_HOME_STREET_ADDRESS, streetAddress);
        mFields.put(ServerFieldType.ADDRESS_HOME_STATE, region);
        mFields.put(ServerFieldType.ADDRESS_HOME_CITY, locality);
        mFields.put(ServerFieldType.ADDRESS_HOME_DEPENDENT_LOCALITY, dependentLocality);
        mFields.put(ServerFieldType.ADDRESS_HOME_ZIP, postalCode);
        mFields.put(ServerFieldType.ADDRESS_HOME_SORTING_CODE, sortingCode);
        mFields.put(ServerFieldType.ADDRESS_HOME_COUNTRY, countryCode);
        mFields.put(ServerFieldType.PHONE_HOME_WHOLE_NUMBER, phoneNumber);
        mFields.put(ServerFieldType.EMAIL_ADDRESS, emailAddress);

        mLanguageCode = languageCode;
    }

    /* Builds an AutofillProfile that is an exact copy of the one passed as parameter. */
    public AutofillProfile(AutofillProfile profile) {
        mGUID = profile.getGUID();
        mIsLocal = profile.getIsLocal();
        mSource = profile.getSource();

        mFields = new HashMap<>(profile.mFields);

        mLanguageCode = profile.getLanguageCode();
        mLabel = profile.getLabel();
    }

    @CalledByNative
    public String getGUID() {
        return mGUID;
    }

    @CalledByNative
    public @Source int getSource() {
        return mSource;
    }

    public String getLabel() {
        return mLabel;
    }

    public void resetDynamicFields() {
        Set<Integer> staticFields = new HashSet<>(AutofillProfileBridge.getStaticEditorFields());
        mFields.keySet().removeIf(key -> !staticFields.contains(key));
    }

    @CalledByNative
    public String getInfo(@ServerFieldType int fieldType) {
        if (!mFields.containsKey(fieldType)) {
            return "";
        }
        return mFields.get(fieldType).getValue();
    }

    @CalledByNative
    public @VerificationStatus int getInfoStatus(@ServerFieldType int fieldType) {
        if (!mFields.containsKey(fieldType)) {
            return VerificationStatus.NO_STATUS;
        }
        return mFields.get(fieldType).getStatus();
    }

    public String getHonorificPrefix() {
        return getInfo(ServerFieldType.NAME_HONORIFIC_PREFIX);
    }

    private @VerificationStatus int getHonorificPrefixStatus() {
        return getInfoStatus(ServerFieldType.NAME_HONORIFIC_PREFIX);
    }

    public String getFullName() {
        return getInfo(ServerFieldType.NAME_FULL);
    }

    @VisibleForTesting
    @VerificationStatus
    int getFullNameStatus() {
        return getInfoStatus(ServerFieldType.NAME_FULL);
    }

    public String getCompanyName() {
        return getInfo(ServerFieldType.COMPANY_NAME);
    }

    @VerificationStatus
    int getCompanyNameStatus() {
        return getInfoStatus(ServerFieldType.COMPANY_NAME);
    }

    public String getStreetAddress() {
        return getInfo(ServerFieldType.ADDRESS_HOME_STREET_ADDRESS);
    }

    @VisibleForTesting
    @VerificationStatus
    int getStreetAddressStatus() {
        return getInfoStatus(ServerFieldType.ADDRESS_HOME_STREET_ADDRESS);
    }

    public String getRegion() {
        return getInfo(ServerFieldType.ADDRESS_HOME_STATE);
    }

    @VisibleForTesting
    @VerificationStatus
    int getRegionStatus() {
        return getInfoStatus(ServerFieldType.ADDRESS_HOME_STATE);
    }

    public String getLocality() {
        return getInfo(ServerFieldType.ADDRESS_HOME_CITY);
    }

    @VisibleForTesting
    @VerificationStatus
    int getLocalityStatus() {
        return getInfoStatus(ServerFieldType.ADDRESS_HOME_CITY);
    }

    public String getDependentLocality() {
        return getInfo(ServerFieldType.ADDRESS_HOME_DEPENDENT_LOCALITY);
    }

    private @VerificationStatus int getDependentLocalityStatus() {
        return getInfoStatus(ServerFieldType.ADDRESS_HOME_DEPENDENT_LOCALITY);
    }

    public String getPostalCode() {
        return getInfo(ServerFieldType.ADDRESS_HOME_ZIP);
    }

    @VisibleForTesting
    @VerificationStatus
    int getPostalCodeStatus() {
        return getInfoStatus(ServerFieldType.ADDRESS_HOME_ZIP);
    }

    public String getSortingCode() {
        return getInfo(ServerFieldType.ADDRESS_HOME_SORTING_CODE);
    }

    private @VerificationStatus int getSortingCodeStatus() {
        return getInfoStatus(ServerFieldType.ADDRESS_HOME_SORTING_CODE);
    }

    public String getCountryCode() {
        return getInfo(ServerFieldType.ADDRESS_HOME_COUNTRY);
    }

    private @VerificationStatus int getCountryCodeStatus() {
        return getInfoStatus(ServerFieldType.ADDRESS_HOME_COUNTRY);
    }

    public String getPhoneNumber() {
        return getInfo(ServerFieldType.PHONE_HOME_WHOLE_NUMBER);
    }

    private @VerificationStatus int getPhoneNumberStatus() {
        return getInfoStatus(ServerFieldType.PHONE_HOME_WHOLE_NUMBER);
    }

    public String getEmailAddress() {
        return getInfo(ServerFieldType.EMAIL_ADDRESS);
    }

    private @VerificationStatus int getEmailAddressStatus() {
        return getInfoStatus(ServerFieldType.EMAIL_ADDRESS);
    }

    @CalledByNative
    public String getLanguageCode() {
        return mLanguageCode;
    }

    public boolean getIsLocal() {
        return mIsLocal;
    }

    public void setGUID(String guid) {
        mGUID = guid;
    }

    public void setLabel(String label) {
        mLabel = label;
    }

    public void setSource(@Source int source) {
        mSource = source;
    }

    public void setInfo(@ServerFieldType int fieldType, @Nullable String value) {
        value = value == null ? "" : value;
        mFields.put(fieldType, new ValueWithStatus(value, VerificationStatus.USER_VERIFIED));
    }

    public void setHonorificPrefix(String honorificPrefix) {
        setInfo(ServerFieldType.NAME_HONORIFIC_PREFIX, honorificPrefix);
    }

    public void setFullName(String fullName) {
        setInfo(ServerFieldType.NAME_FULL, fullName);
    }

    public void setCompanyName(String companyName) {
        setInfo(ServerFieldType.COMPANY_NAME, companyName);
    }

    public void setStreetAddress(String streetAddress) {
        setInfo(ServerFieldType.ADDRESS_HOME_STREET_ADDRESS, streetAddress);
    }

    public void setRegion(String region) {
        setInfo(ServerFieldType.ADDRESS_HOME_STATE, region);
    }

    public void setLocality(String locality) {
        setInfo(ServerFieldType.ADDRESS_HOME_CITY, locality);
    }

    public void setDependentLocality(String dependentLocality) {
        setInfo(ServerFieldType.ADDRESS_HOME_DEPENDENT_LOCALITY, dependentLocality);
    }

    public void setPostalCode(String postalCode) {
        setInfo(ServerFieldType.ADDRESS_HOME_ZIP, postalCode);
    }

    public void setSortingCode(String sortingCode) {
        setInfo(ServerFieldType.ADDRESS_HOME_SORTING_CODE, sortingCode);
    }

    public void setCountryCode(String countryCode) {
        setInfo(ServerFieldType.ADDRESS_HOME_COUNTRY, countryCode);
    }

    public void setPhoneNumber(String phoneNumber) {
        setInfo(ServerFieldType.PHONE_HOME_WHOLE_NUMBER, phoneNumber);
    }

    public void setEmailAddress(String emailAddress) {
        setInfo(ServerFieldType.EMAIL_ADDRESS, emailAddress);
    }

    public void setLanguageCode(String languageCode) {
        mLanguageCode = languageCode;
    }

    public void setIsLocal(boolean isLocal) {
        mIsLocal = isLocal;
    }

    /** Used by ArrayAdapter in credit card settings. */
    @Override
    public String toString() {
        return mLabel;
    }
}
