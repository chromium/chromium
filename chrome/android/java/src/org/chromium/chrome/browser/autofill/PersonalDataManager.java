// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.chrome.browser.preferences.MainPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

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
     * Callback for full card request.
     */
    public interface FullCardRequestDelegate {
        /**
         * Called when user provided the full card details, including the CVC and the full PAN.
         *
         * @param card The full card.
         * @param cvc The CVC for the card.
         */
        @CalledByNative("FullCardRequestDelegate")
        void onFullCardDetails(CreditCard card, String cvc);

        /**
         * Called when user did not provide full card details.
         */
        @CalledByNative("FullCardRequestDelegate")
        void onFullCardError();
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

    /**
     * Autofill address information.
     */
    public static class AutofillProfile {
        private String mGUID;
        private String mOrigin;
        private boolean mIsLocal;
        private String mFullName;
        private String mCompanyName;
        private String mStreetAddress;
        private String mRegion;
        private String mLocality;
        private String mDependentLocality;
        private String mPostalCode;
        private String mSortingCode;
        private String mCountryCode;
        private String mPhoneNumber;
        private String mEmailAddress;
        private String mLabel;
        private String mLanguageCode;

        @CalledByNative("AutofillProfile")
        public static AutofillProfile create(String guid, String origin, boolean isLocal,
                String fullName, String companyName, String streetAddress, String region,
                String locality, String dependentLocality, String postalCode, String sortingCode,
                String country, String phoneNumber, String emailAddress, String languageCode) {
            return new AutofillProfile(guid, origin, isLocal, fullName, companyName, streetAddress,
                    region, locality, dependentLocality, postalCode, sortingCode, country,
                    phoneNumber, emailAddress, languageCode);
        }

        public AutofillProfile(String guid, String origin, boolean isLocal, String fullName,
                String companyName, String streetAddress, String region, String locality,
                String dependentLocality, String postalCode, String sortingCode, String countryCode,
                String phoneNumber, String emailAddress, String languageCode) {
            mGUID = guid;
            mOrigin = origin;
            mIsLocal = isLocal;
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

        /**
         * Builds an empty local profile with "settings" origin and country code from the default
         * locale. All other fields are empty strings, because JNI does not handle null strings.
         */
        public AutofillProfile() {
            this("" /* guid */, MainPreferences.SETTINGS_ORIGIN /* origin */, true /* isLocal */,
                    "" /* fullName */, "" /* companyName */, "" /* streetAddress */,
                    "" /* region */, "" /* locality */, "" /* dependentLocality */,
                    "" /* postalCode */, "" /* sortingCode */,
                    Locale.getDefault().getCountry() /* country */, "" /* phoneNumber */,
                    "" /* emailAddress */, "" /* languageCode */);
        }

        /* Builds an AutofillProfile that is an exact copy of the one passed as parameter. */
        public AutofillProfile(AutofillProfile profile) {
            mGUID = profile.getGUID();
            mOrigin = profile.getOrigin();
            mIsLocal = profile.getIsLocal();
            mFullName = profile.getFullName();
            mCompanyName = profile.getCompanyName();
            mStreetAddress = profile.getStreetAddress();
            mRegion = profile.getRegion();
            mLocality = profile.getLocality();
            mDependentLocality = profile.getDependentLocality();
            mPostalCode = profile.getPostalCode();
            mSortingCode = profile.getSortingCode();
            mCountryCode = profile.getCountryCode();
            mPhoneNumber = profile.getPhoneNumber();
            mEmailAddress = profile.getEmailAddress();
            mLanguageCode = profile.getLanguageCode();
            mLabel = profile.getLabel();
        }

        /** TODO(estade): remove this constructor. */
        @VisibleForTesting
        public AutofillProfile(String guid, String origin, String fullName, String companyName,
                String streetAddress, String region, String locality, String dependentLocality,
                String postalCode, String sortingCode, String countryCode, String phoneNumber,
                String emailAddress, String languageCode) {
            this(guid, origin, true /* isLocal */, fullName, companyName, streetAddress, region,
                    locality, dependentLocality, postalCode, sortingCode, countryCode, phoneNumber,
                    emailAddress, languageCode);
        }

        @CalledByNative("AutofillProfile")
        public String getGUID() {
            return mGUID;
        }

        @CalledByNative("AutofillProfile")
        public String getOrigin() {
            return mOrigin;
        }

        @CalledByNative("AutofillProfile")
        public String getFullName() {
            return mFullName;
        }

        @CalledByNative("AutofillProfile")
        public String getCompanyName() {
            return mCompanyName;
        }

        @CalledByNative("AutofillProfile")
        public String getStreetAddress() {
            return mStreetAddress;
        }

        @CalledByNative("AutofillProfile")
        public String getRegion() {
            return mRegion;
        }

        @CalledByNative("AutofillProfile")
        public String getLocality() {
            return mLocality;
        }

        @CalledByNative("AutofillProfile")
        public String getDependentLocality() {
            return mDependentLocality;
        }

        public String getLabel() {
            return mLabel;
        }

        @CalledByNative("AutofillProfile")
        public String getPostalCode() {
            return mPostalCode;
        }

        @CalledByNative("AutofillProfile")
        public String getSortingCode() {
            return mSortingCode;
        }

        @CalledByNative("AutofillProfile")
        public String getCountryCode() {
            return mCountryCode;
        }

        @CalledByNative("AutofillProfile")
        public String getPhoneNumber() {
            return mPhoneNumber;
        }

        @CalledByNative("AutofillProfile")
        public String getEmailAddress() {
            return mEmailAddress;
        }

        @CalledByNative("AutofillProfile")
        public String getLanguageCode() {
            return mLanguageCode;
        }

        public boolean getIsLocal() {
            return mIsLocal;
        }

        @VisibleForTesting
        public void setGUID(String guid) {
            mGUID = guid;
        }

        public void setLabel(String label) {
            mLabel = label;
        }

        public void setOrigin(String origin) {
            mOrigin = origin;
        }

        public void setFullName(String fullName) {
            mFullName = fullName;
        }

        public void setCompanyName(String companyName) {
            mCompanyName = companyName;
        }

        @VisibleForTesting
        public void setStreetAddress(String streetAddress) {
            mStreetAddress = streetAddress;
        }

        public void setRegion(String region) {
            mRegion = region;
        }

        public void setLocality(String locality) {
            mLocality = locality;
        }

        public void setDependentLocality(String dependentLocality) {
            mDependentLocality = dependentLocality;
        }

        public void setPostalCode(String postalCode) {
            mPostalCode = postalCode;
        }

        public void setSortingCode(String sortingCode) {
            mSortingCode = sortingCode;
        }

        @VisibleForTesting
        public void setCountryCode(String countryCode) {
            mCountryCode = countryCode;
        }

        public void setPhoneNumber(String phoneNumber) {
            mPhoneNumber = phoneNumber;
        }

        public void setEmailAddress(String emailAddress) {
            mEmailAddress = emailAddress;
        }

        @VisibleForTesting
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
        @CardType
        private final int mCardType;
        private String mGUID;
        private String mOrigin;
        private boolean mIsLocal;
        private boolean mIsCached;
        private String mName;
        private String mNumber;
        private String mObfuscatedNumber;
        private String mMonth;
        private String mYear;
        private String mBasicCardIssuerNetwork;
        private int mIssuerIconDrawableId;
        private String mBillingAddressId;
        private String mServerId;

        @CalledByNative("CreditCard")
        public static CreditCard create(String guid, String origin, boolean isLocal,
                boolean isCached, String name, String number, String obfuscatedNumber, String month,
                String year, String basicCardIssuerNetwork, int enumeratedIconId,
                @CardType int cardType, String billingAddressId, String serverId) {
            return new CreditCard(guid, origin, isLocal, isCached, name, number, obfuscatedNumber,
                    month, year, basicCardIssuerNetwork,
                    ResourceId.mapToDrawableId(enumeratedIconId), cardType, billingAddressId,
                    serverId);
        }

        public CreditCard(String guid, String origin, boolean isLocal, boolean isCached,
                String name, String number, String obfuscatedNumber, String month, String year,
                String basicCardIssuerNetwork, int issuerIconDrawableId, @CardType int cardType,
                String billingAddressId, String serverId) {
            mGUID = guid;
            mOrigin = origin;
            mIsLocal = isLocal;
            mIsCached = isCached;
            mName = name;
            mNumber = number;
            mObfuscatedNumber = obfuscatedNumber;
            mMonth = month;
            mYear = year;
            mBasicCardIssuerNetwork = basicCardIssuerNetwork;
            mIssuerIconDrawableId = issuerIconDrawableId;
            mCardType = cardType;
            mBillingAddressId = billingAddressId;
            mServerId = serverId;
        }

        public CreditCard() {
            this("" /* guid */, MainPreferences.SETTINGS_ORIGIN /*origin */, true /* isLocal */,
                    false /* isCached */, "" /* name */, "" /* number */, "" /* obfuscatedNumber */,
                    "" /* month */, "" /* year */, "" /* basicCardIssuerNetwork */,
                    0 /* issuerIconDrawableId */, CardType.UNKNOWN, "" /* billingAddressId */,
                    "" /* serverId */);
        }

        /** TODO(estade): remove this constructor. */
        @VisibleForTesting
        public CreditCard(String guid, String origin, String name, String number,
                String obfuscatedNumber, String month, String year) {
            this(guid, origin, true /* isLocal */, false /* isCached */, name, number,
                    obfuscatedNumber, month, year, "" /* basicCardIssuerNetwork */,
                    0 /* issuerIconDrawableId */, CardType.UNKNOWN, "" /* billingAddressId */,
                    "" /* serverId */);
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

        public String getObfuscatedNumber() {
            return mObfuscatedNumber;
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
            return getMonth()
                    + context.getResources().getString(
                              R.string.autofill_card_unmask_expiration_date_separator) + getYear();
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
        public String getBasicCardIssuerNetwork() {
            return mBasicCardIssuerNetwork;
        }

        public int getIssuerIconDrawableId() {
            return mIssuerIconDrawableId;
        }

        @CardType
        @CalledByNative("CreditCard")
        public int getCardType() {
            return mCardType;
        }

        @CalledByNative("CreditCard")
        public String getBillingAddressId() {
            return mBillingAddressId;
        }

        @CalledByNative("CreditCard")
        public String getServerId() {
            return mServerId;
        }

        @VisibleForTesting
        public void setGUID(String guid) {
            mGUID = guid;
        }

        public void setOrigin(String origin) {
            mOrigin = origin;
        }

        public void setName(String name) {
            mName = name;
        }

        @VisibleForTesting
        public void setNumber(String number) {
            mNumber = number;
        }

        public void setObfuscatedNumber(String obfuscatedNumber) {
            mObfuscatedNumber = obfuscatedNumber;
        }

        @VisibleForTesting
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

    private PersonalDataManager() {
        // Note that this technically leaks the native object, however, PersonalDataManager
        // is a singleton that lives forever and there's no clean shutdown of Chrome on Android
        mPersonalDataManagerAndroid = nativeInit();
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
    }

    /**
     * Registers a PersonalDataManagerObserver on the native side.
     */
    public boolean registerDataObserver(PersonalDataManagerObserver observer) {
        ThreadUtils.assertOnUiThread();
        assert !mDataObservers.contains(observer);
        mDataObservers.add(observer);
        return nativeIsDataLoaded(mPersonalDataManagerAndroid);
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
        return getProfilesWithLabels(nativeGetProfileLabelsForSettings(mPersonalDataManagerAndroid),
                nativeGetProfileGUIDsForSettings(mPersonalDataManagerAndroid));
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
                nativeGetProfileLabelsToSuggest(
                        mPersonalDataManagerAndroid, includeNameInLabel,
                        true /* includeOrganizationInLabel */, true /* includeCountryInLabel */),
                nativeGetProfileGUIDsToSuggest(mPersonalDataManagerAndroid));
    }

    /**
     * TODO(crbug.com/616102): Reduce the number of Java to Native calls when getting profiles.
     *
     * Gets the profiles to suggest when associating a billing address to a credit card. The
     * profiles will have been processed to be more relevant to the user.
     *
     * @return The list of billing addresses to suggest to the user.
     */
    public ArrayList<AutofillProfile> getBillingAddressesToSuggest() {
        ThreadUtils.assertOnUiThread();
        return getProfilesWithLabels(
                nativeGetProfileLabelsToSuggest(
                        mPersonalDataManagerAndroid, true /* includeNameInLabel */,
                        false /* includeOrganizationInLabel */, false /* includeCountryInLabel */),
                nativeGetProfileGUIDsToSuggest(mPersonalDataManagerAndroid));
    }

    private ArrayList<AutofillProfile> getProfilesWithLabels(
            String[] profileLabels, String[] profileGUIDs) {
        ArrayList<AutofillProfile> profiles = new ArrayList<AutofillProfile>(profileGUIDs.length);
        for (int i = 0; i < profileGUIDs.length; i++) {
            AutofillProfile profile =
                    nativeGetProfileByGUID(mPersonalDataManagerAndroid, profileGUIDs[i]);
            profile.setLabel(profileLabels[i]);
            profiles.add(profile);
        }

        return profiles;
    }

    public AutofillProfile getProfile(String guid) {
        ThreadUtils.assertOnUiThread();
        return nativeGetProfileByGUID(mPersonalDataManagerAndroid, guid);
    }

    public void deleteProfile(String guid) {
        ThreadUtils.assertOnUiThread();
        nativeRemoveByGUID(mPersonalDataManagerAndroid, guid);
    }

    public String setProfile(AutofillProfile profile) {
        ThreadUtils.assertOnUiThread();
        return nativeSetProfile(mPersonalDataManagerAndroid, profile);
    }

    public String setProfileToLocal(AutofillProfile profile) {
        ThreadUtils.assertOnUiThread();
        return nativeSetProfileToLocal(mPersonalDataManagerAndroid, profile);
    }

    /**
     * Gets the credit cards to show in the settings page. Returns all the cards without any
     * processing.
     */
    public List<CreditCard> getCreditCardsForSettings() {
        ThreadUtils.assertOnUiThread();
        return getCreditCards(nativeGetCreditCardGUIDsForSettings(mPersonalDataManagerAndroid));
    }

    /**
     * Gets the credit cards to suggest when filling a form or completing a transaction. The cards
     * will have been processed to be more relevant to the user.
     * @param includeServerCards Whether server cards should be included in the response.
     */
    public ArrayList<CreditCard> getCreditCardsToSuggest(boolean includeServerCards) {
        ThreadUtils.assertOnUiThread();
        return getCreditCards(
                nativeGetCreditCardGUIDsToSuggest(mPersonalDataManagerAndroid, includeServerCards));
    }

    private ArrayList<CreditCard> getCreditCards(String[] creditCardGUIDs) {
        ArrayList<CreditCard> cards = new ArrayList<CreditCard>(creditCardGUIDs.length);
        for (int i = 0; i < creditCardGUIDs.length; i++) {
            cards.add(nativeGetCreditCardByGUID(mPersonalDataManagerAndroid, creditCardGUIDs[i]));
        }
        return cards;
    }

    public CreditCard getCreditCard(String guid) {
        ThreadUtils.assertOnUiThread();
        return nativeGetCreditCardByGUID(mPersonalDataManagerAndroid, guid);
    }

    public CreditCard getCreditCardForNumber(String cardNumber) {
        ThreadUtils.assertOnUiThread();
        return nativeGetCreditCardForNumber(mPersonalDataManagerAndroid, cardNumber);
    }

    public String setCreditCard(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        assert card.getIsLocal();
        return nativeSetCreditCard(mPersonalDataManagerAndroid, card);
    }

    public void updateServerCardBillingAddress(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        nativeUpdateServerCardBillingAddress(mPersonalDataManagerAndroid, card);
    }

    public String getBasicCardIssuerNetwork(String cardNumber, boolean emptyIfInvalid) {
        ThreadUtils.assertOnUiThread();
        return nativeGetBasicCardIssuerNetwork(
                mPersonalDataManagerAndroid, cardNumber, emptyIfInvalid);
    }

    @VisibleForTesting
    public void addServerCreditCardForTest(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        assert !card.getIsLocal();
        nativeAddServerCreditCardForTest(mPersonalDataManagerAndroid, card);
    }

    public void deleteCreditCard(String guid) {
        ThreadUtils.assertOnUiThread();
        nativeRemoveByGUID(mPersonalDataManagerAndroid, guid);
    }

    public void clearUnmaskedCache(String guid) {
        nativeClearUnmaskedCache(mPersonalDataManagerAndroid, guid);
    }

    public String getShippingAddressLabelWithCountryForPaymentRequest(AutofillProfile profile) {
        return nativeGetShippingAddressLabelWithCountryForPaymentRequest(
                mPersonalDataManagerAndroid, profile);
    }

    public String getShippingAddressLabelWithoutCountryForPaymentRequest(AutofillProfile profile) {
        return nativeGetShippingAddressLabelWithoutCountryForPaymentRequest(
                mPersonalDataManagerAndroid, profile);
    }

    public String getBillingAddressLabelForPaymentRequest(AutofillProfile profile) {
        return nativeGetBillingAddressLabelForPaymentRequest(mPersonalDataManagerAndroid, profile);
    }

    public void getFullCard(WebContents webContents, CreditCard card,
            FullCardRequestDelegate delegate) {
        nativeGetFullCardForPaymentRequest(mPersonalDataManagerAndroid, webContents, card,
                delegate);
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
        nativeRecordAndLogProfileUse(mPersonalDataManagerAndroid, guid);
    }

    @VisibleForTesting
    protected void setProfileUseStatsForTesting(String guid, int count, long date) {
        ThreadUtils.assertOnUiThread();
        nativeSetProfileUseStatsForTesting(mPersonalDataManagerAndroid, guid, count, date);
    }

    @VisibleForTesting
    int getProfileUseCountForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return nativeGetProfileUseCountForTesting(mPersonalDataManagerAndroid, guid);
    }

    @VisibleForTesting
    long getProfileUseDateForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return nativeGetProfileUseDateForTesting(mPersonalDataManagerAndroid, guid);
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
        nativeRecordAndLogCreditCardUse(mPersonalDataManagerAndroid, guid);
    }

    @VisibleForTesting
    protected void setCreditCardUseStatsForTesting(String guid, int count, long date) {
        ThreadUtils.assertOnUiThread();
        nativeSetCreditCardUseStatsForTesting(mPersonalDataManagerAndroid, guid, count, date);
    }

    @VisibleForTesting
    int getCreditCardUseCountForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return nativeGetCreditCardUseCountForTesting(mPersonalDataManagerAndroid, guid);
    }

    @VisibleForTesting
    long getCreditCardUseDateForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return nativeGetCreditCardUseDateForTesting(mPersonalDataManagerAndroid, guid);
    }

    @VisibleForTesting
    long getCurrentDateForTesting() {
        ThreadUtils.assertOnUiThread();
        return nativeGetCurrentDateForTesting(mPersonalDataManagerAndroid);
    }

    /**
     * Starts loading the address validation rules for the specified {@code regionCode}.
     *
     * @param regionCode The code of the region for which to load the rules.
     */
    public void loadRulesForAddressNormalization(String regionCode) {
        ThreadUtils.assertOnUiThread();
        nativeLoadRulesForAddressNormalization(mPersonalDataManagerAndroid, regionCode);
    }

    /**
     * Starts loading the sub-key request rules for the specified {@code regionCode}.
     *
     * @param regionCode The code of the region for which to load the rules.
     */
    public void loadRulesForSubKeys(String regionCode) {
        ThreadUtils.assertOnUiThread();
        nativeLoadRulesForSubKeys(mPersonalDataManagerAndroid, regionCode);
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
        nativeStartRegionSubKeysRequest(
                mPersonalDataManagerAndroid, regionCode, sRequestTimeoutSeconds, delegate);
    }

    /** Cancels the pending subkeys request. */
    public void cancelPendingGetSubKeys() {
        ThreadUtils.assertOnUiThread();
        nativeCancelPendingGetSubKeys(mPersonalDataManagerAndroid);
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
        nativeStartAddressNormalization(
                mPersonalDataManagerAndroid, profile, sRequestTimeoutSeconds, delegate);
    }

    /**
     * Checks whether the Autofill PersonalDataManager has profiles.
     *
     * @return True If there are profiles.
     */
    public boolean hasProfiles() {
        return nativeHasProfiles(mPersonalDataManagerAndroid);
    }

    /**
     * Checks whether the Autofill PersonalDataManager has credit cards.
     *
     * @return True If there are credit cards.
     */
    public boolean hasCreditCards() {
        return nativeHasCreditCards(mPersonalDataManagerAndroid);
    }

    /**
     * @return Whether the Autofill feature for Profiles (addresses) is enabled.
     */
    public static boolean isAutofillProfileEnabled() {
        return nativeGetPref(Pref.AUTOFILL_PROFILE_ENABLED);
    }

    /**
     * @return Whether the Autofill feature for Credit Cards is enabled.
     */
    public static boolean isAutofillCreditCardEnabled() {
        return nativeGetPref(Pref.AUTOFILL_CREDIT_CARD_ENABLED);
    }

    /**
     * Enables or disables the Autofill feature for Profiles.
     * @param enable True to disable profile Autofill, false otherwise.
     */
    public static void setAutofillProfileEnabled(boolean enable) {
        nativeSetPref(Pref.AUTOFILL_PROFILE_ENABLED, enable);
    }

    /**
     * Enables or disables the Autofill feature for Credit Cards.
     * @param enable True to disable credit card Autofill, false otherwise.
     */
    public static void setAutofillCreditCardEnabled(boolean enable) {
        nativeSetPref(Pref.AUTOFILL_CREDIT_CARD_ENABLED, enable);
    }

    /**
     * @return Whether the Autofill feature is managed.
     */
    public static boolean isAutofillManaged() {
        return nativeIsAutofillManaged();
    }

    /**
     * @return Whether the Autofill feature for Profiles (addresses) is managed.
     */
    public static boolean isAutofillProfileManaged() {
        return nativeIsAutofillProfileManaged();
    }

    /**
     * @return Whether the Autofill feature for Credit Cards is managed.
     */
    public static boolean isAutofillCreditCardManaged() {
        return nativeIsAutofillCreditCardManaged();
    }

    /**
     * @return Whether the Payments integration feature is enabled.
     */
    public static boolean isPaymentsIntegrationEnabled() {
        return nativeIsPaymentsIntegrationEnabled();
    }

    /**
     * Enables or disables the Payments integration.
     * @param enable True to enable Payments data import.
     */
    public static void setPaymentsIntegrationEnabled(boolean enable) {
        nativeSetPaymentsIntegrationEnabled(enable);
    }

    @VisibleForTesting
    public static void setRequestTimeoutForTesting(int timeout) {
        sRequestTimeoutSeconds = timeout;
    }

    @VisibleForTesting
    public void setSyncServiceForTesting() {
        nativeSetSyncServiceForTesting(mPersonalDataManagerAndroid);
    }

    /**
     * @return The sub-key request timeout in milliseconds.
     */
    public static long getRequestTimeoutMS() {
        return TimeUnit.SECONDS.toMillis(sRequestTimeoutSeconds);
    }

    private native long nativeInit();
    private native boolean nativeIsDataLoaded(long nativePersonalDataManagerAndroid);
    private native String[] nativeGetProfileGUIDsForSettings(long nativePersonalDataManagerAndroid);
    private native String[] nativeGetProfileGUIDsToSuggest(long nativePersonalDataManagerAndroid);
    private native String[] nativeGetProfileLabelsForSettings(
            long nativePersonalDataManagerAndroid);
    private native String[] nativeGetProfileLabelsToSuggest(long nativePersonalDataManagerAndroid,
            boolean includeNameInLabel, boolean includeOrganizationInLabel,
            boolean includeCountryInLabel);
    private native AutofillProfile nativeGetProfileByGUID(long nativePersonalDataManagerAndroid,
            String guid);
    private native String nativeSetProfile(
            long nativePersonalDataManagerAndroid, AutofillProfile profile);
    private native String nativeSetProfileToLocal(
            long nativePersonalDataManagerAndroid, AutofillProfile profile);
    private native String nativeGetShippingAddressLabelWithCountryForPaymentRequest(
            long nativePersonalDataManagerAndroid, AutofillProfile profile);
    private native String nativeGetShippingAddressLabelWithoutCountryForPaymentRequest(
            long nativePersonalDataManagerAndroid, AutofillProfile profile);
    private native String nativeGetBillingAddressLabelForPaymentRequest(
            long nativePersonalDataManagerAndroid, AutofillProfile profile);
    private native String[] nativeGetCreditCardGUIDsForSettings(
            long nativePersonalDataManagerAndroid);
    private native String[] nativeGetCreditCardGUIDsToSuggest(
            long nativePersonalDataManagerAndroid, boolean includeServerCards);
    private native CreditCard nativeGetCreditCardByGUID(long nativePersonalDataManagerAndroid,
            String guid);
    private native CreditCard nativeGetCreditCardForNumber(long nativePersonalDataManagerAndroid,
            String cardNumber);
    private native String nativeSetCreditCard(long nativePersonalDataManagerAndroid,
            CreditCard card);
    private native void nativeUpdateServerCardBillingAddress(long nativePersonalDataManagerAndroid,
            CreditCard card);
    private native String nativeGetBasicCardIssuerNetwork(
            long nativePersonalDataManagerAndroid, String cardNumber, boolean emptyIfInvalid);
    private native void nativeAddServerCreditCardForTest(long nativePersonalDataManagerAndroid,
            CreditCard card);
    private native void nativeRemoveByGUID(long nativePersonalDataManagerAndroid, String guid);
    private native void nativeRecordAndLogProfileUse(long nativePersonalDataManagerAndroid,
            String guid);
    private native void nativeSetProfileUseStatsForTesting(
            long nativePersonalDataManagerAndroid, String guid, int count, long date);
    private native int nativeGetProfileUseCountForTesting(long nativePersonalDataManagerAndroid,
            String guid);
    private native long nativeGetProfileUseDateForTesting(long nativePersonalDataManagerAndroid,
            String guid);
    private native void nativeRecordAndLogCreditCardUse(long nativePersonalDataManagerAndroid,
            String guid);
    private native void nativeSetCreditCardUseStatsForTesting(
            long nativePersonalDataManagerAndroid, String guid, int count, long date);
    private native int nativeGetCreditCardUseCountForTesting(long nativePersonalDataManagerAndroid,
            String guid);
    private native long nativeGetCreditCardUseDateForTesting(long nativePersonalDataManagerAndroid,
            String guid);
    private native long nativeGetCurrentDateForTesting(long nativePersonalDataManagerAndroid);
    private native void nativeClearUnmaskedCache(
            long nativePersonalDataManagerAndroid, String guid);
    private native void nativeGetFullCardForPaymentRequest(long nativePersonalDataManagerAndroid,
            WebContents webContents, CreditCard card, FullCardRequestDelegate delegate);
    private native void nativeLoadRulesForAddressNormalization(
            long nativePersonalDataManagerAndroid, String regionCode);
    private native void nativeLoadRulesForSubKeys(
            long nativePersonalDataManagerAndroid, String regionCode);
    private native void nativeStartAddressNormalization(long nativePersonalDataManagerAndroid,
            AutofillProfile profile, int timeoutSeconds, NormalizedAddressRequestDelegate delegate);
    private native void nativeStartRegionSubKeysRequest(long nativePersonalDataManagerAndroid,
            String regionCode, int timeoutSeconds, GetSubKeysRequestDelegate delegate);
    private static native boolean nativeHasProfiles(long nativePersonalDataManagerAndroid);
    private static native boolean nativeHasCreditCards(long nativePersonalDataManagerAndroid);
    private static native boolean nativeIsAutofillManaged();
    private static native boolean nativeIsAutofillProfileManaged();
    private static native boolean nativeIsAutofillCreditCardManaged();
    private static native boolean nativeIsPaymentsIntegrationEnabled();
    private static native void nativeSetPaymentsIntegrationEnabled(boolean enable);
    private static native String nativeToCountryCode(String countryName);
    private static native void nativeCancelPendingGetSubKeys(long nativePersonalDataManagerAndroid);
    private static native void nativeSetSyncServiceForTesting(
            long nativePersonalDataManagerAndroid);
    private static native boolean nativeGetPref(int preference);
    private static native void nativeSetPref(int preference, boolean enable);
}
