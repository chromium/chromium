// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.IbanRecordType;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.BnplIssuerForSettings;
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;
import java.util.Objects;

/**
 * Android wrapper of the PersonalDataManager which provides access from the Java layer.
 *
 * <p>Only usable from the UI thread as it's primary purpose is for supporting the Android
 * preferences UI.
 *
 * <p>See chrome/browser/autofill/personal_data_manager.h for more details.
 */
@NullMarked
@JNINamespace("autofill")
public class PersonalDataManager implements Destroyable {
    private static final String TAG = "PersonalDataManager";

    @VisibleForTesting
    static final String AUTOFILL_ADDRESS_OPT_IN_CHANGE_HISTOGRAM_NAME =
            "Autofill.Address.IsEnabled.Change";

    // Enum to represent the Autofill address opt-in changes.
    //
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(AutofillAddressOptInChange)
    @VisibleForTesting
    @IntDef({AutofillAddressOptInChange.OPT_IN, AutofillAddressOptInChange.OPT_OUT})
    @Retention(RetentionPolicy.SOURCE)
    @interface AutofillAddressOptInChange {
        int OPT_IN = 0;
        int OPT_OUT = 1;
        int HISTOGRAM_BUCKET_COUNT = 2;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:AutofillAddressOptInChange)

    /** Observer of PersonalDataManager events. */
    public interface PersonalDataManagerObserver {
        /** Called when the data is changed. */
        void onPersonalDataChanged();
    }

    /** Autofill credit card information. */
    public static class CreditCard {
        // Note that while some of these fields are numbers, they're predominantly read,
        // marshaled and compared as strings. To save conversions, we sometimes use strings.
        private String mGUID;
        private String mOrigin;
        private final boolean mIsLocal;
        private final boolean mIsVirtual;
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
        private @Nullable GURL mCardArtUrl;
        private String mCvc;
        private final String mIssuerId;
        private final String mBenefitSource;
        private final @Nullable GURL mProductTermsUrl;
        private final @VirtualCardEnrollmentState int mVirtualCardEnrollmentState;
        private final String mProductDescription;
        private final String mCardNameForAutofillDisplay;
        private final String mObfuscatedLastFourDigits;

        @CalledByNative("CreditCard")
        public static CreditCard create(
                @JniType("std::string") String guid,
                @JniType("std::string") String origin,
                boolean isLocal,
                boolean isVirtual,
                @JniType("std::u16string") String name,
                @JniType("std::u16string") String number,
                @JniType("std::u16string") String networkAndLastFourDigits,
                @JniType("std::u16string") String month,
                @JniType("std::u16string") String year,
                @JniType("std::string") String basicCardIssuerNetwork,
                int iconId,
                @JniType("std::string") String billingAddressId,
                @JniType("std::string") String serverId,
                long instrumentId,
                @JniType("std::u16string") String cardLabel,
                @JniType("std::u16string") String nickname,
                GURL cardArtUrl,
                @VirtualCardEnrollmentState int virtualCardEnrollmentState,
                @JniType("std::u16string") String productDescription,
                @JniType("std::u16string") String cardNameForAutofillDisplay,
                @JniType("std::u16string") String obfuscatedLastFourDigits,
                @JniType("std::u16string") String cvc,
                @JniType("std::string") String issuerId,
                @JniType("std::string") String benefitSource,
                GURL productTermsUrl) {
            return new CreditCard(
                    guid,
                    origin,
                    isLocal,
                    isVirtual,
                    name,
                    number,
                    networkAndLastFourDigits,
                    month,
                    year,
                    basicCardIssuerNetwork,
                    iconId,
                    billingAddressId,
                    serverId,
                    instrumentId,
                    cardLabel,
                    nickname,
                    cardArtUrl,
                    virtualCardEnrollmentState,
                    productDescription,
                    cardNameForAutofillDisplay,
                    obfuscatedLastFourDigits,
                    cvc,
                    issuerId,
                    benefitSource,
                    productTermsUrl);
        }

        public CreditCard(
                String guid,
                String origin,
                boolean isLocal,
                String name,
                String number,
                String networkAndLastFourDigits,
                String month,
                String year,
                String basicCardIssuerNetwork,
                int issuerIconDrawableId,
                String billingAddressId,
                String serverId) {
            this(
                    guid,
                    origin,
                    isLocal,
                    /* isVirtual= */ false,
                    name,
                    number,
                    networkAndLastFourDigits,
                    month,
                    year,
                    basicCardIssuerNetwork,
                    issuerIconDrawableId,
                    billingAddressId,
                    serverId,
                    /* instrumentId= */ 0,
                    /* cardLabel= */ networkAndLastFourDigits,
                    /* nickname= */ "",
                    /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.UNSPECIFIED,
                    /* productDescription= */ "",
                    /* cardNameForAutofillDisplay= */ "",
                    /* obfuscatedLastFourDigits= */ "",
                    /* cvc= */ "",
                    /* issuerId= */ "",
                    /* benefitSource= */ "",
                    /* productTermsUrl= */ null);
        }

