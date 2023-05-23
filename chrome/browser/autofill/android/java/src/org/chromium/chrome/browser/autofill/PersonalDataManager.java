// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Android wrapper of the PersonalDataManager which provides access from the Java
 * layer.
 *
 * Only usable from the UI thread as it's primary purpose is for supporting the Android
 * preferences UI.
 *
 * See chrome/browser/autofill/personal_data_manager.h for more details.
 */
@JNINamespace("autofill")
public class PersonalDataManager {
    private static final String TAG = "PersonalDataManager";

    /**
     * Observer of PersonalDataManager events.
     */
    public interface PersonalDataManagerObserver {
        /**
         * Called when the data is changed.
         */
        void onPersonalDataChanged();
    }

    /**
     * Callback for subKeys request.
     */
    public interface GetSubKeysRequestDelegate {
        /**
         * Called when the subkeys are received sucessfully.
         * Here the subkeys are admin areas.
         *
         * @param subKeysCodes The subkeys' codes.
         * @param subKeysNames The subkeys' names.
         */
        @CalledByNative("GetSubKeysRequestDelegate")
        void onSubKeysReceived(String[] subKeysCodes, String[] subKeysNames);
    }

    /**
     * Callback for normalized addresses.
     */
    public interface NormalizedAddressRequestDelegate {
        /**
         * Called when the address has been sucessfully normalized.
         *
         * @param profile The profile with the normalized address.
         */
        @CalledByNative("NormalizedAddressRequestDelegate")
        void onAddressNormalized(AutofillProfile profile);

        /**
         * Called when the address could not be normalized.
         *
         * @param profile The non normalized profile.
         */
        @CalledByNative("NormalizedAddressRequestDelegate")
        void onCouldNotNormalize(AutofillProfile profile);
    }

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
     * Autofill address information.
     * The creation and/or modification of an AutofillProfile is assumed to involve the user (e.g.
     * data reviewed by the user in the {@link
     * org.chromium.chrome.browser.autofill.settings.AddressEditor}), therefore all new values gain
     * {@link VerificationStatus.USER_VERIFIED} status.
     */
    public static class AutofillProfile {
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

