// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

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
    private ValueWithStatus mHonorificPrefix;
    private ValueWithStatus mFullName;
    private ValueWithStatus mCompanyName;
    private ValueWithStatus mStreetAddress;
    private ValueWithStatus mRegion;
    private ValueWithStatus mLocality;
    private ValueWithStatus mDependentLocality;
    private ValueWithStatus mPostalCode;
    private ValueWithStatus mSortingCode;
    private ValueWithStatus mCountryCode;
    private ValueWithStatus mPhoneNumber;
    private ValueWithStatus mEmailAddress;
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

    // TODO(crbug/1408117): remove duplicate constructors when the source is unnecessary.
    private AutofillProfile(String guid, boolean isLocal, @Source int source,
            ValueWithStatus honorificPrefix, ValueWithStatus fullName, ValueWithStatus companyName,
            ValueWithStatus streetAddress, ValueWithStatus region, ValueWithStatus locality,
            ValueWithStatus dependentLocality, ValueWithStatus postalCode,
            ValueWithStatus sortingCode, ValueWithStatus countryCode, ValueWithStatus phoneNumber,
            ValueWithStatus emailAddress, String languageCode) {
        mGUID = guid;
        mIsLocal = isLocal;
        mSource = source;
        mHonorificPrefix = honorificPrefix;
        mFullName = fullName;
        mCompanyName = companyName;
        mStreetAddress = streetAddress;
        mRegion = region;
        mLocality = locality;
        mDependentLocality = dependentLocality;
        mPostalCode = postalCode;
        mSortingCode = sortingCode;
        mCountryCode = countryCode;
        mPhoneNumber = phoneNumber;
        mEmailAddress = emailAddress;
        mLanguageCode = languageCode;
    }

    /* Builds an AutofillProfile that is an exact copy of the one passed as parameter. */
    public AutofillProfile(AutofillProfile profile) {
        mGUID = profile.getGUID();
        mIsLocal = profile.getIsLocal();
        mSource = profile.getSource();
        mHonorificPrefix = new ValueWithStatus(
                profile.getHonorificPrefix(), profile.getHonorificPrefixStatus());
        mFullName = new ValueWithStatus(profile.getFullName(), profile.getFullNameStatus());
        mCompanyName =
                new ValueWithStatus(profile.getCompanyName(), profile.getCompanyNameStatus());
        mStreetAddress =
                new ValueWithStatus(profile.getStreetAddress(), profile.getStreetAddressStatus());
        mRegion = new ValueWithStatus(profile.getRegion(), profile.getRegionStatus());
        mLocality = new ValueWithStatus(profile.getLocality(), profile.getLocalityStatus());
        mDependentLocality = new ValueWithStatus(
                profile.getDependentLocality(), profile.getDependentLocalityStatus());
        mPostalCode = new ValueWithStatus(profile.getPostalCode(), profile.getPostalCodeStatus());
        mSortingCode =
                new ValueWithStatus(profile.getSortingCode(), profile.getSortingCodeStatus());
        mCountryCode =
                new ValueWithStatus(profile.getCountryCode(), profile.getCountryCodeStatus());
        mPhoneNumber =
                new ValueWithStatus(profile.getPhoneNumber(), profile.getPhoneNumberStatus());
        mEmailAddress =
                new ValueWithStatus(profile.getEmailAddress(), profile.getEmailAddressStatus());
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

    @CalledByNative
    public String getHonorificPrefix() {
        return mHonorificPrefix.getValue();
    }

    @CalledByNative
    private @VerificationStatus int getHonorificPrefixStatus() {
        return mHonorificPrefix.getStatus();
    }

    @CalledByNative
    public String getFullName() {
        return mFullName.getValue();
    }

    @CalledByNative
    @VisibleForTesting
    @VerificationStatus
    int getFullNameStatus() {
        return mFullName.getStatus();
    }

    @CalledByNative
    public String getCompanyName() {
        return mCompanyName.getValue();
    }

    @CalledByNative
    @VerificationStatus
    int getCompanyNameStatus() {
        return mCompanyName.getStatus();
    }

    @CalledByNative
    public String getStreetAddress() {
        return mStreetAddress.getValue();
    }

    @CalledByNative
    @VisibleForTesting
    @VerificationStatus
    int getStreetAddressStatus() {
        return mStreetAddress.getStatus();
    }

    @CalledByNative
    public String getRegion() {
        return mRegion.getValue();
    }

    @CalledByNative
    @VisibleForTesting
    @VerificationStatus
    int getRegionStatus() {
        return mRegion.getStatus();
    }

    @CalledByNative
    public String getLocality() {
        return mLocality.getValue();
    }

    @CalledByNative
    @VisibleForTesting
    @VerificationStatus
    int getLocalityStatus() {
        return mLocality.getStatus();
    }

    @CalledByNative
    public String getDependentLocality() {
        return mDependentLocality.getValue();
    }

    @CalledByNative
    private @VerificationStatus int getDependentLocalityStatus() {
        return mDependentLocality.getStatus();
    }

    public String getLabel() {
        return mLabel;
    }

    @CalledByNative
    public String getPostalCode() {
        return mPostalCode.getValue();
    }

    @CalledByNative
    @VisibleForTesting
    @VerificationStatus
    int getPostalCodeStatus() {
        return mPostalCode.getStatus();
    }

    @CalledByNative
    public String getSortingCode() {
        return mSortingCode.getValue();
    }

    @CalledByNative
    private @VerificationStatus int getSortingCodeStatus() {
        return mSortingCode.getStatus();
    }

    @CalledByNative
    public String getCountryCode() {
        return mCountryCode.getValue();
    }

    @CalledByNative
    private @VerificationStatus int getCountryCodeStatus() {
        return mCountryCode.getStatus();
    }

    @CalledByNative
    public String getPhoneNumber() {
        return mPhoneNumber.getValue();
    }

    @CalledByNative
    private @VerificationStatus int getPhoneNumberStatus() {
        return mPhoneNumber.getStatus();
    }

    @CalledByNative
    public String getEmailAddress() {
        return mEmailAddress.getValue();
    }

    @CalledByNative
    private @VerificationStatus int getEmailAddressStatus() {
        return mEmailAddress.getStatus();
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

    public void setHonorificPrefix(String honorificPrefix) {
        mHonorificPrefix = new ValueWithStatus(honorificPrefix, VerificationStatus.USER_VERIFIED);
    }

    public void setFullName(String fullName) {
        mFullName = new ValueWithStatus(fullName, VerificationStatus.USER_VERIFIED);
    }

    public void setCompanyName(String companyName) {
        mCompanyName = new ValueWithStatus(companyName, VerificationStatus.USER_VERIFIED);
    }

    public void setStreetAddress(String streetAddress) {
        mStreetAddress = new ValueWithStatus(streetAddress, VerificationStatus.USER_VERIFIED);
    }

    public void setRegion(String region) {
        mRegion = new ValueWithStatus(region, VerificationStatus.USER_VERIFIED);
    }

    public void setLocality(String locality) {
        mLocality = new ValueWithStatus(locality, VerificationStatus.USER_VERIFIED);
    }

    public void setDependentLocality(String dependentLocality) {
        mDependentLocality =
                new ValueWithStatus(dependentLocality, VerificationStatus.USER_VERIFIED);
    }

    public void setPostalCode(String postalCode) {
        mPostalCode = new ValueWithStatus(postalCode, VerificationStatus.USER_VERIFIED);
    }

    public void setSortingCode(String sortingCode) {
        mSortingCode = new ValueWithStatus(sortingCode, VerificationStatus.USER_VERIFIED);
    }

    public void setCountryCode(String countryCode) {
        mCountryCode = new ValueWithStatus(countryCode, VerificationStatus.USER_VERIFIED);
    }

    public void setPhoneNumber(String phoneNumber) {
        mPhoneNumber = new ValueWithStatus(phoneNumber, VerificationStatus.USER_VERIFIED);
    }

    public void setEmailAddress(String emailAddress) {
        mEmailAddress = new ValueWithStatus(emailAddress, VerificationStatus.USER_VERIFIED);
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