        public CreditCard(
                String guid,
                String origin,
                boolean isLocal,
                boolean isVirtual,
                String name,
                String number,
                String networkAndLastFourDigits,
                String month,
                String year,
                String basicCardIssuerNetwork,
                int issuerIconDrawableId,
                String billingAddressId,
                String serverId,
                long instrumentId,
                String cardLabel,
                String nickname,
                @Nullable GURL cardArtUrl,
                @VirtualCardEnrollmentState int virtualCardEnrollmentState,
                String productDescription,
                String cardNameForAutofillDisplay,
                String obfuscatedLastFourDigits,
                String cvc,
                String issuerId,
                String benefitSource,
                @Nullable GURL productTermsUrl) {
            mGUID = guid;
            mOrigin = origin;
            mIsLocal = isLocal;
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
            mCvc = cvc;
            mIssuerId = issuerId;
            mBenefitSource = benefitSource;
            mProductTermsUrl = productTermsUrl;
        }

        public CreditCard() {
            this(
                    /* guid= */ "",
                    /* origin= */ AutofillEditorBase.SETTINGS_ORIGIN,
                    /* isLocal= */ true,
                    /* name= */ "",
                    /* number= */ "",
                    /* networkAndLastFourDigits= */ "",
                    /* month= */ "",
                    /* year= */ "",
                    /* basicCardIssuerNetwork= */ "",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "");
        }

        @CalledByNative("CreditCard")
        public @JniType("std::string") String getGUID() {
            return mGUID;
        }

        @CalledByNative("CreditCard")
        public @JniType("std::string") String getOrigin() {
            return mOrigin;
        }

        @CalledByNative("CreditCard")
        public @JniType("std::u16string") String getName() {
            return mName;
        }

        @CalledByNative("CreditCard")
        public @JniType("std::u16string") String getNumber() {
            return mNumber;
        }

        public String getNetworkAndLastFourDigits() {
            return mNetworkAndLastFourDigits;
        }

        @CalledByNative("CreditCard")
        public @JniType("std::u16string") String getMonth() {
            return mMonth;
        }

        @CalledByNative("CreditCard")
        public @JniType("std::u16string") String getYear() {
            return mYear;
        }

        public String getFormattedExpirationDate(Context context) {
            String twoDigityear = getYear().substring(2);
            return getMonth()
                    + context.getString(R.string.autofill_expiration_date_separator)
                    + twoDigityear;
        }

        public String getFormattedExpirationDateWithCvcSavedMessage(Context context) {
            return context.getString(
                    R.string.autofill_settings_page_summary_separated_by_pipe,
                    getFormattedExpirationDate(context),
                    context.getString(R.string.autofill_settings_page_cvc_saved_label));
        }

        @CalledByNative("CreditCard")
        public boolean getIsLocal() {
            return mIsLocal;
        }

        @CalledByNative("CreditCard")
        public boolean getIsVirtual() {
            return mIsVirtual;
        }

        @CalledByNative("CreditCard")
        public @JniType("std::string") String getBasicCardIssuerNetwork() {
            return mBasicCardIssuerNetwork;
        }

        public int getIssuerIconDrawableId() {
            return mIssuerIconDrawableId;
        }

        @CalledByNative("CreditCard")
        public @JniType("std::string") String getBillingAddressId() {
            return mBillingAddressId;
        }