            public Builder setHonorificPrefix(
                    String honorificPrefix, @VerificationStatus int status) {
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
                mStreetAddress =
                        new ValueWithStatus(streetAddress, VerificationStatus.USER_VERIFIED);
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

        @CalledByNative("AutofillProfile")
        private static AutofillProfile create(String guid, boolean isLocal, @Source int source,
                String honorificPrefix, @VerificationStatus int honorificPrefixStatus,
                String fullName, @VerificationStatus int fullNameStatus, String companyName,
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
                ValueWithStatus honorificPrefix, ValueWithStatus fullName,
                ValueWithStatus companyName, ValueWithStatus streetAddress, ValueWithStatus region,
                ValueWithStatus locality, ValueWithStatus dependentLocality,
                ValueWithStatus postalCode, ValueWithStatus sortingCode,
                ValueWithStatus countryCode, ValueWithStatus phoneNumber,
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
            mStreetAddress = new ValueWithStatus(
                    profile.getStreetAddress(), profile.getStreetAddressStatus());
            mRegion = new ValueWithStatus(profile.getRegion(), profile.getRegionStatus());
            mLocality = new ValueWithStatus(profile.getLocality(), profile.getLocalityStatus());
            mDependentLocality = new ValueWithStatus(
                    profile.getDependentLocality(), profile.getDependentLocalityStatus());
            mPostalCode =
                    new ValueWithStatus(profile.getPostalCode(), profile.getPostalCodeStatus());
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

        @CalledByNative("AutofillProfile")
        public String getGUID() {
            return mGUID;
        }

        @CalledByNative("AutofillProfile")
        public @Source int getSource() {
            return mSource;
        }

        @CalledByNative("AutofillProfile")
        public String getHonorificPrefix() {
            return mHonorificPrefix.getValue();
        }

        @CalledByNative("AutofillProfile")
        private @VerificationStatus int getHonorificPrefixStatus() {
            return mHonorificPrefix.getStatus();
        }

        @CalledByNative("AutofillProfile")
        public String getFullName() {
            return mFullName.getValue();
        }

        @CalledByNative("AutofillProfile")
        @VisibleForTesting
        @VerificationStatus
        int getFullNameStatus() {
            return mFullName.getStatus();
        }

        @CalledByNative("AutofillProfile")
        public String getCompanyName() {
            return mCompanyName.getValue();
        }

        @CalledByNative("AutofillProfile")
        @VerificationStatus
        int getCompanyNameStatus() {
            return mCompanyName.getStatus();
        }

        @CalledByNative("AutofillProfile")
        public String getStreetAddress() {
            return mStreetAddress.getValue();
        }

        @CalledByNative("AutofillProfile")
        @VisibleForTesting
        @VerificationStatus
        int getStreetAddressStatus() {
            return mStreetAddress.getStatus();
        }

        @CalledByNative("AutofillProfile")
        public String getRegion() {
            return mRegion.getValue();
        }

        @CalledByNative("AutofillProfile")
        @VisibleForTesting
        @VerificationStatus
        int getRegionStatus() {
            return mRegion.getStatus();
        }

        @CalledByNative("AutofillProfile")
        public String getLocality() {
            return mLocality.getValue();
        }

        @CalledByNative("AutofillProfile")
        @VisibleForTesting
        @VerificationStatus
        int getLocalityStatus() {
            return mLocality.getStatus();
        }

        @CalledByNative("AutofillProfile")
        public String getDependentLocality() {
            return mDependentLocality.getValue();
        }

        @CalledByNative("AutofillProfile")
        private @VerificationStatus int getDependentLocalityStatus() {
            return mDependentLocality.getStatus();
        }

        public String getLabel() {
            return mLabel;
        }

        @CalledByNative("AutofillProfile")
        public String getPostalCode() {
            return mPostalCode.getValue();
        }

        @CalledByNative("AutofillProfile")
        @VisibleForTesting
        @VerificationStatus
        int getPostalCodeStatus() {
            return mPostalCode.getStatus();
        }

        @CalledByNative("AutofillProfile")
        public String getSortingCode() {
            return mSortingCode.getValue();
        }

        @CalledByNative("AutofillProfile")
        private @VerificationStatus int getSortingCodeStatus() {
            return mSortingCode.getStatus();
        }

        @CalledByNative("AutofillProfile")
        public String getCountryCode() {
            return mCountryCode.getValue();
        }

        @CalledByNative("AutofillProfile")
        private @VerificationStatus int getCountryCodeStatus() {
            return mCountryCode.getStatus();
        }

        @CalledByNative("AutofillProfile")
        public String getPhoneNumber() {
            return mPhoneNumber.getValue();
        }

        @CalledByNative("AutofillProfile")
        private @VerificationStatus int getPhoneNumberStatus() {
            return mPhoneNumber.getStatus();
        }

        @CalledByNative("AutofillProfile")
        public String getEmailAddress() {
            return mEmailAddress.getValue();
        }

        @CalledByNative("AutofillProfile")
        private @VerificationStatus int getEmailAddressStatus() {
            return mEmailAddress.getStatus();
        }

        @CalledByNative("AutofillProfile")
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
            mHonorificPrefix =
                    new ValueWithStatus(honorificPrefix, VerificationStatus.USER_VERIFIED);
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

    /**
     * Autofill credit card information.
     */
    public static class CreditCard {
        // Note that while some of these fields are numbers, they're predominantly read,
        // marshaled and compared as strings. To save conversions, we sometimes use strings.
        private String mGUID;
        private String mOrigin;
        private boolean mIsLocal;
        private boolean mIsCached;
        private boolean mIsVirtual;
        private String mName;
        private String mNumber;
        private String mNetworkAndLastFourDigits;
        private String mMonth;
        private String mYear;
        private String mBasicCardIssuerNetwork;
        private int mIssuerIconDrawableId;
        private String mBillingAddressId;
        private final String mServerId;
        private final long mInstrumentId;
        // The label set for the card. This could be one of Card Network + LastFour, Nickname +
        // LastFour or a Google specific string for Google-issued cards. This is used for displaying
        // the card in PaymentMethods in Settings.
        private String mCardLabel;
        private String mNickname;
        private GURL mCardArtUrl;
        private final @VirtualCardEnrollmentState int mVirtualCardEnrollmentState;
        private final String mProductDescription;
        private final String mCardNameForAutofillDisplay;
        private final String mObfuscatedLastFourDigits;

        @CalledByNative("CreditCard")
        public static CreditCard create(String guid, String origin, boolean isLocal,
                boolean isCached, boolean isVirtual, String name, String number,
                String networkAndLastFourDigits, String month, String year,
                String basicCardIssuerNetwork, int iconId, String billingAddressId, String serverId,
                long instrumentId, String cardLabel, String nickname, GURL cardArtUrl,
                @VirtualCardEnrollmentState int virtualCardEnrollmentState,
                String productDescription, String cardNameForAutofillDisplay,
                String obfuscatedLastFourDigits) {
            return new CreditCard(guid, origin, isLocal, isCached, isVirtual, name, number,
                    networkAndLastFourDigits, month, year, basicCardIssuerNetwork, iconId,
                    billingAddressId, serverId, instrumentId, cardLabel, nickname, cardArtUrl,
                    virtualCardEnrollmentState, productDescription, cardNameForAutofillDisplay,
                    obfuscatedLastFourDigits);
        }

        public CreditCard(String guid, String origin, boolean isLocal, boolean isCached,
                String name, String number, String networkAndLastFourDigits, String month,
                String year, String basicCardIssuerNetwork, int issuerIconDrawableId,
                String billingAddressId, String serverId) {
            this(guid, origin, isLocal, isCached, /* isVirtual= */ false, name, number,
                    networkAndLastFourDigits, month, year, basicCardIssuerNetwork,
                    issuerIconDrawableId, billingAddressId, serverId,
                    /* instrumentId= */ 0, /* cardLabel= */ networkAndLastFourDigits,
                    /* nickname= */ "",
                    /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.UNSPECIFIED,
                    /* productDescription= */ "", /* cardNameForAutofillDisplay= */ "",
                    /* obfuscatedLastFourDigits= */ "");
        }

        public CreditCard(String guid, String origin, boolean isLocal, boolean isCached,
                boolean isVirtual, String name, String number, String networkAndLastFourDigits,
                String month, String year, String basicCardIssuerNetwork, int issuerIconDrawableId,
                String billingAddressId, String serverId, long instrumentId, String cardLabel,
                String nickname, GURL cardArtUrl,
                @VirtualCardEnrollmentState int virtualCardEnrollmentState,
                String productDescription, String cardNameForAutofillDisplay,
                String obfuscatedLastFourDigits) {
            mGUID = guid;
            mOrigin = origin;
            mIsLocal = isLocal;
            mIsCached = isCached;
            mIsVirtual = isVirtual;
            mName = name;
            mNumber = number;
            mNetworkAndLastFourDigits = networkAndLastFourDigits;
            mMonth = month;
            mYear = year;
            mBasicCardIssuerNetwork = basicCardIssuerNetwork;
            mIssuerIconDrawableId = issuerIconDrawableId;
            mBillingAddressId = billingAddressId;
            mServerId = serverId;
            mInstrumentId = instrumentId;
            mCardLabel = cardLabel;
            mNickname = nickname;
            mCardArtUrl = cardArtUrl;
            mVirtualCardEnrollmentState = virtualCardEnrollmentState;
            mProductDescription = productDescription;
            mCardNameForAutofillDisplay = cardNameForAutofillDisplay;
            mObfuscatedLastFourDigits = obfuscatedLastFourDigits;
        }

        public CreditCard() {
            this("" /* guid */, AutofillEditorBase.SETTINGS_ORIGIN /*origin */, true /* isLocal */,
                    false /* isCached */, "" /* name */, "" /* number */,
                    "" /* networkAndLastFourDigits */, "" /* month */, "" /* year */,
                    "" /* basicCardIssuerNetwork */, 0 /* issuerIconDrawableId */,
                    "" /* billingAddressId */, "" /* serverId */);
        }

        @CalledByNative("CreditCard")
        public String getGUID() {
            return mGUID;
        }

        @CalledByNative("CreditCard")
        public String getOrigin() {
            return mOrigin;
        }

        @CalledByNative("CreditCard")
        public String getName() {
            return mName;
        }

        @CalledByNative("CreditCard")
        public String getNumber() {
            return mNumber;
        }

        public String getNetworkAndLastFourDigits() {
            return mNetworkAndLastFourDigits;
        }

        @CalledByNative("CreditCard")
        public String getMonth() {
            return mMonth;
        }

        @CalledByNative("CreditCard")
        public String getYear() {
            return mYear;
        }

        public String getFormattedExpirationDate(Context context) {
            String twoDigityear = getYear().substring(2);
            return getMonth()
                    + context.getResources().getString(R.string.autofill_expiration_date_separator)
                    + twoDigityear;
        }

        @CalledByNative("CreditCard")
        public boolean getIsLocal() {
            return mIsLocal;
        }

        @CalledByNative("CreditCard")
        public boolean getIsCached() {
            return mIsCached;
        }

        @CalledByNative("CreditCard")
        public boolean getIsVirtual() {
            return mIsVirtual;
        }

        @CalledByNative("CreditCard")
        public String getBasicCardIssuerNetwork() {
            return mBasicCardIssuerNetwork;
        }

        public int getIssuerIconDrawableId() {
            return mIssuerIconDrawableId;
        }

        @CalledByNative("CreditCard")
        public String getBillingAddressId() {
            return mBillingAddressId;
        }

        @CalledByNative("CreditCard")
        public String getServerId() {
            return mServerId;
        }

        @CalledByNative("CreditCard")
        public long getInstrumentId() {
            return mInstrumentId;
        }

        public String getCardLabel() {
            return mCardLabel;
        }

        @CalledByNative("CreditCard")
        public String getNickname() {
            return mNickname;
        }

        @CalledByNative("CreditCard")
        public GURL getCardArtUrl() {
            return mCardArtUrl;
        }

        @CalledByNative("CreditCard")
        public @VirtualCardEnrollmentState int getVirtualCardEnrollmentState() {
            return mVirtualCardEnrollmentState;
        }

        @CalledByNative("CreditCard")
        public String getProductDescription() {
            return mProductDescription;
        }

        public String getCardNameForAutofillDisplay() {
            return mCardNameForAutofillDisplay;
        }

        public String getObfuscatedLastFourDigits() {
            return mObfuscatedLastFourDigits;
        }

        public void setGUID(String guid) {
            mGUID = guid;
        }

        public void setOrigin(String origin) {
            mOrigin = origin;
        }

        public void setName(String name) {
            mName = name;
        }

        public void setNumber(String number) {
            mNumber = number;
        }

        public void setNetworkAndLastFourDigits(String networkAndLastFourDigits) {
            mNetworkAndLastFourDigits = networkAndLastFourDigits;
        }

        public void setMonth(String month) {
            mMonth = month;
        }

        public void setYear(String year) {
            mYear = year;
        }

        public void setBasicCardIssuerNetwork(String network) {
            mBasicCardIssuerNetwork = network;
        }

        public void setIssuerIconDrawableId(int id) {
            mIssuerIconDrawableId = id;
        }

        public void setBillingAddressId(String id) {
            mBillingAddressId = id;
        }

        public void setCardLabel(String cardLabel) {
            mCardLabel = cardLabel;
        }

        public void setNickname(String nickname) {
            mNickname = nickname;
        }

        public void setCardArtUrl(GURL cardArtUrl) {
            mCardArtUrl = cardArtUrl;
        }

        public boolean hasValidCreditCardExpirationDate() {
            if (mMonth.isEmpty() || mYear.isEmpty()) return false;

            Calendar expiryDate = Calendar.getInstance();
            // The mMonth value is 1 based but the month in calendar is 0 based.
            expiryDate.set(Calendar.MONTH, Integer.parseInt(mMonth) - 1);
            expiryDate.set(Calendar.YEAR, Integer.parseInt(mYear));
            // Add 1 minute to the expiry instance to ensure that the card is still valid on its
            // exact expiration date.
            expiryDate.add(Calendar.MINUTE, 1);
            return Calendar.getInstance().before(expiryDate);
        }
    }

    private static PersonalDataManager sManager;

    // Suppress FindBugs warning, since |sManager| is only used on the UI thread.
    public static PersonalDataManager getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sManager == null) {
            sManager = new PersonalDataManager();
        }
        return sManager;
    }

