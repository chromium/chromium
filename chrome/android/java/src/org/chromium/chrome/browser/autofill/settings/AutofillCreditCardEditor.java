// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.chromium.build.NullUtil.assertNonNull;

import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.Spinner;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.FieldType;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** The base class for credit card settings. */
@NullMarked
public abstract class AutofillCreditCardEditor extends AutofillEditorBase
        implements ProfileDependentSetting {
    private @Nullable Profile mProfile;
    private @Nullable Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;

    protected CreditCard mCard;
    // These fields are mutually exclusive. Only one is non-null depending on whether
    // ChromeFeatureList.sAndroidSettingsContainment is enabled.
    protected @Nullable Spinner mBillingAddressSpinner;
    protected @Nullable AutoCompleteTextView mBillingAddressDropdown;
    protected @Nullable AutofillProfile mInitialBillingProfile;

    protected @Nullable AutofillProfile mSelectedBillingProfile;
    protected int mInitialBillingAddressPos;

    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View v = super.onCreateView(inflater, container, savedInstanceState);

        PersonalDataManager personalDataManager =
                PersonalDataManagerFactory.getForProfile(getProfile());
        mCard = personalDataManager.getCreditCard(mGUID);
        List<AutofillProfile> profiles = personalDataManager.getProfilesForSettings();

        List<AutofillProfile> billingAddresses = new ArrayList<>();
        AutofillProfile noSelection = AutofillProfile.builder().build();
        noSelection.setLabel(getActivity().getString(R.string.select));
        billingAddresses.add(noSelection);

        for (AutofillProfile profile : profiles) {
            if (!TextUtils.isEmpty(profile.getInfo(FieldType.ADDRESS_HOME_STREET_ADDRESS))) {
                billingAddresses.add(profile);
            }
        }

        if (mCard != null && !TextUtils.isEmpty(mCard.getBillingAddressId())) {
            for (int i = 0; i < billingAddresses.size(); i++) {
                AutofillProfile profile = billingAddresses.get(i);
                if (profile != null
                        && TextUtils.equals(profile.getGUID(), mCard.getBillingAddressId())) {
                    mInitialBillingAddressPos = i;
                    mInitialBillingProfile = profile;
                    mSelectedBillingProfile = profile;
                    break;
                }
            }
        }

        if (ChromeFeatureList.sAndroidSettingsContainment.isEnabled()) {
            v.findViewById(R.id.autofill_credit_card_editor_legacy_dropdown_container)
                    .setVisibility(View.GONE);
            v.findViewById(R.id.autofill_credit_card_editor_billing_address_outlined_layout)
                    .setVisibility(View.VISIBLE);
            mBillingAddressDropdown =
                    v.findViewById(
                            R.id.autofill_credit_card_editor_billing_address_spinner_outlined);
            ArrayAdapter<AutofillProfile> adapter =
                    new ArrayAdapter<>(
                            getActivity(),
                            android.R.layout.simple_dropdown_item_1line,
                            billingAddresses);
            mBillingAddressDropdown.setAdapter(adapter);
            if (mInitialBillingProfile != null) {
                mBillingAddressDropdown.setText(mInitialBillingProfile.getLabel(), false);
            }
        } else {
            mBillingAddressSpinner =
                    v.findViewById(R.id.autofill_credit_card_editor_billing_address_spinner);
            ArrayAdapter<AutofillProfile> adapter =
                    new ArrayAdapter<>(
                            getActivity(), android.R.layout.simple_spinner_item, billingAddresses);
            adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
            mBillingAddressSpinner.setAdapter(adapter);
            mBillingAddressSpinner.setSelection(mInitialBillingAddressPos);
            // TODO(rouslan): Use an [+ ADD ADDRESS] button instead of disabling the dropdown.
            // http://crbug.com/623629
            if (adapter.getCount() == 1) mBillingAddressSpinner.setEnabled(false);
        }

        return v;
    }

    @Override
    protected void initializeButtons(View v) {
        super.initializeButtons(v);
        if (ChromeFeatureList.sAndroidSettingsContainment.isEnabled()) {
            assert mBillingAddressDropdown != null;
            mBillingAddressDropdown.setOnItemClickListener(
                    (parent, view, position, id) -> {
                        mSelectedBillingProfile =
                                (AutofillProfile) parent.getItemAtPosition(position);
                        onBillingAddressSelected(mSelectedBillingProfile);
                    });
            mBillingAddressDropdown.setOnFocusChangeListener(
                    (view, hasFocus) -> {
                        if (hasFocus) {
                            KeyboardVisibilityDelegate.getInstance().hideKeyboard(view);
                        }
                    });
        } else {
            assert mBillingAddressSpinner != null;
            mBillingAddressSpinner.setOnItemSelectedListener(this);
            // Listen for touch events on billing address field. We clear the keyboard when user
            // touches the billing address field because it is a drop down menu.
            mBillingAddressSpinner.setOnTouchListener(this);
        }
    }

    /**
     * Called when a new billing address is selected.
     *
     * @param profile The newly selected billing address.
     */
    protected void onBillingAddressSelected(AutofillProfile profile) {}

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
    public void setProfile(Profile profile) {
        mProfile = profile;
    }

    /** Return the {@link Profile} associated with the card being edited. */
    public Profile getProfile() {
        return assertNonNull(mProfile);
    }

    /**
     * Sets Supplier for {@lnk ModalDialogManager} used to display {@link
     * AutofillDeletePaymentMethodConfirmationDialog}.
     */
    public void setModalDialogManagerSupplier(
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier) {
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
