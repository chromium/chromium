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
import android.widget.Spinner;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.components.version_info.VersionInfo;

import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Locale;

/**
 * Local credit card settings.
 */
public class AutofillLocalCardEditor extends AutofillCreditCardEditor {
    protected Button mDoneButton;
    private TextInputLayout mNameLabel;
    private EditText mNameText;
    protected TextInputLayout mNicknameLabel;
    protected EditText mNicknameText;
    private TextInputLayout mNumberLabel;
    private EditText mNumberText;
    private Spinner mExpirationMonth;
    private Spinner mExpirationYear;
    // Since the nickname field is optional, an empty nickname is a valid nickname.
    private boolean mIsValidNickname = true;
    private int mInitialExpirationMonthPos;
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

        mDoneButton = (Button) v.findViewById(R.id.button_primary);
        mNameLabel = (TextInputLayout) v.findViewById(R.id.credit_card_name_label);
        mNameText = (EditText) v.findViewById(R.id.credit_card_name_edit);
        mNicknameLabel = (TextInputLayout) v.findViewById(R.id.credit_card_nickname_label);
        mNicknameText = (EditText) v.findViewById(R.id.credit_card_nickname_edit);
        mNumberLabel = (TextInputLayout) v.findViewById(R.id.credit_card_number_label);
        mNumberText = (EditText) v.findViewById(R.id.credit_card_number_edit);

        mNicknameText.addTextChangedListener(nicknameTextWatcher());
        mNicknameText.setOnFocusChangeListener(
                (view, hasFocus) -> mNicknameLabel.setCounterEnabled(hasFocus));
        // Set text watcher to format credit card number
        mNumberText.addTextChangedListener(new CreditCardNumberFormattingTextWatcher());

        mExpirationMonth = (Spinner) v.findViewById(R.id.autofill_credit_card_editor_month_spinner);
        mExpirationYear = (Spinner) v.findViewById(R.id.autofill_credit_card_editor_year_spinner);

        addSpinnerAdapters();
        addCardDataToEditFields();
        initializeButtons(v);
        return v;
    }

    @Override
    protected int getLayoutId() {
        return R.layout.autofill_local_card_editor;
    }

    @Override
    protected int getTitleResourceId(boolean isNewEntry) {
        return isNewEntry ? R.string.autofill_create_credit_card
                          : R.string.autofill_edit_credit_card;
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if ((parent == mExpirationYear && position != mInitialExpirationYearPos)
                || (parent == mExpirationMonth && position != mInitialExpirationMonthPos)
                || (parent == mBillingAddress && position != mInitialBillingAddressPos)) {
            updateSaveButtonEnabled();
        }
    }

    @Override
    public void afterTextChanged(Editable s) {
        updateSaveButtonEnabled();
    }

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

        if (!mCard.getNickname().isEmpty()) {
            mNicknameText.setText(mCard.getNickname());
        }
    }

    @Override
    protected boolean saveEntry() {
        // Remove all spaces in editText.
        String cardNumber = mNumberText.getText().toString().replaceAll("\\s+", "");
        PersonalDataManager personalDataManager = PersonalDataManager.getInstance();
        // Issuer network will be empty if credit card number is not valid.
        if (TextUtils.isEmpty(personalDataManager.getBasicCardIssuerNetwork(
                    cardNumber, true /* emptyIfInvalid */))) {
            mNumberLabel.setError(
                    mContext.getString(R.string.payments_card_number_invalid_validation_message));
            return false;
        }
        CreditCard card = personalDataManager.getCreditCardForNumber(cardNumber);
        card.setGUID(mGUID);
        card.setOrigin(SETTINGS_ORIGIN);
        card.setName(mNameText.getText().toString().trim());
        card.setMonth(String.valueOf(mExpirationMonth.getSelectedItemPosition() + 1));
        card.setYear((String) mExpirationYear.getSelectedItem());
        card.setBillingAddressId(((AutofillProfile) mBillingAddress.getSelectedItem()).getGUID());
        card.setNickname(mNicknameText.getText().toString().trim());
        // Set GUID for adding a new card.
        card.setGUID(personalDataManager.setCreditCard(card));
        if (mIsNewEntry) {
            RecordUserAction.record("AutofillCreditCardsAdded");
            if (!card.getNickname().isEmpty()) {
                RecordUserAction.record("AutofillCreditCardsAddedWithNickname");
            }
        }
        return true;
    }

    @Override
    protected void deleteEntry() {
        if (mGUID != null) {
            PersonalDataManager.getInstance().deleteCreditCard(mGUID);
        }
    }

    @Override
    protected void initializeButtons(View v) {
        super.initializeButtons(v);

        // Listen for change to inputs. Enable the save button after something has changed.
        mNameText.addTextChangedListener(this);
        mNumberText.addTextChangedListener(this);
        mExpirationMonth.setOnItemSelectedListener(this);
        mExpirationYear.setOnItemSelectedListener(this);

        // Listen for touch events for drop down menus. We clear the keyboard when user touches any
        // of these fields.
        mExpirationMonth.setOnTouchListener(this);
        mExpirationYear.setOnTouchListener(this);
    }

    private void updateSaveButtonEnabled() {
        // Enable save button if credit card number is not empty and the nickname is valid. We
        // validate the credit card number when user presses the save button.
        boolean enabled = !TextUtils.isEmpty(mNumberText.getText()) && mIsValidNickname;
        mDoneButton.setEnabled(enabled);
    }

    private TextWatcher nicknameTextWatcher() {
        return new TextWatcher() {
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void afterTextChanged(Editable s) {
                // Show an error message if nickname contains any digits.
                mIsValidNickname = !s.toString().matches(".*\\d.*");
                mNicknameLabel.setError(mIsValidNickname
                                ? ""
                                : mContext.getResources().getString(
                                        R.string.autofill_credit_card_editor_invalid_nickname));
                updateSaveButtonEnabled();
            }
        };
    }
}