    private static int sRequestTimeoutSeconds = 5;

    private final long mPersonalDataManagerAndroid;
    private final List<PersonalDataManagerObserver> mDataObservers =
            new ArrayList<PersonalDataManagerObserver>();
    private final Map<String, Bitmap> mCreditCardArtImages = new HashMap<>();
    private ImageFetcher mImageFetcher = ImageFetcherFactory.createImageFetcher(
            ImageFetcherConfig.DISK_CACHE_ONLY, ProfileKey.getLastUsedRegularProfileKey());

    private PersonalDataManager() {
        // Note that this technically leaks the native object, however, PersonalDataManager
        // is a singleton that lives forever and there's no clean shutdown of Chrome on Android
        mPersonalDataManagerAndroid = PersonalDataManagerJni.get().init(PersonalDataManager.this);
    }

    /**
     * Called from native when template URL service is done loading.
     */
    @CalledByNative
    private void personalDataChanged() {
        ThreadUtils.assertOnUiThread();
        for (PersonalDataManagerObserver observer : mDataObservers) {
            observer.onPersonalDataChanged();
        }
        fetchCreditCardArtImages();
    }

    /**
     * Registers a PersonalDataManagerObserver on the native side.
     */
    public boolean registerDataObserver(PersonalDataManagerObserver observer) {
        ThreadUtils.assertOnUiThread();
        assert !mDataObservers.contains(observer);
        mDataObservers.add(observer);
        return PersonalDataManagerJni.get().isDataLoaded(
                mPersonalDataManagerAndroid, PersonalDataManager.this);
    }

