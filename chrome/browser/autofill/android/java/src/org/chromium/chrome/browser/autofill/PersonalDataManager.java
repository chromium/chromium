// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;
import java.util.Optional;

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

    private final long mPersonalDataManagerAndroid;
    private final List<PersonalDataManagerObserver> mDataObservers =
            new ArrayList<PersonalDataManagerObserver>();
    private AutofillImageFetcher mImageFetcher;

    private PersonalDataManager() {
        // Note that this technically leaks the native object, however, PersonalDataManager
        // is a singleton that lives forever and there's no clean shutdown of Chrome on Android
        mPersonalDataManagerAndroid = PersonalDataManagerJni.get().init(PersonalDataManager.this);
        // Get the AutofillImageFetcher instance that was created during browser startup.
        mImageFetcher = PersonalDataManagerJni.get().getOrCreateJavaImageFetcher(
                mPersonalDataManagerAndroid);
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
            AutofillProfile profile = new AutofillProfile(
                    PersonalDataManagerJni.get().getProfileByGUID(mPersonalDataManagerAndroid,
                            PersonalDataManager.this, profileGUIDs[i]));
            profile.setLabel(profileLabels[i]);
            profiles.add(profile);
        }

        return profiles;
    }

    public AutofillProfile getProfile(String guid) {
        ThreadUtils.assertOnUiThread();
        return new AutofillProfile(PersonalDataManagerJni.get().getProfileByGUID(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid));
    }

    public void deleteProfile(String guid) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().removeByGUID(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    public String setProfile(AutofillProfile profile) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().setProfile(
                mPersonalDataManagerAndroid, PersonalDataManager.this, profile, profile.getGUID());
    }

    public String setProfileToLocal(AutofillProfile profile) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().setProfileToLocal(
                mPersonalDataManagerAndroid, PersonalDataManager.this, profile, profile.getGUID());
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

    public void addServerCreditCardForTest(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        assert !card.getIsLocal();
        PersonalDataManagerJni.get().addServerCreditCardForTest(
                mPersonalDataManagerAndroid, PersonalDataManager.this, card);
    }

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

    protected void setProfileUseStatsForTesting(String guid, int count, int daysSinceLastUsed) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().setProfileUseStatsForTesting(mPersonalDataManagerAndroid,
                PersonalDataManager.this, guid, count, daysSinceLastUsed);
    }

    int getProfileUseCountForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getProfileUseCountForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

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

    protected void setCreditCardUseStatsForTesting(String guid, int count, int daysSinceLastUsed) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().setCreditCardUseStatsForTesting(mPersonalDataManagerAndroid,
                PersonalDataManager.this, guid, count, daysSinceLastUsed);
    }

    int getCreditCardUseCountForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCreditCardUseCountForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    long getCreditCardUseDateForTesting(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCreditCardUseDateForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this, guid);
    }

    long getCurrentDateForTesting() {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCurrentDateForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this);
    }

    long getDateNDaysAgoForTesting(int days) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getDateNDaysAgoForTesting( // IN-TEST
                mPersonalDataManagerAndroid, PersonalDataManager.this, days);
    }

    protected void clearServerDataForTesting() {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().clearServerDataForTesting(
                mPersonalDataManagerAndroid, PersonalDataManager.this);
    }

    protected void clearImageDataForTesting() {
        if (mImageFetcher == null) {
            return;
        }

        ThreadUtils.assertOnUiThread();
        mImageFetcher.clearCachedImagesForTesting();
    }

    public static void setInstanceForTesting(PersonalDataManager manager) {
        var oldValue = sManager;
        sManager = manager;
        ResettersForTesting.register(() -> sManager = oldValue);
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
    public static boolean isPaymentMethodsMandatoryReauthEnabled() {
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
     * @return Whether the Autofill feature for payment cvc storage is enabled.
     */
    public static boolean isPaymentCvcStorageEnabled() {
        return getPrefService().getBoolean(Pref.AUTOFILL_PAYMENT_CVC_STORAGE);
    }

    /**
     * Enables or disables the Autofill feature for payment cvc storage.
     * @param enable True to enable payment cvc storage, false otherwise.
     */
    public static void setAutofillPaymentCvcStorage(boolean enable) {
        getPrefService().setBoolean(Pref.AUTOFILL_PAYMENT_CVC_STORAGE, enable);
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

    public void setSyncServiceForTesting() {
        PersonalDataManagerJni.get().setSyncServiceForTesting(mPersonalDataManagerAndroid);
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    private void fetchCreditCardArtImages() {
        mImageFetcher.prefetchImages(getCreditCardsToSuggest()
                                             .stream()
                                             .map(card -> card.getCardArtUrl())
                                             .toArray(GURL[] ::new));
    }

    /**
     * Return the card art image for the given `customImageUrl`.
     * @param customImageUrl  URL of the image. If the image is available, it is returned, otherwise
     *         it is fetched from this URL.
     * @param cardIconSpecs {@code CardIconSpecs} instance containing the specs for the card icon.
     * @return Bitmap image if found in the local cache, else return an empty object.
     */
    public Optional<Bitmap> getCustomImageForAutofillSuggestionIfAvailable(
            GURL customImageUrl, AutofillUiUtils.CardIconSpecs cardIconSpecs) {
        return mImageFetcher.getImageIfAvailable(customImageUrl, cardIconSpecs);
    }

    public void setImageFetcherForTesting(ImageFetcher imageFetcher) {
        var oldValue = this.mImageFetcher;
        this.mImageFetcher = new AutofillImageFetcher(imageFetcher);
        ResettersForTesting.register(() -> this.mImageFetcher = oldValue);
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
                AutofillProfile profile, String guid);
        String setProfileToLocal(long nativePersonalDataManagerAndroid, PersonalDataManager caller,
                AutofillProfile profile, String guid);
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
        long getDateNDaysAgoForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, int days);
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
                PersonalDataManager caller, String guid, int count, int daysSinceLastUsed);
        int getProfileUseCountForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        long getProfileUseDateForTesting(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        void recordAndLogCreditCardUse(
                long nativePersonalDataManagerAndroid, PersonalDataManager caller, String guid);
        void setCreditCardUseStatsForTesting(long nativePersonalDataManagerAndroid,
                PersonalDataManager caller, String guid, int count, int daysSinceLastUsed);
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
        boolean hasProfiles(long nativePersonalDataManagerAndroid);
        boolean hasCreditCards(long nativePersonalDataManagerAndroid);
        boolean isFidoAuthenticationAvailable(long nativePersonalDataManagerAndroid);
        boolean isAutofillManaged();
        boolean isAutofillProfileManaged();
        boolean isAutofillCreditCardManaged();
        String toCountryCode(String countryName);
        void setSyncServiceForTesting(long nativePersonalDataManagerAndroid);
        AutofillImageFetcher getOrCreateJavaImageFetcher(long nativePersonalDataManagerAndroid);
    }
}
