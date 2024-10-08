// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.os.Bundle;
import android.text.Editable;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.EditText;

import androidx.annotation.IntDef;
import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * This class creates a view for adding, editing, and deleting a local IBAN. A local IBAN gets saved
 * to the user's device only.
 */
public class AutofillLocalIbanEditor extends AutofillEditorBase implements ProfileDependentSetting {
    @VisibleForTesting
    static final String SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM =
            "Autofill.SettingsPage.LocalIbanActions";

    private static Callback<Fragment> sObserverForTest;

    protected Button mDoneButton;
    protected EditText mNickname;
    private TextInputLayout mNicknameLabel;
    protected EditText mValue;
    private Iban mIban;
    private Profile mProfile;
    private Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // Needs to stay in sync with AutofillPaymentMethodAction in enums.xml.
    @IntDef({
        IbanAction.IBAN_ADDED_WITH_NICKNAME,
        IbanAction.IBAN_ADDED_WITHOUT_NICKNAME,
        IbanAction.IBAN_DELETED,
        IbanAction.IBAN_EDITOR_CLOSED_WITH_CHANGES,
        IbanAction.IBAN_EDITOR_CLOSED_WITHOUT_CHANGES,
        IbanAction.HISTOGRAM_BUCKET_COUNT
    })
    // TODO(b/371041630): Extend IBAN histograms to track nickname usage across all IBAN actions.
    @VisibleForTesting
    @interface IbanAction {
        int IBAN_ADDED_WITH_NICKNAME = 0;
        int IBAN_ADDED_WITHOUT_NICKNAME = 1;
        int IBAN_DELETED = 2;
        int IBAN_EDITOR_CLOSED_WITH_CHANGES = 3;
        int IBAN_EDITOR_CLOSED_WITHOUT_CHANGES = 4;
        int HISTOGRAM_BUCKET_COUNT = 5;
    }

    @UsedByReflection("AutofillPaymentMethodsFragment.java")
    public AutofillLocalIbanEditor() {}

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View v = super.onCreateView(inflater, container, savedInstanceState);

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
        if (guid.isEmpty()) return false;

        if (mIsNewEntry) {
            RecordHistogram.recordEnumeratedHistogram(
                    SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                    iban.getNickname().isEmpty()
                            ? IbanAction.IBAN_ADDED_WITHOUT_NICKNAME
                            : IbanAction.IBAN_ADDED_WITH_NICKNAME,
                    IbanAction.HISTOGRAM_BUCKET_COUNT);
        } else {
            boolean ibanChanged =
                    !mIban.getNickname().equals(iban.getNickname())
                            || !mIban.getValue().equals(iban.getValue());

            RecordHistogram.recordEnumeratedHistogram(
                    SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                    ibanChanged
                            ? IbanAction.IBAN_EDITOR_CLOSED_WITH_CHANGES
                            : IbanAction.IBAN_EDITOR_CLOSED_WITHOUT_CHANGES,
                    IbanAction.HISTOGRAM_BUCKET_COUNT);
        }

        return true;
    }

    @Override
    protected void deleteEntry() {
        if (mGUID != null) {
            PersonalDataManagerFactory.getForProfile(getProfile()).deleteIban(mGUID);
            RecordHistogram.recordEnumeratedHistogram(
                    SETTINGS_PAGE_LOCAL_IBAN_ACTIONS_HISTOGRAM,
                    IbanAction.IBAN_DELETED,
                    IbanAction.HISTOGRAM_BUCKET_COUNT);
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
        if (item.getItemId() == R.id.help_menu_id) {
            HelpAndFeedbackLauncherFactory.getForProfile(getProfile())
                    .show(
                            getActivity(),
                            getActivity().getString(R.string.help_context_autofill),
                            null);
            return true;
        }
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