    /**
     * Unregisters the provided observer.
     */
    public void unregisterDataObserver(PersonalDataManagerObserver observer) {
        ThreadUtils.assertOnUiThread();
        assert (mDataObservers.size() > 0);
        assert (mDataObservers.contains(observer));
        mDataObservers.remove(observer);
    }

    /**
     * TODO(crbug.com/616102): Reduce the number of Java to Native calls when getting profiles.
     *
     * Gets the profiles to show in the settings page. Returns all the profiles without any
     * processing.
     *
     * @return The list of profiles to show in the settings.
     */
    public List<AutofillProfile> getProfilesForSettings() {
        ThreadUtils.assertOnUiThread();
        return getProfilesWithLabels(PersonalDataManagerJni.get().getProfileLabelsForSettings(
                                             mPersonalDataManagerAndroid, PersonalDataManager.this),
                PersonalDataManagerJni.get().getProfileGUIDsForSettings(
                        mPersonalDataManagerAndroid, PersonalDataManager.this));
    }

    /**
     * TODO(crbug.com/616102): Reduce the number of Java to Native calls when getting profiles
     *
     * Gets the profiles to suggest when filling a form or completing a transaction. The profiles
     * will have been processed to be more relevant to the user.
     *
     * @param includeNameInLabel Whether to include the name in the profile's label.
     * @return The list of profiles to suggest to the user.
     */
    public ArrayList<AutofillProfile> getProfilesToSuggest(boolean includeNameInLabel) {
        ThreadUtils.assertOnUiThread();
        return getProfilesWithLabels(
                PersonalDataManagerJni.get().getProfileLabelsToSuggest(mPersonalDataManagerAndroid,
                        PersonalDataManager.this, includeNameInLabel,
                        true /* includeOrganizationInLabel */, true /* includeCountryInLabel */),
                PersonalDataManagerJni.get().getProfileGUIDsToSuggest(
                        mPersonalDataManagerAndroid, PersonalDataManager.this));
    }

    /**
     * TODO(crbug.com/616102): Reduce the number of Java to Native calls when getting profiles.
     *
     * Gets the profiles to suggest when associating a billing address to a credit card. The
     * profiles will have been processed to be more relevant to the user.
     *
     * @param includeOrganizationInLabel Whether the organization name should be included in the
     *                                   label.
     *
     * @return The list of billing addresses to suggest to the user.
     */
    public ArrayList<AutofillProfile> getBillingAddressesToSuggest(
            boolean includeOrganizationInLabel) {
        ThreadUtils.assertOnUiThread();
        return getProfilesWithLabels(
                PersonalDataManagerJni.get().getProfileLabelsToSuggest(mPersonalDataManagerAndroid,
                        PersonalDataManager.this, true /* includeNameInLabel */,
                        includeOrganizationInLabel, false /* includeCountryInLabel */),
                PersonalDataManagerJni.get().getProfileGUIDsToSuggest(
                        mPersonalDataManagerAndroid, PersonalDataManager.this));
    }

