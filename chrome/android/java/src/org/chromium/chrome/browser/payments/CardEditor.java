// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.os.Handler;
import android.text.SpannableStringBuilder;
import android.text.TextUtils;
import android.text.style.AbsoluteSizeSpan;
import android.text.style.ForegroundColorSpan;
import android.util.Pair;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.CreditCardScanner;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.PaymentRequestImpl.PaymentRequestServiceObserverForTest;
import org.chromium.chrome.browser.preferences.autofill.AutofillProfileBridge.DropdownKeyValue;
import org.chromium.chrome.browser.widget.prefeditor.EditorBase;
import org.chromium.chrome.browser.widget.prefeditor.EditorFieldModel;
import org.chromium.chrome.browser.widget.prefeditor.EditorFieldModel.EditorFieldValidator;
import org.chromium.chrome.browser.widget.prefeditor.EditorFieldModel.EditorValueIconGenerator;
import org.chromium.chrome.browser.widget.prefeditor.EditorModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutionException;

import javax.annotation.Nullable;

/**
 * A credit card editor. Can be used for editing both local and server credit cards. Everything in
 * local cards can be edited. For server cards, only the billing address is editable.
 */
public class CardEditor extends EditorBase<AutofillPaymentInstrument>
        implements CreditCardScanner.Delegate {
    /** Description of a card type. */
    private static class CardIssuerNetwork {
        /**
         * The identifier for the drawable resource of the card issuer network, e.g.,
         * R.drawable.visa_card.
         */
        public final int icon;

        /**
         * The identifier for the localized description string for accessibility, e.g.,
         * R.string.autofill_cc_visa.
         */
        public final int description;

        /**
         * Builds a description of a card issuer network.
         *
         * @param icon        The identifier for the drawable resource of the card issuer network.
         * @param description The identifier for the localized description string for accessibility.
         */
        public CardIssuerNetwork(int icon, int description) {
            this.icon = icon;
            this.description = description;
        }
    }

    /** The support credit card names. */
    private static final String AMEX = "amex";
    private static final String DINERS = "diners";
    private static final String DISCOVER = "discover";
    private static final String JCB = "jcb";
    private static final String MASTERCARD = "mastercard";
    private static final String MIR = "mir";
    private static final String UNIONPAY = "unionpay";
    private static final String VISA = "visa";

    /** The dropdown key that triggers the address editor to add a new billing address. */
    private static final String BILLING_ADDRESS_ADD_NEW = "add";

    /** The shared preference for the 'save card to device' checkbox status. */
    private static final String CHECK_SAVE_CARD_TO_DEVICE = "check_save_card_to_device";

    /** The web contents where the web payments API is invoked. */
    private final WebContents mWebContents;

    /**
     * The list of profiles that can be used for billing address. This cache avoids re-reading
     * profiles from disk, which may have changed due to sync, for example. updateBillingAddress()
     * updates this cache.
     */
    private final List<AutofillProfile> mProfilesForBillingAddress;

    /** A map of GUIDs of the incomplete profiles to their edit required message resource Ids. */
    private final Map<String, Integer> mIncompleteProfilesForBillingAddress;

    /** Used for verifying billing address completeness and also editing billing addresses. */
    private final AddressEditor mAddressEditor;

    /** An optional observer used by tests. */
    @Nullable private final PaymentRequestServiceObserverForTest mObserverForTest;

    /**
     * A mapping from all card issuer networks recognized in Chrome to information about these
     * networks. The networks (e.g., "visa") are defined in:
     * https://w3c.github.io/webpayments-methods-card/#method-id
     */
    private final Map<String, CardIssuerNetwork> mCardIssuerNetworks;

    /**
     * The issuer networks accepted by the merchant website. This is a subset of recognized cards.
     * Used in the validator.
     */
    private final Set<String> mAcceptedIssuerNetworks;

    /**
     * The issuer networks accepted by the merchant website that should have "basic-card" as the
     * payment method. This is a subset of the accepted issuer networks. Used when creating the
     * complete payment instrument.
     */
    private final Set<String> mAcceptedBasicCardIssuerNetworks;

    /**
     * The accepted card types: CardType.UNKNOWN, CardType.CREDIT, CardType.DEBIT, CardType.PREPAID.
     */
    private final Set<Integer> mAcceptedBasicCardTypes;

    /**
     * The information about the accepted card issuer networks. Used in the editor as a hint to the
     * user about the valid card issuer networks. This is important to keep in a list, because the
     * display order matters.
     */
    private final List<CardIssuerNetwork> mAcceptedCardIssuerNetworks;

    private final Handler mHandler;
    private final EditorFieldValidator mCardNumberValidator;
    private final EditorValueIconGenerator mCardIconGenerator;
    private final AsyncTask<Calendar> mCalendar;

    @Nullable private EditorFieldModel mIconHint;
    @Nullable private EditorFieldModel mNumberField;
    @Nullable private EditorFieldModel mNameField;
    @Nullable private EditorFieldModel mMonthField;
    @Nullable private EditorFieldModel mYearField;
    @Nullable private EditorFieldModel mBillingAddressField;
    @Nullable private EditorFieldModel mSaveCardCheckbox;
    @Nullable private CreditCardScanner mCardScanner;
    @Nullable private EditorFieldValidator mCardExpirationMonthValidator;
    private boolean mCanScan;
    private boolean mIsScanning;
    private int mCurrentMonth;
    private int mCurrentYear;
    private boolean mIsIncognito;

    /**
     * Builds a credit card editor.
     *
     * @param webContents     The web contents where the web payments API is invoked.
     * @param addressEditor   Used for verifying billing address completeness and also editing
     *                        billing addresses.
     * @param observerForTest Optional observer for test.
     */
    public CardEditor(WebContents webContents, AddressEditor addressEditor,
            @Nullable PaymentRequestServiceObserverForTest observerForTest) {
        assert webContents != null;
        assert addressEditor != null;

        mWebContents = webContents;
        mAddressEditor = addressEditor;
        mObserverForTest = observerForTest;

        List<AutofillProfile> profiles =
                PersonalDataManager.getInstance().getBillingAddressesToSuggest();
        mProfilesForBillingAddress = new ArrayList<>();
        mIncompleteProfilesForBillingAddress = new HashMap<>();
        for (int i = 0; i < profiles.size(); i++) {
            AutofillProfile profile = profiles.get(i);
            // Include only local profiles, because GUIDs of server profiles change on every browser
            // restart. Server profiles are not supported as billing addresses.
            if (!profile.getIsLocal()) continue;
            // Do not include profiles without street address.
            if (TextUtils.isEmpty(profile.getStreetAddress())) continue;
            mProfilesForBillingAddress.add(profile);
            Pair<Integer, Integer> editMessageResIds = AutofillAddress.getEditMessageAndTitleResIds(
                    AutofillAddress.checkAddressCompletionStatus(
                            profile, AutofillAddress.CompletenessCheckType.IGNORE_PHONE));
            if (editMessageResIds.first.intValue() != 0) {
                mIncompleteProfilesForBillingAddress.put(
                        profile.getGUID(), editMessageResIds.first);
            }
        }

        // Sort profiles for billing address according to completeness.
        Collections.sort(mProfilesForBillingAddress, (a, b) -> {
            boolean isAComplete = AutofillAddress.checkAddressCompletionStatus(
                                          a, AutofillAddress.CompletenessCheckType.NORMAL)
                    == AutofillAddress.CompletionStatus.COMPLETE;
            boolean isBComplete = AutofillAddress.checkAddressCompletionStatus(
                                          b, AutofillAddress.CompletenessCheckType.NORMAL)
                    == AutofillAddress.CompletionStatus.COMPLETE;
            return ApiCompatibilityUtils.compareBoolean(isBComplete, isAComplete);
        });

        mCardIssuerNetworks = new HashMap<>();
        mCardIssuerNetworks.put(
                AMEX, new CardIssuerNetwork(R.drawable.amex_card, R.string.autofill_cc_amex));
        mCardIssuerNetworks.put(
                DINERS, new CardIssuerNetwork(R.drawable.diners_card, R.string.autofill_cc_diners));
        mCardIssuerNetworks.put(DISCOVER,
                new CardIssuerNetwork(R.drawable.discover_card, R.string.autofill_cc_discover));
        mCardIssuerNetworks.put(
                JCB, new CardIssuerNetwork(R.drawable.jcb_card, R.string.autofill_cc_jcb));
        mCardIssuerNetworks.put(MASTERCARD,
                new CardIssuerNetwork(R.drawable.mc_card, R.string.autofill_cc_mastercard));
        mCardIssuerNetworks.put(
                MIR, new CardIssuerNetwork(R.drawable.mir_card, R.string.autofill_cc_mir));
        mCardIssuerNetworks.put(UNIONPAY,
                new CardIssuerNetwork(R.drawable.unionpay_card, R.string.autofill_cc_union_pay));
        mCardIssuerNetworks.put(
                VISA, new CardIssuerNetwork(R.drawable.visa_card, R.string.autofill_cc_visa));

        mAcceptedIssuerNetworks = new HashSet<>();
        mAcceptedBasicCardIssuerNetworks = new HashSet<>();
        mAcceptedBasicCardTypes = new HashSet<>();
        mAcceptedCardIssuerNetworks = new ArrayList<>();
        mHandler = new Handler();

        mCardNumberValidator = new EditorFieldValidator() {
            @Override
            public boolean isValid(@Nullable CharSequence value) {
                return value != null
                        && mAcceptedIssuerNetworks.contains(
                                   PersonalDataManager.getInstance().getBasicCardIssuerNetwork(
                                           value.toString(), true));
            }

            @Override
            public boolean isLengthMaximum(@Nullable CharSequence value) {
                return isCardNumberLengthMaximum(value);
            }
        };

        mCardIconGenerator = value -> {
            if (value == null) return 0;
            CardIssuerNetwork cardTypeInfo = mCardIssuerNetworks.get(
                    PersonalDataManager.getInstance().getBasicCardIssuerNetwork(
                            value.toString(), false));
            if (cardTypeInfo == null) return 0;
            return cardTypeInfo.icon;
        };

        mCalendar = new AsyncTask<Calendar>() {
            @Override
            protected Calendar doInBackground() {
                return Calendar.getInstance();
            }
        };
        mCalendar.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        ChromeActivity activity = ChromeActivity.fromWebContents(mWebContents);
        mIsIncognito = activity != null && activity.getCurrentTabModel() != null
                && activity.getCurrentTabModel().isIncognito();
    }

    private boolean isCardNumberLengthMaximum(@Nullable CharSequence value) {
        if (TextUtils.isEmpty(value)) return false;
        String cardType = PersonalDataManager.getInstance().getBasicCardIssuerNetwork(
                value.toString(), false);
        if (TextUtils.isEmpty(cardType)) return false;

        // Below maximum values are consistent with the values used to check the validity of the
        // credit card number in autofill::IsValidCreditCardNumber.
        String cardNumber = removeSpaceAndBar(value);
        switch (cardType) {
            case AMEX:
                return cardNumber.length() == 15;
            case DINERS:
                return cardNumber.length() == 14;
            case UNIONPAY:
                return cardNumber.length() == 19;
            default:
                // Valid DISCOVER, JCB, MASTERCARD, MIR and VISA cards have at most 16 digits.
                return cardNumber.length() == 16;
        }
    }

    private static String removeSpaceAndBar(CharSequence value) {
        return value.toString().replace(" ", "").replace("-", "");
    }

    /**
     * Adds accepted payment method to the editor, if they are recognized credit card types.
     *
     * @param data Supported method and method specific data. Should not be null.
     */
    public void addAcceptedPaymentMethodIfRecognized(PaymentMethodData data) {
        assert data != null;
        String method = data.supportedMethod;
        if (mCardIssuerNetworks.containsKey(method)) {
            addAcceptedNetwork(method);
        } else if (BasicCardUtils.BASIC_CARD_METHOD_NAME.equals(method)) {
            Set<String> basicCardNetworks = BasicCardUtils.convertBasicCardToNetworks(data);
            mAcceptedBasicCardIssuerNetworks.addAll(basicCardNetworks);
            for (String network : basicCardNetworks) {
                addAcceptedNetwork(network);
            }
            mAcceptedBasicCardTypes.addAll(BasicCardUtils.convertBasicCardToTypes(data));
        }
    }

    /**
     * Adds a card network to the list of accepted networks.
     *
     * @param network An accepted network. Will be shown in UI only once, regardless of how many
     *                times this method is called.
     */
    private void addAcceptedNetwork(String network) {
        if (!mAcceptedIssuerNetworks.contains(network)) {
            mAcceptedIssuerNetworks.add(network);
            mAcceptedCardIssuerNetworks.add(mCardIssuerNetworks.get(network));
        }
    }

    /**
     * Builds and shows an editor model with the following fields for local cards.
     *
     * [ accepted card types hint images     ]
     * [ card number                         ]
     * [ name on card                        ]
     * [ expiration month ][ expiration year ]
     * [ billing address dropdown            ]
     * [ save this card checkbox             ] <-- Shown only for new cards when not in incognito
     *                                             mode.
     *
     * Server cards have the following fields instead.
     *
     * [ card's obfuscated number            ]
     * [ billing address dropdown            ]
     */
    @Override
    public void edit(@Nullable final AutofillPaymentInstrument toEdit,
            final Callback<AutofillPaymentInstrument> callback) {
        super.edit(toEdit, callback);

        // If |toEdit| is null, we're creating a new credit card.
        final boolean isNewCard = toEdit == null;

        // Ensure that |instrument| and |card| are never null.
        final AutofillPaymentInstrument instrument = isNewCard
                ? new AutofillPaymentInstrument(mWebContents, new CreditCard(),
                          null /* billingAddress */, null /* methodName */,
                          false /* matchesMerchantCardTypeExactly */)
                : toEdit;
        final CreditCard card = instrument.getCard();

        final EditorModel editor = new EditorModel(
                isNewCard ? mContext.getString(R.string.payments_add_card) : toEdit.getEditTitle());

        if (card.getIsLocal()) {
            Calendar calendar = null;
            try {
                calendar = mCalendar.get();
            } catch (InterruptedException | ExecutionException e) {
                mHandler.post(() -> callback.onResult(null));
                return;
            }
            assert calendar != null;

            // Let user edit any part of the local card.
            addLocalCardInputs(editor, card, calendar);
        } else {
            // Display some information about the server card.
            editor.addField(EditorFieldModel.createLabel(card.getObfuscatedNumber(), card.getName(),
                    mContext.getString(R.string.payments_credit_card_expiration_date_abbr,
                            card.getMonth(), card.getYear()),
                    card.getIssuerIconDrawableId()));
        }

        // Always show the billing address dropdown.
        addBillingAddressDropdown(editor, card);

        // Allow saving new cards on disk unless in incognito mode.
        if (isNewCard && !mIsIncognito) addSaveCardCheckbox(editor);

        // If the user clicks [Cancel], send |toEdit| card back to the caller (will return original
        // state, which could be null, a full card, or a partial card).
        editor.setCancelCallback(() -> callback.onResult(toEdit));

        // If the user clicks [Done], save changes on disk, mark the card "complete," and send it
        // back to the caller.
        editor.setDoneCallback(() -> {
            commitChanges(card, isNewCard);

            String methodName = card.getBasicCardIssuerNetwork();
            if (mAcceptedBasicCardIssuerNetworks.contains(methodName)) {
                methodName = BasicCardUtils.BASIC_CARD_METHOD_NAME;
            }
            assert methodName != null;

            AutofillProfile billingAddress =
                    findTargetProfile(mProfilesForBillingAddress, card.getBillingAddressId());
            assert billingAddress != null;

            instrument.completeInstrument(card, methodName, billingAddress);
            callback.onResult(instrument);
        });

        mEditorDialog.show(editor);
    }

    /**
     * Adds the given billing address to the list of billing addresses, if it's complete. If the
     * address is already known, then updates the existing address. Should be called before opening
     * the card editor.
     *
     * @param billingAddress The billing address to add or update. Should not be null.
     */
    public void updateBillingAddressIfComplete(AutofillAddress billingAddress) {
        if (!billingAddress.isComplete()) return;

        for (int i = 0; i < mProfilesForBillingAddress.size(); ++i) {
            if (TextUtils.equals(mProfilesForBillingAddress.get(i).getGUID(),
                        billingAddress.getIdentifier())) {
                mProfilesForBillingAddress.set(i, billingAddress.getProfile());
                mIncompleteProfilesForBillingAddress.remove(billingAddress.getIdentifier());
                return;
            }
        }

        // No matching profile was found. Add the new profile at the top of the list.
        billingAddress.setBillingAddressLabel();
        mProfilesForBillingAddress.add(0, new AutofillProfile(billingAddress.getProfile()));
    }

    /**
     * Adds the following fields to the editor.
     *
     * [ accepted card types hint images     ]
     * [ card number              [ocr icon] ]
     * [ name on card                        ]
     * [ expiration month ][ expiration year ]
     */
    private void addLocalCardInputs(EditorModel editor, CreditCard card, Calendar calendar) {
        // Local card editor shows a card icon hint.
        if (mIconHint == null) {
            List<Integer> icons = new ArrayList<>();
            List<Integer> descriptions = new ArrayList<>();
            for (int i = 0; i < mAcceptedCardIssuerNetworks.size(); i++) {
                icons.add(mAcceptedCardIssuerNetworks.get(i).icon);
                descriptions.add(mAcceptedCardIssuerNetworks.get(i).description);
            }
            mIconHint = EditorFieldModel.createIconList(
                    mContext.getString(getAcceptedCardsLabelResourceId()), icons, descriptions);
        }
        editor.addField(mIconHint);

        // Card scanner is expensive to query.
        if (mCardScanner == null) {
            mCardScanner = CreditCardScanner.create(mWebContents, this);
            mCanScan = mCardScanner.canScan();
        }

        // Card number is validated.
        if (mNumberField == null) {
            mNumberField = EditorFieldModel.createTextInput(
                    EditorFieldModel.INPUT_TYPE_HINT_CREDIT_CARD,
                    mContext.getString(R.string.autofill_credit_card_editor_number),
                    null /* suggestions */, null /* formatter */, mCardNumberValidator,
                    mCardIconGenerator,
                    mContext.getString(R.string.pref_edit_dialog_field_required_validation_message),
                    mContext.getString(R.string.payments_card_number_invalid_validation_message),
                    null /* value */);
            if (mCanScan) {
                mNumberField.addActionIcon(R.drawable.ic_photo_camera,
                        R.string.autofill_scan_credit_card, (Runnable) () -> {
                            if (mIsScanning) return;
                            mIsScanning = true;
                            mCardScanner.scan();
                        });
            }
        }
        mNumberField.setValue(card.getNumber());
        editor.addField(mNumberField);

        // Name on card is required.
        if (mNameField == null) {
            mNameField = EditorFieldModel.createTextInput(
                    EditorFieldModel.INPUT_TYPE_HINT_PERSON_NAME,
                    mContext.getString(R.string.autofill_credit_card_editor_name),
                    null /* suggestions */, null /* formatter */, null /* validator */,
                    null /* valueIconGenerator */,
                    mContext.getString(R.string.pref_edit_dialog_field_required_validation_message),
                    null /* invalidErrorMessage */, null /* value */);
        }
        mNameField.setValue(card.getName());
        editor.addField(mNameField);

        // Expiration month dropdown.
        if (mMonthField == null) {
            mCurrentYear = calendar.get(Calendar.YEAR);
            // The month in calendar is 0 based but the month value is 1 based.
            mCurrentMonth = calendar.get(Calendar.MONTH) + 1;

            if (mCardExpirationMonthValidator == null) {
                mCardExpirationMonthValidator = new EditorFieldValidator() {
                    @Override
                    public boolean isValid(@Nullable CharSequence monthValue) {
                        CharSequence yearValue = mYearField.getValue();
                        if (monthValue == null || yearValue == null) return false;

                        int month = Integer.parseInt(monthValue.toString());
                        int year = Integer.parseInt(yearValue.toString());

                        return year > mCurrentYear
                              || (year == mCurrentYear && month >= mCurrentMonth);
                    }

                    @Override
                    public boolean isLengthMaximum(@Nullable CharSequence value) {
                        return false;
                    }
                };
            }

            mMonthField = EditorFieldModel.createDropdown(
                    mContext.getString(R.string.autofill_credit_card_editor_expiration_date),
                    buildMonthDropdownKeyValues(calendar),
                    mCardExpirationMonthValidator,
                    mContext.getString(
                          R.string.payments_card_expiration_invalid_validation_message));
            mMonthField.setIsFullLine(false);

            if (mObserverForTest != null) {
                mMonthField.setDropdownCallback(new Callback<Pair<String, Runnable>>() {
                    @Override
                    public void onResult(final Pair<String, Runnable> eventData) {
                        mObserverForTest.onPaymentRequestServiceExpirationMonthChange();
                    }
                });
            }
        }
        if (mMonthField.getDropdownKeys().contains(card.getMonth())) {
            mMonthField.setValue(card.getMonth());
        } else {
            mMonthField.setValue(mMonthField.getDropdownKeyValues().get(0).getKey());
        }
        editor.addField(mMonthField);

        // Expiration year dropdown is side-by-side with the expiration year dropdown. The dropdown
        // should include the card's expiration year, so it's not cached.
        mYearField = EditorFieldModel.createDropdown(
                null /* label */, buildYearDropdownKeyValues(calendar, card.getYear()),
                null /* hint */);
        mYearField.setIsFullLine(false);
        if (mYearField.getDropdownKeys().contains(card.getYear())) {
            mYearField.setValue(card.getYear());
        } else {
            mYearField.setValue(mYearField.getDropdownKeyValues().get(0).getKey());
        }
        editor.addField(mYearField);
    }

    private int getAcceptedCardsLabelResourceId() {
        int credit = mAcceptedBasicCardTypes.contains(CardType.CREDIT) ? 1 : 0;
        int debit = mAcceptedBasicCardTypes.contains(CardType.DEBIT) ? 1 : 0;
        int prepaid = mAcceptedBasicCardTypes.contains(CardType.PREPAID) ? 1 : 0;
        int[][][] resourceIds = new int[2][2][2];
        resourceIds[0][0][0] = R.string.payments_accepted_cards_label;
        resourceIds[0][0][1] = R.string.payments_accepted_prepaid_cards_label;
        resourceIds[0][1][0] = R.string.payments_accepted_debit_cards_label;
        resourceIds[0][1][1] = R.string.payments_accepted_debit_prepaid_cards_label;
        resourceIds[1][0][0] = R.string.payments_accepted_credit_cards_label;
        resourceIds[1][0][1] = R.string.payments_accepted_credit_prepaid_cards_label;
        resourceIds[1][1][0] = R.string.payments_accepted_credit_debit_cards_label;
        resourceIds[1][1][1] = R.string.payments_accepted_cards_label;
        return resourceIds[credit][debit][prepaid];
    }

    /** Builds the key-value pairs for the month dropdown. */
    private static List<DropdownKeyValue> buildMonthDropdownKeyValues(Calendar calendar) {
        List<DropdownKeyValue> result = new ArrayList<>();

        Locale locale = Locale.getDefault();
        SimpleDateFormat keyFormatter = new SimpleDateFormat("MM", locale);
        SimpleDateFormat valueFormatter = new SimpleDateFormat("MMMM (MM)", locale);

        calendar.set(Calendar.DAY_OF_MONTH, 1);
        for (int month = 0; month < 12; month++) {
            calendar.set(Calendar.MONTH, month);
            Date date = calendar.getTime();
            result.add(
                    new DropdownKeyValue(keyFormatter.format(date), valueFormatter.format(date)));
        }

        return result;
    }

    /** Builds the key-value pairs for the year dropdown. */
    private static List<DropdownKeyValue> buildYearDropdownKeyValues(
            Calendar calendar, String alwaysIncludedYear) {
        List<DropdownKeyValue> result = new ArrayList<>();

        int initialYear = calendar.get(Calendar.YEAR);
        boolean foundAlwaysIncludedYear = false;
        for (int year = initialYear; year < initialYear + 10; year++) {
            String yearString = Integer.toString(year);
            if (yearString.equals(alwaysIncludedYear)) foundAlwaysIncludedYear = true;
            result.add(new DropdownKeyValue(yearString, yearString));
        }

        if (!foundAlwaysIncludedYear && !TextUtils.isEmpty(alwaysIncludedYear)) {
            result.add(0, new DropdownKeyValue(alwaysIncludedYear, alwaysIncludedYear));
        }

        return result;
    }

    /**
     * Adds the billing address dropdown to the editor with the following items.
     *
     * | "select"           |
     * | complete address 1 |
     * | complete address 2 |
     *      ...
     * | complete address n |
     * | "add address"      |
     */
    private void addBillingAddressDropdown(EditorModel editor, final CreditCard card) {
        final List<DropdownKeyValue> billingAddresses = new ArrayList<>();

        for (int i = 0; i < mProfilesForBillingAddress.size(); ++i) {
            AutofillProfile profile = mProfilesForBillingAddress.get(i);
            SpannableStringBuilder builder = new SpannableStringBuilder(profile.getLabel());

            // Append the edit required message if the address is incomplete.
            if (mIncompleteProfilesForBillingAddress.containsKey(profile.getGUID())) {
                builder.append(mContext.getString(R.string.autofill_address_summary_separator));

                int startIndex = builder.length();
                int editMessageResId =
                        mIncompleteProfilesForBillingAddress.get(profile.getGUID()).intValue();
                String editMessage = mContext.getString(editMessageResId);
                builder.append(editMessage);
                int endIndex = builder.length();

                Object foregroundSpanner = new ForegroundColorSpan(ApiCompatibilityUtils.getColor(
                        mContext.getResources(), R.color.default_text_color_link));
                builder.setSpan(foregroundSpanner, startIndex, endIndex, 0);

                // The text size in the dropdown is 14dp.
                Object sizeSpanner = new AbsoluteSizeSpan(14, true);
                builder.setSpan(sizeSpanner, startIndex, endIndex, 0);
            }

            billingAddresses.add(
                    new DropdownKeyValue(mProfilesForBillingAddress.get(i).getGUID(), builder));
        }

        billingAddresses.add(new DropdownKeyValue(
                BILLING_ADDRESS_ADD_NEW, mContext.getString(R.string.payments_add_address)));

        // Don't cache the billing address dropdown, because the user may have added or removed
        // profiles. Also pass the "Select" dropdown item as a hint to the dropdown constructor.
        mBillingAddressField = EditorFieldModel.createDropdown(
                mContext.getString(R.string.autofill_credit_card_editor_billing_address),
                billingAddresses, mContext.getString(R.string.select));
        mBillingAddressField.setDisplayPlusIcon(true);

        // The billing address is required.
        mBillingAddressField.setRequiredErrorMessage(
                mContext.getString(R.string.pref_edit_dialog_field_required_validation_message));

        mBillingAddressField.setDropdownCallback(new Callback<Pair<String, Runnable>>() {
            @Override
            public void onResult(final Pair<String, Runnable> eventData) {
                final boolean isAddingNewAddress = BILLING_ADDRESS_ADD_NEW.equals(eventData.first);
                final boolean isSelectingIncompleteAddress =
                        mIncompleteProfilesForBillingAddress.containsKey(eventData.first);
                if (!isAddingNewAddress && !isSelectingIncompleteAddress) {
                    if (mObserverForTest != null) {
                        mObserverForTest.onPaymentRequestServiceBillingAddressChangeProcessed();
                    }
                    return;
                }
                assert isAddingNewAddress != isSelectingIncompleteAddress;

                AutofillProfile profile;
                if (isAddingNewAddress) {
                    profile = new AutofillProfile();
                    // Prefill card holder name as the billing address name only when adding a new
                    // address.
                    profile.setFullName(
                            card.getIsLocal() ? mNameField.getValue().toString() : card.getName());
                } else {
                    profile = findTargetProfile(mProfilesForBillingAddress, eventData.first);
                }

                final AutofillAddress editAddress = new AutofillAddress(mContext, profile);
                mAddressEditor.edit(editAddress, new Callback<AutofillAddress>() {
                    @Override
                    public void onResult(AutofillAddress billingAddress) {
                        if (!billingAddress.isComplete()) {
                            // User cancelled out of the add or edit flow. Restore the selection
                            // to the card's billing address, if any, else clear the selection.
                            if (mBillingAddressField.getDropdownKeys().contains(
                                        card.getBillingAddressId())) {
                                mBillingAddressField.setValue(card.getBillingAddressId());
                            } else {
                                mBillingAddressField.setValue(null);
                            }
                        } else {
                            // Set the billing address label.
                            billingAddress.setBillingAddressLabel();

                            if (isSelectingIncompleteAddress) {
                                // User completed an incomplete address.
                                mIncompleteProfilesForBillingAddress.remove(
                                        billingAddress.getProfile().getGUID());

                                // Remove the old key-value from the dropdown.
                                for (int i = 0; i < billingAddresses.size(); i++) {
                                    if (billingAddresses.get(i).first.equals(
                                                billingAddress.getIdentifier())) {
                                        billingAddresses.remove(i);
                                        break;
                                    }
                                }
                            } else {
                                // User added a new complete address.
                                mProfilesForBillingAddress.add(billingAddress.getProfile());
                            }

                            // Add the newly added or edited address to the top of the dropdown.
                            billingAddresses.add(
                                    0, new DropdownKeyValue(billingAddress.getIdentifier(),
                                               billingAddress.getSublabel()));
                            mBillingAddressField.setDropdownKeyValues(billingAddresses);
                            mBillingAddressField.setValue(billingAddress.getIdentifier());
                        }

                        // Let the card editor UI re-read the model and re-create UI elements.
                        mHandler.post(eventData.second);
                    }
                });
            }
        });

        if (mBillingAddressField.getDropdownKeys().contains(card.getBillingAddressId())) {
            mBillingAddressField.setValue(card.getBillingAddressId());
        }

        editor.addField(mBillingAddressField);
    }

    private static AutofillProfile findTargetProfile(List<AutofillProfile> profiles, String guid) {
        for (int i = 0; i < profiles.size(); i++) {
            if (profiles.get(i).getGUID().equals(guid)) return profiles.get(i);
        }
        assert false : "Never reached.";
        return null;
    }

    /** Adds the "save this card" checkbox to the editor. */
    private void addSaveCardCheckbox(EditorModel editor) {
        if (mSaveCardCheckbox == null) {
            mSaveCardCheckbox = EditorFieldModel.createCheckbox(
                    mContext.getString(R.string.payments_save_card_to_device_checkbox),
                    CHECK_SAVE_CARD_TO_DEVICE);
        }
        editor.addField(mSaveCardCheckbox);
    }

    /**
     * Saves the edited credit card. Note that we do not save changes to disk in incognito mode.
     *
     * If this is a server card, then only its billing address identifier is updated.
     *
     * If this is a new local card, then it's saved on this device only if the user has checked the
     * "save this card" checkbox.
     */
    private void commitChanges(CreditCard card, boolean isNewCard) {
        card.setBillingAddressId(mBillingAddressField.getValue().toString());

        PersonalDataManager pdm = PersonalDataManager.getInstance();
        if (!card.getIsLocal()) {
            if (!mIsIncognito) pdm.updateServerCardBillingAddress(card);
            return;
        }

        card.setNumber(removeSpaceAndBar(mNumberField.getValue()));
        card.setName(mNameField.getValue().toString());
        card.setMonth(mMonthField.getValue().toString());
        card.setYear(mYearField.getValue().toString());

        // Calculate the basic card issuer network, obfuscated number, and the icon for this card.
        // All of these depend on the card number. The issuer network is sent to the merchant
        // website. The obfuscated number and the icon are displayed in the user interface.
        CreditCard displayableCard = pdm.getCreditCardForNumber(card.getNumber());
        card.setBasicCardIssuerNetwork(displayableCard.getBasicCardIssuerNetwork());
        card.setObfuscatedNumber(displayableCard.getObfuscatedNumber());
        card.setIssuerIconDrawableId(displayableCard.getIssuerIconDrawableId());

        if (!isNewCard) {
            if (!mIsIncognito) pdm.setCreditCard(card);
            return;
        }

        if (mSaveCardCheckbox != null && mSaveCardCheckbox.isChecked()) {
            assert !mIsIncognito;
            card.setGUID(pdm.setCreditCard(card));
        }
    }

    @Override
    public void onScanCompleted(
            String cardHolderName, String cardNumber, int expirationMonth, int expirationYear) {
        if (!TextUtils.isEmpty(cardHolderName)) mNameField.setValue(cardHolderName);
        if (!TextUtils.isEmpty(cardNumber)) mNumberField.setValue(cardNumber);
        if (expirationYear >= 2000) mYearField.setValue(Integer.toString(expirationYear));

        if (expirationMonth >= 1 && expirationMonth <= 12) {
            String monthKey = Integer.toString(expirationMonth);
            // The month key format is 'MM' in the dropdown.
            if (monthKey.length() == 1) monthKey = '0' + monthKey;
            mMonthField.setValue(monthKey);
        }

        mEditorDialog.update();
        mIsScanning = false;
    }

    @Override
    public void onScanCancelled() {
        mIsScanning = false;
    }
}
