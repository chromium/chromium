// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/** The base class for credit card settings. */
public abstract class AutofillCreditCardEditor extends AutofillEditorBase
        implements ProfileDependentSetting {
    private Profile mProfile;
    private Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    protected CreditCard mCard;
    protected Spinner mBillingAddress;
    protected int mInitialBillingAddressPos;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View v = super.onCreateView(inflater, container, savedInstanceState);

        // Populate the billing address dropdown.
        ArrayAdapter<AutofillProfile> profilesAdapter =
                new ArrayAdapter<AutofillProfile>(
                        getActivity(), android.R.layout.simple_spinner_item);
        profilesAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);

        AutofillProfile noSelection = AutofillProfile.builder().build();
        noSelection.setLabel(getActivity().getString(R.string.select));
        profilesAdapter.add(noSelection);

        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(mProfile);
        List<AutofillProfile> profiles = personalDataManager.getProfilesForSettings();
        for (int i = 0; i < profiles.size(); i++) {
            AutofillProfile profile = profiles.get(i);
            if (!TextUtils.isEmpty(profile.getStreetAddress())) {
                profilesAdapter.add(profile);
            }
        }

        mBillingAddress = v.findViewById(R.id.autofill_credit_card_editor_billing_address_spinner);
        mBillingAddress.setAdapter(profilesAdapter);

        // TODO(rouslan): Use an [+ ADD ADDRESS] button instead of disabling the dropdown.
        // http://crbug.com/623629
        if (profilesAdapter.getCount() == 1) mBillingAddress.setEnabled(false);

        mCard = personalDataManager.getCreditCard(mGUID);
        if (mCard != null) {
            if (!TextUtils.isEmpty(mCard.getBillingAddressId())) {
                for (int i = 0; i < mBillingAddress.getAdapter().getCount(); i++) {
                    AutofillProfile profile =
                            (AutofillProfile) mBillingAddress.getAdapter().getItem(i);
                    if (TextUtils.equals(profile.getGUID(), mCard.getBillingAddressId())) {
                        mInitialBillingAddressPos = i;
                        mBillingAddress.setSelection(i);
                        break;
                    }
                }
            }
        }

        return v;
    }

    @Override
    protected void initializeButtons(View v) {
        super.initializeButtons(v);

        mBillingAddress.setOnItemSelectedListener(this);

        // Listen for touch events on billing address field. We clear the keyboard when user touches
        // the billing address field because it is a drop down menu.
        mBillingAddress.setOnTouchListener(this);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.delete_menu_id) {
            showDeletePaymentMethodConfirmationDialog();
            return true;
        }
        if (item.getItemId() == R.id.help_menu_id) {
            HelpAndFeedbackLauncherFactory.getForProfile(mProfile)
                    .show(
                            getActivity(),
                            getActivity().getString(R.string.help_context_autofill),
                            null);
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }

    /** Return the {@link Profile} associated with the card being edited. */
    public Profile getProfile() {
        return mProfile;
    }

    /**
     * Sets Supplier for {@lnk ModalDialogManager} used to display {@link
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
                        /* titleResId= */ R.string.autofill_credit_card_delete_confirmation_title);
        dialog.show();
    }
}