    private ArrayList<AutofillProfile> getProfilesWithLabels(
            String[] profileLabels, String[] profileGUIDs) {
        ArrayList<AutofillProfile> profiles = new ArrayList<AutofillProfile>(profileGUIDs.length);
        for (int i = 0; i < profileGUIDs.length; i++) {
            AutofillProfile profile = PersonalDataManagerJni.get().getProfileByGUID(
                    mPersonalDataManagerAndroid, PersonalDataManager.this, profileGUIDs[i]);
            profile.setLabel(profileLabels[i]);
            profiles.add(profile);
        }

        return profiles;
    }

    public AutofillProfile getProfile(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getProfileByGUID(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    public void deleteProfile(String guid) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().removeByGUID(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    public String setProfile(AutofillProfile profile) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().setProfile(
                mPersonalDataManagerAndroid, PersonalDataManager.this, profile);
    }

    public String setProfileToLocal(AutofillProfile profile) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().setProfileToLocal(
                mPersonalDataManagerAndroid, PersonalDataManager.this, profile);
    }

    /**
     * Gets the credit cards to show in the settings page. Returns all the cards without any
     * processing.
     */
    public List<CreditCard> getCreditCardsForSettings() {
        ThreadUtils.assertOnUiThread();
        return getCreditCards(PersonalDataManagerJni.get().getCreditCardGUIDsForSettings(
                mPersonalDataManagerAndroid, PersonalDataManager.this));
    }

    /**
     * Gets the credit cards to suggest when filling a form or completing a transaction. The cards
     * will have been processed to be more relevant to the user.
     */
    public ArrayList<CreditCard> getCreditCardsToSuggest() {
        ThreadUtils.assertOnUiThread();
        return getCreditCards(PersonalDataManagerJni.get().getCreditCardGUIDsToSuggest(
                mPersonalDataManagerAndroid, PersonalDataManager.this));
    }

    private ArrayList<CreditCard> getCreditCards(String[] creditCardGUIDs) {
        ArrayList<CreditCard> cards = new ArrayList<CreditCard>(creditCardGUIDs.length);
        for (int i = 0; i < creditCardGUIDs.length; i++) {
            cards.add(PersonalDataManagerJni.get().getCreditCardByGUID(
                    mPersonalDataManagerAndroid, PersonalDataManager.this, creditCardGUIDs[i]));
        }
        return cards;
    }

    public CreditCard getCreditCard(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCreditCardByGUID(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    public CreditCard getCreditCardForNumber(String cardNumber) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCreditCardForNumber(
                mPersonalDataManagerAndroid, PersonalDataManager.this, cardNumber);
    }

    public String setCreditCard(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        assert card.getIsLocal();
        return PersonalDataManagerJni.get().setCreditCard(
                mPersonalDataManagerAndroid, PersonalDataManager.this, card);
    }

    public void updateServerCardBillingAddress(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().updateServerCardBillingAddress(
                mPersonalDataManagerAndroid, PersonalDataManager.this, card);
    }

    public String getBasicCardIssuerNetwork(String cardNumber, boolean emptyIfInvalid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getBasicCardIssuerNetwork(
                mPersonalDataManagerAndroid, PersonalDataManager.this, cardNumber, emptyIfInvalid);
    }

    @VisibleForTesting
    public void addServerCreditCardForTest(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        assert !card.getIsLocal();
        PersonalDataManagerJni.get().addServerCreditCardForTest(
                mPersonalDataManagerAndroid, PersonalDataManager.this, card);
    }

    @VisibleForTesting
    public void addServerCreditCardForTestWithAdditionalFields(
            CreditCard card, String nickname, int cardIssuer) {
        ThreadUtils.assertOnUiThread();
        assert !card.getIsLocal();
        PersonalDataManagerJni.get().addServerCreditCardForTestWithAdditionalFields(
                mPersonalDataManagerAndroid, PersonalDataManager.this, card, nickname, cardIssuer);
    }

    public void deleteCreditCard(String guid) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().removeByGUID(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    public void clearUnmaskedCache(String guid) {
        PersonalDataManagerJni.get().clearUnmaskedCache(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    public String getShippingAddressLabelWithCountryForPaymentRequest(AutofillProfile profile) {
        return PersonalDataManagerJni.get().getShippingAddressLabelWithCountryForPaymentRequest(
                mPersonalDataManagerAndroid, PersonalDataManager.this, profile);
    }

    public String getShippingAddressLabelWithoutCountryForPaymentRequest(AutofillProfile profile) {
        return PersonalDataManagerJni.get().getShippingAddressLabelWithoutCountryForPaymentRequest(
                mPersonalDataManagerAndroid, PersonalDataManager.this, profile);
    }

    public String getBillingAddressLabelForPaymentRequest(AutofillProfile profile) {
        return PersonalDataManagerJni.get().getBillingAddressLabelForPaymentRequest(
                mPersonalDataManagerAndroid, PersonalDataManager.this, profile);
    }

    /**
     * Records the use of the profile associated with the specified {@code guid}. Effectively
     * increments the use count of the profile and sets its use date to the current time. Also logs
     * usage metrics.
     *
     * @param guid The GUID of the profile.
     */
    public void recordAndLogProfileUse(String guid) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().recordAndLogProfileUse(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    @VisibleForTesting
    protected void setProfileUseStatsForTesting(String guid, int count, long date) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().setProfileUseStatsForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid, count, date);
    }

    @VisibleForTesting
    int getProfileUseCountForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getProfileUseCountForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    @VisibleForTesting
    long getProfileUseDateForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getProfileUseDateForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    /**
     * Records the use of the credit card associated with the specified {@code guid}. Effectively
     * increments the use count of the credit card and set its use date to the current time. Also
     * logs usage metrics.
     *
     * @param guid The GUID of the credit card.
     */
    public void recordAndLogCreditCardUse(String guid) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().recordAndLogCreditCardUse(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    @VisibleForTesting
    protected void setCreditCardUseStatsForTesting(String guid, int count, long date) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().setCreditCardUseStatsForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid, count, date);
    }

    @VisibleForTesting
    int getCreditCardUseCountForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCreditCardUseCountForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    @VisibleForTesting
    long getCreditCardUseDateForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCreditCardUseDateForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    @VisibleForTesting
    long getCurrentDateForTesting() {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCurrentDateForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this);
    }

    @VisibleForTesting
    protected void clearServerDataForTesting() {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().clearServerDataForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this);
    }

    @VisibleForTesting
    public static void setInstanceForTesting(PersonalDataManager manager) {
        sManager = manager;
    }

    /**
     * Determines whether the logged in user (if any) is eligible to store
     * Autofill address profiles to their account.
     */
    public boolean isEligibleForAddressAccountStorage() {
        return PersonalDataManagerJni.get().isEligibleForAddressAccountStorage(
                mPersonalDataManagerAndroid, PersonalDataManager.this);
    }

    /**
     * Users based in unsupported countries and profiles with a country value set
     * to an unsupported country are not eligible for account storage. This
     * function determines if the `country_code` is eligible.
     */
    public boolean isCountryEligibleForAccountStorage(String countryCode) {
        return PersonalDataManagerJni.get().isCountryEligibleForAccountStorage(
                mPersonalDataManagerAndroid, PersonalDataManager.this, countryCode);
    }

    /**
     * Starts loading the address validation rules for the specified {@code regionCode}.
     *
     * @param regionCode The code of the region for which to load the rules.
     */
    public void loadRulesForAddressNormalization(String regionCode) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().loadRulesForAddressNormalization(
                mPersonalDataManagerAndroid, PersonalDataManager.this, regionCode);
    }

