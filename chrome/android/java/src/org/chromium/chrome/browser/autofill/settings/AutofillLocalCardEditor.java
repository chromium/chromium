// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.ui.text.EmptyTextWatcher;

import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Local credit card settings. */
public class AutofillLocalCardEditor extends AutofillCreditCardEditor {
    private static Callback<Fragment> sObserverForTest;
    private static final String EXPIRATION_DATE_SEPARATOR = "/";
    private static final String EXPIRATION_DATE_REGEX = "^(0[1-9]|1[0-2])\\/(\\d{2})$";
    // TODO(crbug.com/40945216): Leverage the value from C++ code to have a single source of truth.
    private static final String AMEX_NETWORK_NAME = "amex";
    static final String CARD_COUNT_BEFORE_ADDING_NEW_CARD_HISTOGRAM =
            "Autofill.PaymentMethods.SettingsPage.StoredCreditCardCountBeforeCardAdded";

    protected Button mDoneButton;
    private TextInputLayout mNameLabel;
    private EditText mNameText;
    protected TextInputLayout mNicknameLabel;
    protected EditText mNicknameText;
    private TextInputLayout mNumberLabel;
    protected EditText mNumberText;
    protected Spinner mExpirationMonth;
    protected Spinner mExpirationYear;
    // Since the nickname field is optional, an empty nickname is a valid nickname.
    private boolean mIsValidNickname = true;
    private boolean mIsCvcStorageEnabled;
    private int mInitialExpirationMonthPos;
    protected EditText mExpirationDate;
    protected EditText mCvc;
    protected ImageView mCvcHintImage;
    private boolean mIsValidExpirationDate;
    private int mInitialExpirationYearPos;

    @UsedByReflection("AutofillPaymentMethodsFragment.java")
    public AutofillLocalCardEditor() {}

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // Allow screenshots of the credit card number in Canary, Dev, and developer builds.
        if (VersionInfo.isBetaBuild() || VersionInfo.isStableBuild()) {
            WindowManager.LayoutParams attributes = getActivity().getWindow().getAttributes();
            attributes.flags |= WindowManager.LayoutParams.FLAG_SECURE;
            getActivity().getWindow().setAttributes(attributes);
        }

        View v = super.onCreateView(inflater, container, savedInstanceState);

        mDoneButton = v.findViewById(R.id.button_primary);
        mNameLabel = v.findViewById(R.id.credit_card_name_label);
        mNameText = v.findViewById(R.id.credit_card_name_edit);
        mNicknameLabel = v.findViewById(R.id.credit_card_nickname_label);
        mNicknameText = v.findViewById(R.id.credit_card_nickname_edit);
        mNumberLabel = v.findViewById(R.id.credit_card_number_label);
        mNumberText = v.findViewById(R.id.credit_card_number_edit);

        mNicknameText.addTextChangedListener(nicknameTextWatcher());
        mNicknameText.setOnFocusChangeListener(
                (view, hasFocus) -> mNicknameLabel.setCounterEnabled(hasFocus));
        // Set text watcher to format credit card number
        mNumberText.addTextChangedListener(new CreditCardNumberFormattingTextWatcher());

        mIsCvcStorageEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE);

        if (mIsCvcStorageEnabled) {
            LinearLayout creditCardExpirationSpinnerContainer =
                    v.findViewById(R.id.credit_card_expiration_spinner_container);
            TextView creditCardExpirationLabel = v.findViewById(R.id.credit_card_expiration_label);
            creditCardExpirationSpinnerContainer.setVisibility(View.GONE);
            creditCardExpirationLabel.setVisibility(View.GONE);

            mExpirationDate = v.findViewById(R.id.expiration_month_and_year);
            mExpirationDate.addTextChangedListener(expirationDateTextWatcher());

            mCvc = v.findViewById(R.id.cvc);
            mCvcHintImage = v.findViewById(R.id.cvc_hint_image);
            mNumberText.addTextChangedListener(creditCardNumberTextWatcherForCvc());
        } else {
            RelativeLayout creditCardExpirationAndCvcLayout =
                    v.findViewById(R.id.credit_card_expiration_and_cvc_layout);
            creditCardExpirationAndCvcLayout.setVisibility(View.GONE);

            mExpirationMonth = v.findViewById(R.id.autofill_credit_card_editor_month_spinner);
            mExpirationYear = v.findViewById(R.id.autofill_credit_card_editor_year_spinner);

            addSpinnerAdapters();
        }

        addCardDataToEditFields();
        initializeButtons(v);
        if (sObserverForTest != null) {
            sObserverForTest.onResult(this);
        }
        return v;
    }

    @Override
    protected int getLayoutId() {
        return R.layout.autofill_local_card_editor;
    }

    @Override
    protected int getTitleResourceId(boolean isNewEntry) {
        return isNewEntry
                ? R.string.autofill_create_credit_card
                : R.string.autofill_edit_credit_card;
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        boolean isAddressSpinnerUpdated =
                parent == mBillingAddress && position != mInitialBillingAddressPos;
        if (isAddressSpinnerUpdated) {
            updateSaveButtonEnabled();
        }
        if (!mIsCvcStorageEnabled) {
            boolean isYearSpinnerUpdated =
                    parent == mExpirationYear && position != mInitialExpirationYearPos;
            boolean isMonthSpinnerUpdated =
                    parent == mExpirationMonth && position != mInitialExpirationMonthPos;
            if (isYearSpinnerUpdated || isMonthSpinnerUpdated) {
                updateSaveButtonEnabled();
            }
        }
    }

    @Override
    public void afterTextChanged(Editable s) {
        updateSaveButtonEnabled();
    }

    public static void setObserverForTest(Callback<Fragment> observerForTest) {
        sObserverForTest = observerForTest;
        ResettersForTesting.register(() -> sObserverForTest = null);
    }

    @SuppressWarnings("DuplicateDateFormatField") // There's probably a bug here...
    void addSpinnerAdapters() {
        ArrayAdapter<CharSequence> adapter =
                new ArrayAdapter<CharSequence>(getActivity(), android.R.layout.simple_spinner_item);

        // Populate the month dropdown.
        Calendar calendar = Calendar.getInstance();
        calendar.set(Calendar.DAY_OF_MONTH, 1);
        SimpleDateFormat formatter = new SimpleDateFormat("MMMM (MM)", Locale.getDefault());

        for (int month = 0; month < 12; month++) {
            calendar.set(Calendar.MONTH, month);
            adapter.add(formatter.format(calendar.getTime()));
        }
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mExpirationMonth.setAdapter(adapter);

        // Populate the year dropdown.
        adapter =
                new ArrayAdapter<CharSequence>(getActivity(), android.R.layout.simple_spinner_item);
        int initialYear = calendar.get(Calendar.YEAR);
        for (int year = initialYear; year < initialYear + 10; year++) {
            adapter.add(Integer.toString(year));
        }
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mExpirationYear.setAdapter(adapter);
    }

    private void addCardDataToEditFields() {
        if (mCard == null) {
            mNumberLabel.requestFocus();
            return;
        }

        if (!TextUtils.isEmpty(mCard.getName())) {
            mNameLabel.getEditText().setText(mCard.getName());
        }
        if (!TextUtils.isEmpty(mCard.getNumber())) {
            mNumberLabel.getEditText().setText(mCard.getNumber());
        }

        // Make the name label focusable in touch mode so that mNameText doesn't get focused.
        mNameLabel.setFocusableInTouchMode(true);

        if (mIsCvcStorageEnabled) {
            if (!mCard.getMonth().isEmpty() && !mCard.getYear().isEmpty()) {
                mExpirationDate.setText(
                        String.format("%s/%s", mCard.getMonth(), mCard.getYear().substring(2)));
            }

            if (!mCard.getCvc().isEmpty()) {
                mCvc.setText(mCard.getCvc());
            }
        } else {
            int monthAsInt = 1;
            if (!mCard.getMonth().isEmpty()) {
                monthAsInt = Integer.parseInt(mCard.getMonth());
            }
            mInitialExpirationMonthPos = monthAsInt - 1;
            mExpirationMonth.setSelection(mInitialExpirationMonthPos);

            mInitialExpirationYearPos = 0;
            boolean foundYear = false;
            for (int i = 0; i < mExpirationYear.getAdapter().getCount(); i++) {
                if (mCard.getYear().equals(mExpirationYear.getAdapter().getItem(i))) {
                    mInitialExpirationYearPos = i;
                    foundYear = true;
                    break;
                }
            }
            // Maybe your card expired years ago? Add the card's year
            // to the spinner adapter if not found.
            if (!foundYear && !mCard.getYear().isEmpty()) {
                @SuppressWarnings("unchecked")
                ArrayAdapter<CharSequence> adapter =
                        (ArrayAdapter<CharSequence>) mExpirationYear.getAdapter();
                adapter.insert(mCard.getYear(), 0);
                mInitialExpirationYearPos = 0;
            }
            mExpirationYear.setSelection(mInitialExpirationYearPos);
        }

        if (!mCard.getNickname().isEmpty()) {
            mNicknameText.setText(mCard.getNickname());
        }
    }

    @Override
    protected boolean saveEntry() {
        // Remove all spaces in editText.
        String cardNumber = mNumberText.getText().toString().replaceAll("\\s+", "");
        // Issuer network will be empty if credit card number is not valid.
        if (TextUtils.isEmpty(
                PersonalDataManager.getBasicCardIssuerNetwork(
                        cardNumber, /* emptyIfInvalid= */ true))) {
            mNumberLabel.setError(
                    mContext.getString(R.string.payments_card_number_invalid_validation_message));
            return false;
        }

        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(getProfile());
        CreditCard card = personalDataManager.getCreditCardForNumber(cardNumber);
        card.setGUID(mGUID);
        card.setOrigin(SETTINGS_ORIGIN);
        card.setName(mNameText.getText().toString().trim());

        if (mIsCvcStorageEnabled) {
            String expirationDate = mExpirationDate.getText().toString().trim();
            if (TextUtils.isEmpty(expirationDate)) {
                mExpirationDate.setError(
                        mContext.getResources()
                                .getString(
                                        R.string
                                                .autofill_credit_card_editor_invalid_expiration_date));
                return false;
            }
            card.setMonth(AutofillLocalCardEditor.getExpirationMonth(expirationDate));
            card.setYear(AutofillLocalCardEditor.getExpirationYear(expirationDate));
            card.setCvc(mCvc.getText().toString().trim());
            // TODO(crbug.com/41483891): Move metric logging to a separate class.
            if (mIsNewEntry) {
                if (!card.getCvc().isEmpty()) {
                    RecordUserAction.record("AutofillCreditCardsAddedWithCvc");
                }
            } else {
                // Verify if the CVC value for the existing card is absent.
                if (mCard.getCvc().isEmpty()) {
                    // Verify if the CVC value is absent for the new card that is replacing the
                    // existing card.
                    if (card.getCvc().isEmpty()) {
                        // Record when an existing card without CVC is edited and no CVC was
                        // added.
                        RecordUserAction.record("AutofillCreditCardsEditedAndCvcWasLeftBlank");
                    } else {
                        // Record when an existing card without CVC is edited and CVC was added.
                        RecordUserAction.record("AutofillCreditCardsEditedAndCvcWasAdded");
                    }
                } else {
                    if (card.getCvc().isEmpty()) {
                        // Record when an existing card with CVC is edited and CVC was removed.
                        RecordUserAction.record("AutofillCreditCardsEditedAndCvcWasRemoved");
                    } else if (!card.getCvc().equals(mCard.getCvc())) {
                        // Record when an existing card with CVC is edited and CVC was updated.
                        RecordUserAction.record("AutofillCreditCardsEditedAndCvcWasUpdated");
                    } else {
                        // Record when an existing card with CVC is edited and CVC was
                        // unchanged.
                        RecordUserAction.record("AutofillCreditCardsEditedAndCvcWasUnchanged");
                    }
                }
            }
        } else {
            card.setMonth(String.valueOf(mExpirationMonth.getSelectedItemPosition() + 1));
            card.setYear((String) mExpirationYear.getSelectedItem());
        }

        card.setBillingAddressId(((AutofillProfile) mBillingAddress.getSelectedItem()).getGUID());
        card.setNickname(mNicknameText.getText().toString().trim());

        // Get the current card count before setting the new card.
        int currentCardCount = personalDataManager.getCreditCardCountForSettings();

        // Set GUID for adding a new card.
        card.setGUID(personalDataManager.setCreditCard(card));
        if (mIsNewEntry) {
            RecordUserAction.record("AutofillCreditCardsAdded");
            if (!card.getNickname().isEmpty()) {
                RecordUserAction.record("AutofillCreditCardsAddedWithNickname");
            }
            RecordHistogram.recordCount100Histogram(
                    CARD_COUNT_BEFORE_ADDING_NEW_CARD_HISTOGRAM, currentCardCount);
        }
        return true;
    }

    @Override
    protected void deleteEntry() {
        if (mGUID != null) {
            PersonalDataManagerFactory.getForProfile(getProfile()).deleteCreditCard(mGUID);
        }
    }

    @Override
    protected void initializeButtons(View v) {
        super.initializeButtons(v);

        // Listen for change to inputs. Enable the save button after something has changed.
        mNameText.addTextChangedListener(this);
        mNumberText.addTextChangedListener(this);

        if (mIsCvcStorageEnabled) {
            mExpirationDate.addTextChangedListener(this);
            mCvc.addTextChangedListener(this);
        } else {
            mExpirationMonth.setOnItemSelectedListener(this);
            mExpirationYear.setOnItemSelectedListener(this);
            // Listen for touch events for drop down menus. We clear the keyboard when user touches
            // any of these fields.
            mExpirationMonth.setOnTouchListener(this);
            mExpirationYear.setOnTouchListener(this);
        }
    }

    private void updateSaveButtonEnabled() {
        // Enable save button if credit card number is not empty and the nickname is valid
        // and the expiration date is valid. We validate the credit card number when the user
        // presses the save button.
        boolean enabled =
                !TextUtils.isEmpty(mNumberText.getText())
                        && mIsValidNickname
                        && (!mIsCvcStorageEnabled || mIsValidExpirationDate);
        mDoneButton.setEnabled(enabled);
    }

    private TextWatcher nicknameTextWatcher() {
        return new EmptyTextWatcher() {
            @Override
            public void afterTextChanged(Editable s) {
                // Show an error message if nickname contains any digits.
                mIsValidNickname = !s.toString().matches(".*\\d.*");
                mNicknameLabel.setError(
                        mIsValidNickname
                                ? ""
                                : mContext.getResources()
                                        .getString(
                                                R.string
                                                        .autofill_credit_card_editor_invalid_nickname));
                updateSaveButtonEnabled();
            }
        };
    }

    private TextWatcher expirationDateTextWatcher() {
        return new EmptyTextWatcher() {
            private static final int SEPARATOR_INDEX = 2;
            private static final int VALID_DATE_LENGTH = 5;

            @Override
            public void afterTextChanged(Editable s) {
                if (TextUtils.indexOf(s, EXPIRATION_DATE_SEPARATOR) < 0
                        && s.length() > SEPARATOR_INDEX) {
                    s.insert(SEPARATOR_INDEX, EXPIRATION_DATE_SEPARATOR);
                }
                if (s.length() == VALID_DATE_LENGTH) {
                    if (!validExpirationDate(s.toString())) {
                        mExpirationDate.setError(
                                mContext.getResources()
                                        .getString(
                                                R.string
                                                        .autofill_credit_card_editor_invalid_expiration_date));
                    } else if (!validFutureExpirationDate(s.toString())) {
                        mExpirationDate.setError(
                                mContext.getResources()
                                        .getString(
                                                R.string.autofill_credit_card_editor_expired_card));
                    } else if (mExpirationDate.getError() != null) {
                        // Removes error message if a previous error exists and the user inputs
                        // a valid date.
                        mExpirationDate.setError(null);
                    }
                }
                mIsValidExpirationDate =
                        validExpirationDate(s.toString())
                                && validFutureExpirationDate(s.toString());
                updateSaveButtonEnabled();
            }
        };
    }

    private TextWatcher creditCardNumberTextWatcherForCvc() {
        return new EmptyTextWatcher() {
            private boolean mUsingAmExCvcHintImage;

            @Override
            public void afterTextChanged(Editable s) {
                String cardNumber = s.toString().replaceAll("\\s+", "");
                if (isAmExCard(cardNumber)) {
                    if (!mUsingAmExCvcHintImage) {
                        mUsingAmExCvcHintImage = true;
                        mCvcHintImage.setImageResource(R.drawable.cvc_icon_amex);
                    }
                } else {
                    if (mUsingAmExCvcHintImage) {
                        mUsingAmExCvcHintImage = false;
                        mCvcHintImage.setImageResource(R.drawable.cvc_icon);
                    }
                }
            }
        };
    }

    @VisibleForTesting
    public static String getExpirationMonth(String expirationDate) {
        String month = expirationDate.split(EXPIRATION_DATE_SEPARATOR)[0];
        if (month.startsWith("0")) {
            return month.substring(1);
        }
        return month;
    }

    @VisibleForTesting
    public static String getExpirationYear(String expirationDate) {
        String year = expirationDate.split(EXPIRATION_DATE_SEPARATOR)[1];
        return "20" + year;
    }

    private boolean validExpirationDate(String expirationDate) {
        return expirationDate.matches(EXPIRATION_DATE_REGEX);
    }

    private boolean validFutureExpirationDate(String expirationMonthAndYear) {
        Pattern pattern = Pattern.compile(EXPIRATION_DATE_REGEX);
        Matcher matcher = pattern.matcher(expirationMonthAndYear);
        if (matcher.find()) {
            Calendar today = Calendar.getInstance(Locale.getDefault());
            Calendar expirationDate = Calendar.getInstance(Locale.getDefault());
            expirationDate.set(Calendar.MONTH, Integer.parseInt(matcher.group(1)));
            expirationDate.set(
                    Calendar.YEAR, Integer.parseInt(String.format("20%s", matcher.group(2))));
            expirationDate.set(
                    Calendar.DAY_OF_MONTH, expirationDate.getActualMaximum(Calendar.DAY_OF_MONTH));
            return !expirationDate.before(today);
        }
        return false;
    }

    public static boolean isAmExCard(String cardNumber) {
        return PersonalDataManager.getBasicCardIssuerNetwork(
                        cardNumber, /* emptyIfInvalid= */ false)
                .equals(AMEX_NETWORK_NAME);
    }
}
