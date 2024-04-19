// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.os.Bundle;
import android.text.Editable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;

import androidx.annotation.LayoutRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.components.autofill.IbanRecordType;

/**
 * This class creates a view for adding and editing a local IBAN. A local IBAN gets saved to the
 * user's device only.
 */
public class AutofillLocalIbanEditor extends AutofillIbanEditor {
    private static Callback<Fragment> sObserverForTest;

    protected Button mDoneButton;
    protected EditText mNickname;
    protected TextInputLayout mNicknameLabel;
    protected EditText mValue;

    @UsedByReflection("AutofillPaymentMethodsFragment.java")
    public AutofillLocalIbanEditor() {}

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View v = super.onCreateView(inflater, container, savedInstanceState);

        mDoneButton = (Button) v.findViewById(R.id.button_primary);
        mNickname = (EditText) v.findViewById(R.id.iban_nickname_edit);
        mNicknameLabel = (TextInputLayout) v.findViewById(R.id.iban_nickname_label);
        mValue = (EditText) v.findViewById(R.id.iban_value_edit);

        mNickname.setOnFocusChangeListener(
                (view, hasFocus) -> mNicknameLabel.setCounterEnabled(hasFocus));

        addIbanDataToEditFields();
        initializeButtons(v);
        if (sObserverForTest != null) {
            sObserverForTest.onResult(this);
        }
        return v;
    }

    @Override
    protected @LayoutRes int getLayoutId() {
        return R.layout.autofill_local_iban_editor;
    }

    @Override
    protected @StringRes int getTitleResourceId(boolean isNewEntry) {
        return isNewEntry ? R.string.autofill_add_local_iban : R.string.autofill_edit_local_iban;
    }

    @Override
    public void afterTextChanged(Editable s) {
        updateSaveButtonEnabled();
    }

    @Override
    protected boolean saveEntry() {
        // If an existing local IBAN is being edited, its GUID and record type are set here. In the
        // case of a new IBAN, these values are set right before being written to the autofill
        // table.
        Iban iban =
                Iban.create(
                        /* guid= */ mGUID,
                        /* label= */ "",
                        /* nickname= */ mNickname.getText().toString().trim(),
                        /* recordType= */ mGUID.isEmpty()
                                ? IbanRecordType.UNKNOWN
                                : IbanRecordType.LOCAL_IBAN,
                        /* value= */ mValue.getText().toString());
        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(getProfile());
        String guid = personalDataManager.addOrUpdateLocalIban(iban);
        // Return true if the GUID is non-empty (successful operation), and false if the GUID is
        // empty (unsuccessful).
        return !guid.isEmpty();
    }

    @Override
    protected void deleteEntry() {
        if (mGUID != null) {
            PersonalDataManagerFactory.getForProfile(getProfile()).deleteIban(mGUID);
        }
    }

    @Override
    protected void initializeButtons(View v) {
        super.initializeButtons(v);
        mNickname.addTextChangedListener(this);
        mValue.addTextChangedListener(this);
    }

    @VisibleForTesting
    public static void setObserverForTest(Callback<Fragment> observerForTest) {
        sObserverForTest = observerForTest;
    }

    private void addIbanDataToEditFields() {
        if (mIban == null) {
            return;
        }

        if (!mIban.getNickname().isEmpty()) {
            mNickname.setText(mIban.getNickname());
        }
        if (!mIban.getValue().isEmpty()) {
            mValue.setText(mIban.getValue());
        }
    }

    private void updateSaveButtonEnabled() {
        // Enable save button if IBAN value is valid.
        mDoneButton.setEnabled(
                PersonalDataManagerFactory.getForProfile(getProfile())
                        .isValidIban(mValue.getText().toString()));
    }
}