    /**
     * Starts loading the sub-key request rules for the specified {@code regionCode}.
     *
     * @param regionCode The code of the region for which to load the rules.
     */
    public void loadRulesForSubKeys(String regionCode) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().loadRulesForSubKeys(
                mPersonalDataManagerAndroid, PersonalDataManager.this, regionCode);
    }

    /**
     * Starts requesting the subkeys for the specified {@code regionCode}, if the rules
     * associated with the {@code regionCode} are done loading. Otherwise sets up the callback to
     * start loading the subkeys when the rules are loaded. The received subkeys will be sent
     * to the {@code delegate}. If the subkeys are not received in the specified
     * {@code sRequestTimeoutSeconds}, the {@code delegate} will be notified.
     *
     * @param regionCode The code of the region for which to load the subkeys.
     * @param delegate The object requesting the subkeys.
     */
    public void getRegionSubKeys(String regionCode, GetSubKeysRequestDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().startRegionSubKeysRequest(mPersonalDataManagerAndroid,
                PersonalDataManager.this, regionCode, sRequestTimeoutSeconds, delegate);
    }

    /** Cancels the pending subkeys request. */
    public void cancelPendingGetSubKeys() {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().cancelPendingGetSubKeys(mPersonalDataManagerAndroid);
    }

    /**
     * Normalizes the address of the profile associated with the {@code guid} if the rules
     * associated with the profile's region are done loading. Otherwise sets up the callback to
     * start normalizing the address when the rules are loaded. The normalized profile will be sent
     * to the {@code delegate}. If the profile is not normalized in the specified
     * {@code sRequestTimeoutSeconds}, the {@code delegate} will be notified.
     *
     * @param profile The profile to normalize.
     * @param delegate The object requesting the normalization.
     */
    public void normalizeAddress(
            AutofillProfile profile, NormalizedAddressRequestDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().startAddressNormalization(mPersonalDataManagerAndroid,
                PersonalDataManager.this, profile, sRequestTimeoutSeconds, delegate);
    }

    /**
     * Checks whether the Autofill PersonalDataManager has profiles.
     *
     * @return True If there are profiles.
     */
    public boolean hasProfiles() {
        return PersonalDataManagerJni.get().hasProfiles(mPersonalDataManagerAndroid);
    }

    /**
     * Checks whether the Autofill PersonalDataManager has credit cards.
     *
     * @return True If there are credit cards.
     */
    public boolean hasCreditCards() {
        return PersonalDataManagerJni.get().hasCreditCards(mPersonalDataManagerAndroid);
    }

    /**
     * @return Whether FIDO authentication is available.
     */
    public boolean isFidoAuthenticationAvailable() {
        return isAutofillCreditCardEnabled()
                && PersonalDataManagerJni.get().isFidoAuthenticationAvailable(
                        mPersonalDataManagerAndroid);
    }

    /**
     * @return Whether the Autofill feature for Profiles (addresses) is enabled.
     */
    public static boolean isAutofillProfileEnabled() {
        return getPrefService().getBoolean(Pref.AUTOFILL_PROFILE_ENABLED);
    }

