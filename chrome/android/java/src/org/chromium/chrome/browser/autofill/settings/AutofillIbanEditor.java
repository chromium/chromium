// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.os.Build;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** The base class for adding, editing, and deleting an IBAN. */
public abstract class AutofillIbanEditor extends AutofillEditorBase
        implements ProfileDependentSetting {
    private Profile mProfile;
    private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    protected Iban mIban;

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
        return v;
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
                                getActivity().finish();
                            }
                        },
                        /* titleResId= */ R.string.autofill_iban_delete_confirmation_title);
        dialog.show();
    }
}