        @CalledByNative("CreditCard")
        public @JniType("std::string") String getServerId() {
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
        public @JniType("std::u16string") String getNickname() {
            return mNickname;
        }

        @CalledByNative("CreditCard")
        public @Nullable GURL getCardArtUrl() {
            return mCardArtUrl;
        }

        @CalledByNative("CreditCard")
        public @VirtualCardEnrollmentState int getVirtualCardEnrollmentState() {
            return mVirtualCardEnrollmentState;
        }

        @CalledByNative("CreditCard")
        public @JniType("std::u16string") String getProductDescription() {
            return mProductDescription;
        }

        @CalledByNative("CreditCard")
        public @JniType("std::u16string") String getCvc() {
            return mCvc;
        }

        @CalledByNative("CreditCard")
        public String getIssuerId() {
            return mIssuerId;
        }

        @CalledByNative("CreditCard")
        public String getBenefitSource() {
            return mBenefitSource;
        }

        @CalledByNative("CreditCard")
        public @Nullable GURL getProductTermsUrl() {
            return mProductTermsUrl;
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

        public void setCvc(String cvc) {
            mCvc = cvc;
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

    /** Autofill IBAN information. */
    public static class Iban {
        private final @Nullable String mGuid;
        private final @Nullable Long mInstrumentId;

        // Obfuscated IBAN value. This is used for displaying the IBAN in the Payment methods page.
        private final String mLabel;

        private String mNickname;
        private final @IbanRecordType int mRecordType;
        // Value is empty for server IBAN.
        private @Nullable String mValue;

        private Iban(
                String guid,
                Long instrumentId,
                String label,
                String nickname,
                @IbanRecordType int recordType,
                String value) {
            mGuid = guid;
            mInstrumentId = instrumentId;
            mLabel = Objects.requireNonNull(label, "Label can't be null");
            mNickname = Objects.requireNonNull(nickname, "Nickname can't be null");
            mRecordType = recordType;
            mValue = value;
        }

        // Creates an Iban instance that is not stored on a server nor locally,
        // yet. This Iban has type IbanRecordType.UNKNOWN and has neither a
        // Guid nor an instrumentId.
        @CalledByNative("Iban")
        public static Iban createEphemeral(
                @JniType("std::u16string") String label,
                @JniType("std::u16string") String nickname,
                @JniType("std::u16string") String value) {
            return new Iban.Builder()
                    .setLabel(label)
                    .setNickname(nickname)
                    .setRecordType(IbanRecordType.UNKNOWN)
                    .setValue(value)
                    .build();
        }

        @CalledByNative("Iban")
        public static Iban createLocal(
                @JniType("std::string") String guid,
                @JniType("std::u16string") String label,
                @JniType("std::u16string") String nickname,
                @JniType("std::u16string") String value) {
            return new Iban.Builder()
                    .setGuid(guid)
                    .setLabel(label)
                    .setNickname(nickname)
                    .setRecordType(IbanRecordType.LOCAL_IBAN)
                    .setValue(value)
                    .build();
        }

        @CalledByNative("Iban")
        public static Iban createServer(
                long instrumentId,
                @JniType("std::u16string") String label,
                @JniType("std::u16string") String nickname,
                @JniType("std::u16string") String value) {
            return new Iban.Builder()
                    .setInstrumentId(Long.valueOf(instrumentId))
                    .setLabel(label)
                    .setNickname(nickname)
                    .setRecordType(IbanRecordType.SERVER_IBAN)
                    .setValue(value)
                    .build();
        }

        @CalledByNative("Iban")
        public @Nullable @JniType("std::string") String getGuid() {
            assert mRecordType != IbanRecordType.SERVER_IBAN;
            return mGuid;
        }

        @CalledByNative("Iban")
        public long getInstrumentId() {
            assert mInstrumentId != null;
            assert mRecordType == IbanRecordType.SERVER_IBAN;
            return mInstrumentId;
        }

        public String getLabel() {
            return mLabel;
        }

        @CalledByNative("Iban")
        public @JniType("std::u16string") String getNickname() {
            return mNickname;
        }

        @CalledByNative("Iban")
        public @IbanRecordType int getRecordType() {
            return mRecordType;
        }

        @CalledByNative("Iban")
        public @Nullable @JniType("std::u16string") String getValue() {
            return mValue;
        }

        public void updateNickname(String nickname) {
            mNickname = nickname;
        }

        public void updateValue(String value) {
            mValue = value;
        }

        @Override
        public boolean equals(Object obj) {

            if (this == obj) return true;
            if (!(obj instanceof Iban)) return false;

            Iban otherIban = (Iban) obj;

            return Objects.equals(mLabel, otherIban.getLabel())
                    && Objects.equals(mNickname, otherIban.getNickname())
                    && mRecordType == otherIban.getRecordType()
                    && (mRecordType != IbanRecordType.SERVER_IBAN
                            || Objects.equals(mInstrumentId, otherIban.getInstrumentId()))
                    && (mRecordType != IbanRecordType.LOCAL_IBAN
                            || Objects.equals(mGuid, otherIban.getGuid()))
                    && Objects.equals(mValue, otherIban.getValue());
        }

        @Override
        public int hashCode() {
            return Objects.hash(mGuid, mLabel, mNickname, mRecordType, mValue);
        }

        /** Builder for {@link Iban}. */
        public static final class Builder {
            private @Nullable String mGuid;
            private @Nullable Long mInstrumentId;
            private @Nullable String mLabel;
            private @Nullable String mNickname;
            private @IbanRecordType int mRecordType;
            private @Nullable String mValue;

            public Builder setGuid(String guid) {
                mGuid = guid;
                return this;
            }

            public Builder setInstrumentId(Long instrumentId) {
                mInstrumentId = instrumentId;
                return this;
            }

            public Builder setLabel(String label) {
                mLabel = label;
                return this;
            }

            public Builder setNickname(String nickname) {
                mNickname = nickname;
                return this;
            }

            public Builder setRecordType(@IbanRecordType int recordType) {
                mRecordType = recordType;
                return this;
            }

            public Builder setValue(String value) {
                mValue = value;
                return this;
            }

            public Iban build() {
                switch (mRecordType) {
                    case IbanRecordType.UNKNOWN:
                        assert mGuid == null && mInstrumentId == null
                                : "IBANs with 'UNKNOWN' record type must have an empty GUID and"
                                        + " InstrumentId.";
                        break;
                    case IbanRecordType.LOCAL_IBAN:
                        assert !TextUtils.isEmpty(mGuid) && mInstrumentId == null
                                : "Local IBANs must have a non-empty GUID and null InstrumentID.";
                        break;
                    case IbanRecordType.SERVER_IBAN:
                        assert mInstrumentId != null
                                        && mInstrumentId != 0L
                                        && TextUtils.isEmpty(mGuid)
                                        && TextUtils.isEmpty(mValue)
                                : "Server IBANs must have a non-zero instrumentId, empty GUID and"
                                        + " empty value.";
                        break;
                }
                // Non-null enforcement happens inside the constructor if applicable, assume
                // non-null for all fields.
                return new Iban(
                        assumeNonNull(mGuid),
                        assumeNonNull(mInstrumentId),
                        assumeNonNull(mLabel),
                        assumeNonNull(mNickname),
                        mRecordType,
                        assumeNonNull(mValue));
            }
        }
    }

    private final PrefService mPrefService;
    private final List<PersonalDataManagerObserver> mDataObservers = new ArrayList<>();

    private long mPersonalDataManagerAndroid;

    PersonalDataManager(Profile profile) {
        mPersonalDataManagerAndroid = PersonalDataManagerJni.get().init(this, profile);
        mPrefService = UserPrefs.get(profile);
    }

    @Override
    public void destroy() {
        PersonalDataManagerJni.get().destroy(mPersonalDataManagerAndroid);
        mPersonalDataManagerAndroid = 0;
    }

    /** Called from native when template URL service is done loading. */
    @CalledByNative
    private void personalDataChanged() {
        ThreadUtils.assertOnUiThread();
        for (PersonalDataManagerObserver observer : mDataObservers) {
            observer.onPersonalDataChanged();
        }
    }

    /** Registers a PersonalDataManagerObserver on the native side. */
    public boolean registerDataObserver(PersonalDataManagerObserver observer) {
        ThreadUtils.assertOnUiThread();
        assert !mDataObservers.contains(observer);
        mDataObservers.add(observer);
        return PersonalDataManagerJni.get().isDataLoaded(mPersonalDataManagerAndroid);
    }

    /** Unregisters the provided observer. */
    public void unregisterDataObserver(PersonalDataManagerObserver observer) {
        ThreadUtils.assertOnUiThread();
        assert (mDataObservers.size() > 0);
        assert mDataObservers.contains(observer);
        mDataObservers.remove(observer);
    }

    /**
     * TODO(crbug.com/41256488): Reduce the number of Java to Native calls when getting profiles.
     *
     * <p>Gets the profiles to show in the settings page. Returns all the profiles without any
     * processing.
     *
     * @return The list of profiles to show in the settings.
     */
    public List<AutofillProfile> getProfilesForSettings() {
        ThreadUtils.assertOnUiThread();
        return getProfilesWithLabels(
                PersonalDataManagerJni.get()
                        .getProfileLabelsForSettings(mPersonalDataManagerAndroid),
                PersonalDataManagerJni.get()
                        .getProfileGUIDsForSettings(mPersonalDataManagerAndroid));
    }

    public String getProfileDescriptionForEditor(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get()
                .getProfileDescriptionForEditor(mPersonalDataManagerAndroid, guid);
    }

    /**
     * TODO(crbug.com/41256488): Reduce the number of Java to Native calls when getting profiles
     *
     * <p>Gets the profiles to suggest when filling a form or completing a transaction. The profiles
     * will have been processed to be more relevant to the user.
     *
     * @return The list of profiles to suggest to the user.
     */
    public ArrayList<AutofillProfile> getProfilesToSuggest() {
        ThreadUtils.assertOnUiThread();
        return getProfilesWithLabels(
                PersonalDataManagerJni.get().getProfileLabelsToSuggest(mPersonalDataManagerAndroid),
                PersonalDataManagerJni.get().getProfileGUIDsToSuggest(mPersonalDataManagerAndroid));
    }

    private ArrayList<AutofillProfile> getProfilesWithLabels(
            String[] profileLabels, String[] profileGUIDs) {
        ArrayList<AutofillProfile> profiles = new ArrayList<>(profileGUIDs.length);
        for (int i = 0; i < profileGUIDs.length; i++) {
            AutofillProfile profile =
                    new AutofillProfile(
                            PersonalDataManagerJni.get()
                                    .getProfileByGUID(
                                            mPersonalDataManagerAndroid, profileGUIDs[i]));
            profile.setLabel(profileLabels[i]);
            profiles.add(profile);
        }

        return profiles;
    }

    public AutofillProfile getProfile(String guid) {
        ThreadUtils.assertOnUiThread();
        return new AutofillProfile(
                PersonalDataManagerJni.get().getProfileByGUID(mPersonalDataManagerAndroid, guid));
    }

    public void deleteProfile(String guid) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().removeProfile(mPersonalDataManagerAndroid, guid);
    }

    public String setProfile(AutofillProfile profile) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get()
                .setProfile(mPersonalDataManagerAndroid, profile, profile.getGUID());
    }

    public String setProfileToLocal(AutofillProfile profile) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get()
                .setProfileToLocal(mPersonalDataManagerAndroid, profile, profile.getGUID());
    }

