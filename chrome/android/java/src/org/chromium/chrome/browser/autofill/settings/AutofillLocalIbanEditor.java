// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.os.Build;
import android.os.Bundle;
import android.text.Editable;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.EditText;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * This class creates a view for adding, editing, and deleting a local IBAN. A local IBAN gets saved
 * to the user's device only.
 */
public class AutofillLocalIbanEditor extends AutofillEditorBase implements ProfileDependentSetting {
    private static Callback<Fragment> sObserverForTest;

    protected Button mDoneButton;
    protected EditText mNickname;
    private TextInputLayout mNicknameLabel;
    protected EditText mValue;
    private Iban mIban;
    private Profile mProfile;
    private Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    @UsedByReflection("AutofillPaymentMethodsFragment.java")
    public AutofillLocalIbanEditor() {}

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View v = super.onCreateView(inflater, container, savedInstanceState);

        // Do not use autofill for the fields.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            getActivity()
                    .getWindow()
                    .getDecorView()
                    .setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS);
        }

        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(getProfile());
        mIban = personalDataManager.getIban(mGUID);

        mDoneButton = v.findViewById(R.id.button_primary);
        mNickname = v.findViewById(R.id.iban_nickname_edit);
        mNicknameLabel = v.findViewById(R.id.iban_nickname_label);
        mValue = v.findViewById(R.id.iban_value_edit);

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
                mGUID.isEmpty()
                        ? Iban.createEphemeral(
                                /* label= */ "",
                                /* nickname= */ mNickname.getText().toString().trim(),
                                /* value= */ mValue.getText().toString())
                        : Iban.createLocal(
                                /* guid= */ mGUID,
                                /* label= */ "",
                                /* nickname= */ mNickname.getText().toString().trim(),
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

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.delete_menu_id) {
            showDeletePaymentMethodConfirmationDialog();
            return true;
        }
        // TODO(b/332954304): Add help button to IBAN editor.
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {}

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }

    /** Return the {@link Profile} associated with the IBAN being edited. */
    public Profile getProfile() {
        return mProfile;
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

    /**
     * Sets Supplier for {@link ModalDialogManager} used to display {@link
     * AutofillDeletePaymentMethodConfirmationDialog}.
     */
    public void setModalDialogManagerSupplier(
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    private void showDeletePaymentMethodConfirmationDialog() {
        assert mModalDialogManagerSupplier != null;

        ModalDialogManager modalDialogManager = mModalDialogManagerSupplier.get();
        assert modalDialogManager != null;

        AutofillDeletePaymentMethodConfirmationDialog dialog =
                new AutofillDeletePaymentMethodConfirmationDialog(
                        modalDialogManager,
                        getContext(),
                        dismissalCause -> {
                            if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                                deleteEntry();
                                finishPage();
                            }
                        },
                        /* titleResId= */ R.string.autofill_iban_delete_confirmation_title);
        dialog.show();
    }
}