    /**
     * @return Whether the Autofill feature for Credit Cards is enabled.
     */
    public static boolean isAutofillCreditCardEnabled() {
        return getPrefService().getBoolean(Pref.AUTOFILL_CREDIT_CARD_ENABLED);
    }

    /**
     * Enables or disables the Autofill feature for Profiles.
     * @param enable True to disable profile Autofill, false otherwise.
     */
    public static void setAutofillProfileEnabled(boolean enable) {
        getPrefService().setBoolean(Pref.AUTOFILL_PROFILE_ENABLED, enable);
    }

    /**
     * Enables or disables the Autofill feature for Credit Cards.
     * @param enable True to disable credit card Autofill, false otherwise.
     */
    public static void setAutofillCreditCardEnabled(boolean enable) {
        getPrefService().setBoolean(Pref.AUTOFILL_CREDIT_CARD_ENABLED, enable);
    }

    /**
     * @return Whether the Autofill feature for FIDO authentication is enabled.
     */
    public static boolean isAutofillCreditCardFidoAuthEnabled() {
        return getPrefService().getBoolean(Pref.AUTOFILL_CREDIT_CARD_FIDO_AUTH_ENABLED);
    }

    /**
     * Enables or disables the Autofill feature for FIDO authentication.
     * We are trying to align this pref with the server's source of truth, but any mismatches
     * between this pref and the server should imply the user's intention to opt in/out.
     * @param enable True to enable credit card FIDO authentication, false otherwise.
     */
    public static void setAutofillCreditCardFidoAuthEnabled(boolean enable) {
        getPrefService().setBoolean(Pref.AUTOFILL_CREDIT_CARD_FIDO_AUTH_ENABLED, enable);
    }

