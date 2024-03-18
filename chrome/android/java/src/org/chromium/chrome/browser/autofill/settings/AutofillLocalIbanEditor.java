// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.os.Bundle;
import android.text.Editable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.EditText;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;

/**
 * This class creates a view for adding a local IBAN. A local IBAN gets saved to the user's device
 * only.
 */
public class AutofillLocalIbanEditor extends AutofillEditorBase implements ProfileDependentSetting {
    private static Callback<Fragment> sObserverForTest;

    protected Button mDoneButton;
    protected EditText mNickname;
    protected TextInputLayout mNicknameLabel;
    protected EditText mValue;

    private Profile mProfile;

    @UsedByReflection("AutofillPaymentMethodsFragment.java")
    public AutofillLocalIbanEditor() {}

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // TODO(b/309163678): Disable autofill for the fields.
        View v = super.onCreateView(inflater, container, savedInstanceState);

        mDoneButton = (Button) v.findViewById(R.id.button_primary);
        mNickname = (EditText) v.findViewById(R.id.iban_nickname_edit);
        mNicknameLabel = (TextInputLayout) v.findViewById(R.id.iban_nickname_label);
        mValue = (EditText) v.findViewById(R.id.iban_value_edit);

        mNickname.setOnFocusChangeListener(
                (view, hasFocus) -> mNicknameLabel.setCounterEnabled(hasFocus));

        initializeButtons(v);
        if (sObserverForTest != null) {
            sObserverForTest.onResult(this);
        }
        return v;
    }

    @Override
    protected int getLayoutId() {
        return R.layout.autofill_local_iban_editor;
    }

    @Override
    protected int getTitleResourceId(boolean isNewEntry) {
        // TODO(b/309163678): Use isNewEntry to decide which title to display
        // (i.e., autofill_add_local_iban or autofill_edit_local_iban).
        return R.string.autofill_add_local_iban;
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        // TODO(b/309163678): Update this once the AutofillIbanEditor class is added.
    }

    @Override
    public void afterTextChanged(Editable s) {
        updateSaveButtonEnabled();
    }

    @Override
    protected boolean saveEntry() {
        // TODO(b/320757907): Save IBAN from settings page.
        return false;
    }

    @Override
    protected void deleteEntry() {
        // TODO(b/309163615): User can delete existing IBANs from settings page.
    }

    @Override
    protected void initializeButtons(View v) {
        super.initializeButtons(v);
        mValue.addTextChangedListener(this);
    }

    @VisibleForTesting
    public static void setObserverForTest(Callback<Fragment> observerForTest) {
        sObserverForTest = observerForTest;
    }

    private void updateSaveButtonEnabled() {
        // Enable save button if IBAN value is valid.
        mDoneButton.setEnabled(
                PersonalDataManagerFactory.getForProfile(mProfile)
                        .isValidIban(mValue.getText().toString()));
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }
}