    /** Gets the number of credit cards for the settings page. */
    public int getCreditCardCountForSettings() {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get()
                .getCreditCardGUIDsForSettings(mPersonalDataManagerAndroid)
                .length;
    }

    /**
     * Gets the credit cards to show in the settings page. Returns all the cards without any
     * processing.
     */
    public List<CreditCard> getCreditCardsForSettings() {
        ThreadUtils.assertOnUiThread();
        return getCreditCards(
                PersonalDataManagerJni.get()
                        .getCreditCardGUIDsForSettings(mPersonalDataManagerAndroid));
    }

    /**
     * Gets the credit cards to suggest when filling a form or completing a transaction. The cards
     * will have been processed to be more relevant to the user.
     */
    public ArrayList<CreditCard> getCreditCardsToSuggest() {
        ThreadUtils.assertOnUiThread();
        return getCreditCards(
                PersonalDataManagerJni.get()
                        .getCreditCardGUIDsToSuggest(mPersonalDataManagerAndroid));
    }

    private ArrayList<CreditCard> getCreditCards(String[] creditCardGUIDs) {
        ArrayList<CreditCard> cards = new ArrayList<>(creditCardGUIDs.length);
        for (int i = 0; i < creditCardGUIDs.length; i++) {
            cards.add(
                    PersonalDataManagerJni.get()
                            .getCreditCardByGUID(mPersonalDataManagerAndroid, creditCardGUIDs[i]));
        }
        return cards;
    }

