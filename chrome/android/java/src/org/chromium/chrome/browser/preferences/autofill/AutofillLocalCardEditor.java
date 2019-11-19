// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill;

import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.SettingsAutofillAndPaymentsObserver;
import org.chromium.chrome.browser.preferences.MainPreferences;
import org.chromium.chrome.browser.widget.ChromeTextInputLayout;

import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Locale;

/**
 * Local credit card settings.
 */
public class AutofillLocalCardEditor extends AutofillCreditCardEditor {
    private ChromeTextInputLayout mNameLabel;
    private EditText mNameText;
    private ChromeTextInputLayout mNumberLabel;
    private EditText mNumberText;
    private Spinner mExpirationMonth;
    private Spinner mExpirationYear;

    private int mInitialExpirationMonthPos;
    private int mInitialExpirationYearPos;

    @UsedByReflection("AutofillPaymentMethodsFragment.java")
    public AutofillLocalCardEditor() {}

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        // Allow screenshots of the credit card number in Canary, Dev, and developer builds.
        if (ChromeVersionInfo.isBetaBuild() || ChromeVersionInfo.isStableBuild()) {
            WindowManager.LayoutParams attributes = getActivity().getWindow().getAttributes();
            attributes.flags |= WindowManager.LayoutParams.FLAG_SECURE;
            getActivity().getWindow().setAttributes(attributes);
        }

        View v = super.onCreateView(inflater, container, savedInstanceState);

        mNameLabel = (ChromeTextInputLayout) v.findViewById(R.id.credit_card_name_label);
        mNameText = (EditText) v.findViewById(R.id.credit_card_name_edit);
        mNumberLabel = (ChromeTextInputLayout) v.findViewById(R.id.credit_card_number_label);
        mNumberText = (EditText) v.findViewById(R.id.credit_card_number_edit);

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
        return isNewEntry
                ? R.string.autofill_create_credit_card : R.string.autofill_edit_credit_card;
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
        ArrayAdapter<CharSequence> adapter = new ArrayAdapter<CharSequence>(getActivity(),
                android.R.layout.simple_spinner_item);

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
        adapter = new ArrayAdapter<CharSequence>(getActivity(),
                android.R.layout.simple_spinner_item);
        int initialYear = calendar.get(Calendar.YEAR);
        for (int year = initialYear; year < initialYear + 10; year++) {
            adapter.add(Integer.toString(year));
        }
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mExpirationYear.setAdapter(adapter);
    }

    private void addCardDataToEditFields() {
        if (mCard == null) {
            mNameLabel.requestFocus();
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
    }

    @Override
    protected boolean saveEntry() {
        // Remove all spaces in editText.
        String cardNumber = mNumberText.getText().toString().replaceAll("\\s+", "");
        PersonalDataManager personalDataManager = PersonalDataManager.getInstance();
        // Issuer network will be empty if credit card number is not valid.
        if (TextUtils.isEmpty(personalDataManager.getBasicCardIssuerNetwork(
                    cardNumber, true /* emptyIfInvalid */))) {
            mNumberLabel.setError(mContext.getString(
                    R.string.payments_card_number_invalid_validation_message));
            return false;
        }
        CreditCard card = personalDataManager.getCreditCardForNumber(cardNumber);
        card.setGUID(mGUID);
        card.setOrigin(MainPreferences.SETTINGS_ORIGIN);
        card.setName(mNameText.getText().toString().trim());
        card.setMonth(String.valueOf(mExpirationMonth.getSelectedItemPosition() + 1));
        card.setYear((String) mExpirationYear.getSelectedItem());
        card.setBillingAddressId(((AutofillProfile) mBillingAddress.getSelectedItem()).getGUID());
        // Set GUID for adding a new card.
        card.setGUID(personalDataManager.setCreditCard(card));
        SettingsAutofillAndPaymentsObserver.getInstance().notifyOnCreditCardUpdated(card);
        if (mIsNewEntry) {
            RecordUserAction.record("AutofillCreditCardsAdded");
        }
        return true;
    }

    @Override
    protected void deleteEntry() {
        if (mGUID != null) {
            PersonalDataManager.getInstance().deleteCreditCard(mGUID);
            SettingsAutofillAndPaymentsObserver.getInstance().notifyOnCreditCardDeleted(mGUID);
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

        // Listen for touch events for drop down menus. We clear the keyboard when user touches
        // any of these fields.
        mExpirationMonth.setOnTouchListener(this);
        mExpirationYear.setOnTouchListener(this);
    }

    private void updateSaveButtonEnabled() {
        // Enable save button if credit card number is not empty. We validate the credit card number
        // when user presses the save button.
        boolean enabled = !TextUtils.isEmpty(mNumberText.getText());
        ((Button) getView().findViewById(R.id.button_primary)).setEnabled(enabled);
    }
}