    /**
     * @return Whether the Autofill feature for payment methods mandatory reauth is enabled.
     */
    public static boolean isAutofillPaymentMethodsMandatoryReauthEnabled() {
        return getPrefService().getBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH);
    }

    /**
     * Enables or disables the Autofill feature for payment methods mandatory reauth.
     * @param enable True to enable payment methods mandatory reauth, false otherwise.
     */
    public static void setAutofillPaymentMethodsMandatoryReauth(boolean enable) {
        getPrefService().setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, enable);
    }

    /**
     * @return Whether the Autofill feature is managed.
     */
    public static boolean isAutofillManaged() {
        return PersonalDataManagerJni.get().isAutofillManaged();
    }

    /**
     * @return Whether the Autofill feature for Profiles (addresses) is managed.
     */
    public static boolean isAutofillProfileManaged() {
        return PersonalDataManagerJni.get().isAutofillProfileManaged();
    }

    /**
     * @return Whether the Autofill feature for Credit Cards is managed.
     */
    public static boolean isAutofillCreditCardManaged() {
        return PersonalDataManagerJni.get().isAutofillCreditCardManaged();
    }

    /**
     * @return Whether the Payments integration feature is enabled.
     */
    public static boolean isPaymentsIntegrationEnabled() {
        return PersonalDataManagerJni.get().isPaymentsIntegrationEnabled();
    }

    /**
     * Enables or disables the Payments integration.
     * @param enable True to enable Payments data import.
     */
    public static void setPaymentsIntegrationEnabled(boolean enable) {
        PersonalDataManagerJni.get().setPaymentsIntegrationEnabled(enable);
    }

    @VisibleForTesting
    public static void setRequestTimeoutForTesting(int timeout) {
        sRequestTimeoutSeconds = timeout;
    }

    @VisibleForTesting
    public void setSyncServiceForTesting() {
        PersonalDataManagerJni.get().setSyncServiceForTesting(mPersonalDataManagerAndroid);
    }

    /**
     * @return The sub-key request timeout in milliseconds.
     */
    public static long getRequestTimeoutMS() {
        return DateUtils.SECOND_IN_MILLIS * sRequestTimeoutSeconds;
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    // TODO (crbug.com/1384128): Add icon dimensions to card art URL.
    private void fetchCreditCardArtImages() {
        for (CreditCard card : getCreditCardsToSuggest()) {
            // Fetch the image using the ImageFetcher only if it is not present in the cache.
            if (card.getCardArtUrl() != null && card.getCardArtUrl().isValid()
                    && !mCreditCardArtImages.containsKey(card.getCardArtUrl().getSpec())) {
                fetchImage(card.getCardArtUrl(),
                        bitmap -> mCreditCardArtImages.put(card.getCardArtUrl().getSpec(), bitmap));
            }
        }
    }

    /**
     * Return the card art image for the given `customImageUrl`.
     * @param context required to get resources.
     * @param customImageUrl  URL of the image. If the image is available, it is returned, otherwise
     *         it is fetched from this URL.
     * @param widthId Resource id of the width spec.
     * @param heightId Resource id of the height spec.
     * @param cornerRadiusId Resource id of the corner radius spec.
     * @return Bitmap if found in the local cache, else return null.
     */
    public Bitmap getCustomImageForAutofillSuggestionIfAvailable(
            Context context, GURL customImageUrl, int widthId, int heightId, int cornerRadiusId) {
        Resources res = context.getResources();
        int width = res.getDimensionPixelSize(widthId);
        int height = res.getDimensionPixelSize(heightId);
        float cornerRadius = res.getDimension(cornerRadiusId);

        // TODO(crbug.com/1313616): The Capital One icon for virtual cards is available in a single
        // size via a static URL. Cache this image at different sizes so it can be used by different
        // surfaces.
        GURL urlToCache =
                AutofillUiUtils.getCreditCardIconUrlWithParams(customImageUrl, width, height);
        GURL urlToFetch = customImageUrl.getSpec().equals(AutofillUiUtils.CAPITAL_ONE_ICON_URL)
                ? customImageUrl
                : urlToCache;

        if (mCreditCardArtImages.containsKey(urlToCache.getSpec())) {
            return mCreditCardArtImages.get(urlToCache.getSpec());
        }
        // Schedule the fetching of image and return null so that the UI thread does not have to
        // wait and can show the default network icon.
        fetchImage(urlToFetch, bitmap -> {
            // TODO (crbug.com/1410418): Log image fetching failure metrics.
            // If the image fetching was unsuccessful, silently return.
            if (bitmap == null) return;

            // When adding new sizes for card icons, check if the corner radius needs to be added as
            // a suffix for caching (crbug.com/1431283).
            mCreditCardArtImages.put(urlToCache.getSpec(),
                    AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(bitmap, width, height,
                            cornerRadius,
                            ChromeFeatureList.isEnabled(
                                    ChromeFeatureList
                                            .AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES)));
        });
        return null;
    }

    @VisibleForTesting
    public void setImageFetcherForTesting(ImageFetcher imageFetcher) {
        this.mImageFetcher = imageFetcher;
    }

    private void fetchImage(GURL customImageUrl, Callback<Bitmap> callback) {
        if (!customImageUrl.isValid()) {
            Log.w(TAG, "Tried to fetch an invalid url %s", customImageUrl.getSpec());
            return;
        }
        ImageFetcher.Params params = ImageFetcher.Params.create(
                customImageUrl.getSpec(), ImageFetcher.AUTOFILL_CARD_ART_UMA_CLIENT_NAME);
        mImageFetcher.fetchImage(params, bitmap -> callback.onResult(bitmap));
    }

    @NativeMethods
    interface Natives {
        long init(PersonalDataManager caller);
        boolean isDataLoaded(long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        String[] getProfileGUIDsForSettings(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        String[] getProfileGUIDsToSuggest(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        String[] getProfileLabelsForSettings(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        String[] getProfileLabelsToSuggest(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, boolean includeNameInLabel,
                boolean includeOrganizationInLabel, boolean includeCountryInLabel);
        AutofillProfile getProfileByGUID(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        boolean isEligibleForAddressAccountStorage(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        boolean isCountryEligibleForAccountStorage(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String countryCode);
        String setProfile(long nativePersonalDataManagerAndroid, PersonalDataManager caller,
                AutofillProfile profile);
        String setProfileToLocal(long nativePersonalDataManagerAndroid, PersonalDataManager caller,
                AutofillProfile profile);
        String getShippingAddressLabelWithCountryForPaymentRequest(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller,
                AutofillProfile profile);
        String getShippingAddressLabelWithoutCountryForPaymentRequest(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller,
                AutofillProfile profile);
        String getBillingAddressLabelForPaymentRequest(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, AutofillProfile profile);
        String[] getCreditCardGUIDsForSettings(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        String[] getCreditCardGUIDsToSuggest(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        CreditCard getCreditCardByGUID(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        CreditCard getCreditCardForNumber(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String cardNumber);
        String setCreditCard(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, CreditCard card);
        void updateServerCardBillingAddress(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, CreditCard card);
        String getBasicCardIssuerNetwork(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String cardNumber, boolean emptyIfInvalid);
        void addServerCreditCardForTest(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, CreditCard card);
        void addServerCreditCardForTestWithAdditionalFields(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, CreditCard card, String nickname, int cardIssuer);
        void removeByGUID(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        void recordAndLogProfileUse(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        void setProfileUseStatsForTesting(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String guid, int count, long date);
        int getProfileUseCountForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        long getProfileUseDateForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        void recordAndLogCreditCardUse(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        void setCreditCardUseStatsForTesting(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String guid, int count, long date);
        int getCreditCardUseCountForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        long getCreditCardUseDateForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        long getCurrentDateForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        void clearServerDataForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller);
        void clearUnmaskedCache(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        void loadRulesForAddressNormalization(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String regionCode);
        void loadRulesForSubKeys(long nativePersonalDataManagerAndroid, PersonalDataManager caller,
                String regionCode);
        void startAddressNormalization(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, AutofillProfile profile, int timeoutSeconds,
                NormalizedAddressRequestDelegate delegate);
        void startRegionSubKeysRequest(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String regionCode, int timeoutSeconds,
                GetSubKeysRequestDelegate delegate);
        boolean hasProfiles(long nativePersonalDataManagerAndroid);
        boolean hasCreditCards(long nativePersonalDataManagerAndroid);
        boolean isFidoAuthenticationAvailable(long nativePersonalDataManagerAndroid);
        boolean isAutofillManaged();
        boolean isAutofillProfileManaged();
        boolean isAutofillCreditCardManaged();
        boolean isPaymentsIntegrationEnabled();
        void setPaymentsIntegrationEnabled(boolean enable);
        String toCountryCode(String countryName);
        void cancelPendingGetSubKeys(long nativePersonalDataManagerAndroid);
        void setSyncServiceForTesting(long nativePersonalDataManagerAndroid);
    }
}