    public CreditCard getCreditCard(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getCreditCardByGUID(mPersonalDataManagerAndroid, guid);
    }

    public CreditCard getCreditCardForNumber(String cardNumber) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get()
                .getCreditCardForNumber(mPersonalDataManagerAndroid, cardNumber);
    }

    public String setCreditCard(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        assert card.getIsLocal();
        return PersonalDataManagerJni.get().setCreditCard(mPersonalDataManagerAndroid, card);
    }

    public void updateServerCardBillingAddress(CreditCard card) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get()
                .updateServerCardBillingAddress(mPersonalDataManagerAndroid, card);
    }

    public static String getBasicCardIssuerNetwork(String cardNumber, boolean emptyIfInvalid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getBasicCardIssuerNetwork(cardNumber, emptyIfInvalid);
    }

    public void deleteCreditCard(String guid) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().removeByGUID(mPersonalDataManagerAndroid, guid);
    }

    /** Deletes all local credit cards. */
    public void deleteAllLocalCreditCards() {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().deleteAllLocalCreditCards(mPersonalDataManagerAndroid);
    }

    public String getShippingAddressLabelWithCountryForPaymentRequest(AutofillProfile profile) {
        return PersonalDataManagerJni.get()
                .getShippingAddressLabelForPaymentRequest(
                        mPersonalDataManagerAndroid,
                        profile,
                        profile.getGUID(),
                        /* includeCountry= */ true);
    }

    public String getShippingAddressLabelWithoutCountryForPaymentRequest(AutofillProfile profile) {
        return PersonalDataManagerJni.get()
                .getShippingAddressLabelForPaymentRequest(
                        mPersonalDataManagerAndroid,
                        profile,
                        profile.getGUID(),
                        /* includeCountry= */ false);
    }

    public void addServerIbanForTest(Iban iban) {
        ThreadUtils.assertOnUiThread();
        assert iban.getRecordType() == IbanRecordType.SERVER_IBAN;
        PersonalDataManagerJni.get()
                .addServerIbanForTest(mPersonalDataManagerAndroid, iban); // IN-TEST
    }

    public Iban getIban(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getIbanByGuid(mPersonalDataManagerAndroid, guid);
    }

    public Iban[] getIbansForSettings() {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getIbansForSettings(mPersonalDataManagerAndroid);
    }

    public String addOrUpdateLocalIban(Iban iban) {
        ThreadUtils.assertOnUiThread();
        assert iban.getRecordType() == IbanRecordType.UNKNOWN
                        || iban.getRecordType() == IbanRecordType.LOCAL_IBAN
                : "Add or update local IBANs only.";
        return PersonalDataManagerJni.get().addOrUpdateLocalIban(mPersonalDataManagerAndroid, iban);
    }

    public void deleteIban(String guid) {
        ThreadUtils.assertOnUiThread();
        PersonalDataManagerJni.get().removeByGUID(mPersonalDataManagerAndroid, guid);
    }

    public boolean isValidIban(String ibanValue) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().isValidIban(mPersonalDataManagerAndroid, ibanValue);
    }

    public boolean shouldShowAddIbanButtonOnSettingsPage() {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get()
                .shouldShowAddIbanButtonOnSettingsPage(mPersonalDataManagerAndroid);
    }

    public BankAccount[] getMaskedBankAccounts() {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getMaskedBankAccounts(mPersonalDataManagerAndroid);
    }

    public Ewallet[] getEwallets() {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getEwallets(mPersonalDataManagerAndroid);
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
        PersonalDataManagerJni.get().recordAndLogProfileUse(mPersonalDataManagerAndroid, guid);
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
        PersonalDataManagerJni.get().recordAndLogCreditCardUse(mPersonalDataManagerAndroid, guid);
    }

    /**
     * Determines whether the logged in user (if any) is eligible to store Autofill address profiles
     * to their account.
     */
    public boolean isEligibleForAddressAccountStorage() {
        return PersonalDataManagerJni.get()
                .isEligibleForAddressAccountStorage(mPersonalDataManagerAndroid);
    }

    /** Determines the country code for a newly created address profile. */
    public String getDefaultCountryCodeForNewAddress() {
        return PersonalDataManagerJni.get()
                .getDefaultCountryCodeForNewAddress(mPersonalDataManagerAndroid);
    }

    /**
     * @return Whether the Autofill feature for Profiles (addresses) is enabled.
     */
    public boolean isAutofillProfileEnabled() {
        return mPrefService.getBoolean(Pref.AUTOFILL_PROFILE_ENABLED);
    }

    /**
     * @return Whether the Autofill feature for Payment Methods is enabled.
     */
    public boolean isAutofillPaymentMethodsEnabled() {
        // TODO(crbug.com/40903277): Rename pref to AUTOFILL_PAYMENT_METHODS_ENABLED.
        return mPrefService.getBoolean(Pref.AUTOFILL_CREDIT_CARD_ENABLED);
    }

    /**
     * Enables or disables the Autofill feature for Profiles.
     *
     * @param enable True to disable profile Autofill, false otherwise.
     */
    public void setAutofillProfileEnabled(boolean enable) {
        mPrefService.setBoolean(Pref.AUTOFILL_PROFILE_ENABLED, enable);
        RecordHistogram.recordEnumeratedHistogram(
                AUTOFILL_ADDRESS_OPT_IN_CHANGE_HISTOGRAM_NAME,
                enable ? AutofillAddressOptInChange.OPT_IN : AutofillAddressOptInChange.OPT_OUT,
                AutofillAddressOptInChange.HISTOGRAM_BUCKET_COUNT);
    }

    /**
     * Enables or disables the Autofill feature for Credit Cards.
     *
     * @param enable True to disable credit card Autofill, false otherwise.
     */
    public void setAutofillCreditCardEnabled(boolean enable) {
        mPrefService.setBoolean(Pref.AUTOFILL_CREDIT_CARD_ENABLED, enable);
    }

    /**
     * @return Whether the Autofill feature for payment methods mandatory reauth is enabled.
     */
    public boolean isPaymentMethodsMandatoryReauthEnabled() {
        return mPrefService.getBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH);
    }

    /**
     * Enables or disables the Autofill feature for payment methods mandatory reauth.
     *
     * @param enable True to enable payment methods mandatory reauth, false otherwise.
     */
    public void setAutofillPaymentMethodsMandatoryReauth(boolean enable) {
        mPrefService.setBoolean(Pref.AUTOFILL_PAYMENT_METHODS_MANDATORY_REAUTH, enable);
    }

    /**
     * @return Whether the Autofill feature for payment cvc storage is enabled.
     */
    public boolean isPaymentCvcStorageEnabled() {
        return mPrefService.getBoolean(Pref.AUTOFILL_PAYMENT_CVC_STORAGE);
    }

    /**
     * Enables or disables the Autofill feature for payment cvc storage.
     *
     * @param enable True to enable payment cvc storage, false otherwise.
     */
    public void setAutofillPaymentCvcStorage(boolean enable) {
        mPrefService.setBoolean(Pref.AUTOFILL_PAYMENT_CVC_STORAGE, enable);
    }

    /**
     * @return Whether the card benefit showing feature is enabled.
     */
    public boolean isCardBenefitEnabled() {
        return mPrefService.getBoolean(Pref.AUTOFILL_PAYMENT_CARD_BENEFITS);
    }

    /**
     * @param guid The GUID of the credit card.
     * @return Whether the card is eligible for benefits, based on its `guid`.
     */
    public boolean isCardEligibleForBenefits(String guid) {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get()
                .isCardEligibleForBenefits(mPersonalDataManagerAndroid, guid);
    }

    /**
     * Enables or disables the card benefit showing feature.
     *
     * @param enable True to enable showing card benefits, false otherwise.
     */
    public void setCardBenefit(boolean enable) {
        mPrefService.setBoolean(Pref.AUTOFILL_PAYMENT_CARD_BENEFITS, enable);
    }

    /**
     * @return Whether the Autofill feature for Profiles (addresses) is managed.
     */
    public boolean isAutofillProfileManaged() {
        return PersonalDataManagerJni.get().isAutofillProfileManaged(mPersonalDataManagerAndroid);
    }

    /**
     * @return Whether the Autofill feature for Credit Cards is managed.
     */
    public boolean isAutofillCreditCardManaged() {
        return PersonalDataManagerJni.get()
                .isAutofillCreditCardManaged(mPersonalDataManagerAndroid);
    }

    /** Sets the preference value for supporting payments using Pix. */
    public void setFacilitatedPaymentsPixPref(boolean value) {
        mPrefService.setBoolean(Pref.FACILITATED_PAYMENTS_PIX, value);
    }

    /** Returns the preference value for supporting payments using Pix. */
    public boolean getFacilitatedPaymentsPixPref() {
        return mPrefService.getBoolean(Pref.FACILITATED_PAYMENTS_PIX);
    }

    /** Sets the preference value for supporting payments using Ewallet. */
    public void setFacilitatedPaymentsEwalletPref(boolean value) {
        mPrefService.setBoolean(Pref.FACILITATED_PAYMENTS_EWALLET, value);
    }

    /** Returns the preference value for supporting payments using Ewallet. */
    public boolean getFacilitatedPaymentsEwalletPref() {
        return mPrefService.getBoolean(Pref.FACILITATED_PAYMENTS_EWALLET);
    }

    /** Sets the preference value for supporting payments using A2A. */
    public void setFacilitatedPaymentsA2AEnabledPref(boolean value) {
        mPrefService.setBoolean(Pref.FACILITATED_PAYMENTS_A2A_ENABLED, value);
    }

    /** Returns the preference value for supporting payments using A2A. */
    public boolean getFacilitatedPaymentsA2AEnabledPref() {
        return mPrefService.getBoolean(Pref.FACILITATED_PAYMENTS_A2A_ENABLED);
    }

    /** Returns the preference value for whether A2A has already been triggered once. */
    public boolean getFacilitatedPaymentsA2ATriggeredOncePref() {
        return mPrefService.getBoolean(Pref.FACILITATED_PAYMENTS_A2A_TRIGGERED_ONCE);
    }

    /** Returns whether the BNPL preference should be shown on the settings page. */
    public boolean shouldShowBnplSettings() {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().shouldShowBnplSettings(mPersonalDataManagerAndroid);
    }

    /**
     * @return Whether the buy now pay later feature {@code kAutofillEnableBuyNowPayLater}, which is
     *     defined in {@code components/autofill/core/common/autofill_payments_features.cc}, is
     *     enabled.
     */
    public boolean isBuyNowPayLaterEnabled() {
        return mPrefService.getBoolean(Pref.AUTOFILL_BNPL_ENABLED);
    }

    /**
     * Enables or disables the buy now pay later feature {@code kAutofillEnableBuyNowPayLater},
     * which is defined in {@code components/autofill/core/common/autofill_payments_features.cc}.
     *
     * @param enable True to enable buy now pay later, false otherwise.
     */
    public void setBuyNowPayLater(boolean enable) {
        mPrefService.setBoolean(Pref.AUTOFILL_BNPL_ENABLED, enable);
    }

    /** Gets the BNPL issuers to show in the settings page. */
    public BnplIssuerForSettings[] getBnplIssuersForSettings() {
        ThreadUtils.assertOnUiThread();
        return PersonalDataManagerJni.get().getBnplIssuersForSettings(mPersonalDataManagerAndroid);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long init(PersonalDataManager self, @JniType("Profile*") Profile profile);

        void destroy(long nativePersonalDataManagerAndroid);

        boolean isDataLoaded(long nativePersonalDataManagerAndroid);

        String[] getProfileGUIDsForSettings(long nativePersonalDataManagerAndroid);

        String[] getProfileGUIDsToSuggest(long nativePersonalDataManagerAndroid);

        String[] getProfileLabelsForSettings(long nativePersonalDataManagerAndroid);

        @JniType("std::u16string")
        String getProfileDescriptionForEditor(
                long nativePersonalDataManagerAndroid, @JniType("std::string") String guid);

        String[] getProfileLabelsToSuggest(long nativePersonalDataManagerAndroid);

        AutofillProfile getProfileByGUID(
                long nativePersonalDataManagerAndroid, @JniType("std::string") String guid);

        boolean isEligibleForAddressAccountStorage(long nativePersonalDataManagerAndroid);

        @JniType("std::string")
        String getDefaultCountryCodeForNewAddress(long nativePersonalDataManagerAndroid);

        @JniType("std::string")
        String setProfile(
                long nativePersonalDataManagerAndroid,
                AutofillProfile profile,
                @JniType("std::string") String guid);

        @JniType("std::string")
        String setProfileToLocal(
                long nativePersonalDataManagerAndroid,
                AutofillProfile profile,
                @JniType("std::string") String guid);

        @JniType("std::u16string")
        String getShippingAddressLabelForPaymentRequest(
                long nativePersonalDataManagerAndroid,
                AutofillProfile profile,
                @JniType("std::string") String guid,
                boolean includeCountry);

        String[] getCreditCardGUIDsForSettings(long nativePersonalDataManagerAndroid);

        String[] getCreditCardGUIDsToSuggest(long nativePersonalDataManagerAndroid);

        CreditCard getCreditCardByGUID(
                long nativePersonalDataManagerAndroid, @JniType("std::string") String guid);

        CreditCard getCreditCardForNumber(
                long nativePersonalDataManagerAndroid,
                @JniType("std::u16string") String cardNumber);

        void deleteAllLocalCreditCards(long nativePersonalDataManagerAndroid);

        @JniType("std::string")
        String setCreditCard(long nativePersonalDataManagerAndroid, CreditCard card);

        void updateServerCardBillingAddress(long nativePersonalDataManagerAndroid, CreditCard card);

        @JniType("std::string")
        String getBasicCardIssuerNetwork(
                @JniType("std::u16string") String cardNumber, boolean emptyIfInvalid);

        void removeByGUID(
                long nativePersonalDataManagerAndroid, @JniType("std::string") String guid);

        void removeProfile(
                long nativePersonalDataManagerAndroid, @JniType("std::string") String guid);

        void recordAndLogProfileUse(
                long nativePersonalDataManagerAndroid, @JniType("std::string") String guid);

        void recordAndLogCreditCardUse(
                long nativePersonalDataManagerAndroid, @JniType("std::string") String guid);

        boolean isAutofillProfileManaged(long nativePersonalDataManagerAndroid);

        boolean isAutofillCreditCardManaged(long nativePersonalDataManagerAndroid);

        @JniType("std::string")
        String toCountryCode(@JniType("std::u16string") String countryName);

        void addServerIbanForTest(long nativePersonalDataManagerAndroid, Iban iban); // IN-TEST

        Iban getIbanByGuid(
                long nativePersonalDataManagerAndroid, @JniType("std::string") String guid);

        Iban[] getIbansForSettings(long nativePersonalDataManagerAndroid);

        @JniType("std::string")
        String addOrUpdateLocalIban(long nativePersonalDataManagerAndroid, Iban iban);

        boolean isValidIban(
                long nativePersonalDataManagerAndroid, @JniType("std::u16string") String ibanValue);

        boolean shouldShowAddIbanButtonOnSettingsPage(long nativePersonalDataManagerAndroid);

        BankAccount[] getMaskedBankAccounts(long nativePersonalDataManagerAndroid);

        Ewallet[] getEwallets(long nativePersonalDataManagerAndroid);

        boolean isCardEligibleForBenefits(
                long nativePersonalDataManagerAndroid, @JniType("std::string") String guid);

        boolean shouldShowBnplSettings(long nativePersonalDataManagerAndroid);

        BnplIssuerForSettings[] getBnplIssuersForSettings(long nativePersonalDataManagerAndroid);
    }
}
