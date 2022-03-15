// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.payments.SettingsAutofillAndPaymentsObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/**
 * Server credit card settings.
 */
public class AutofillServerCardEditor extends AutofillCreditCardEditor {
    private View mLocalCopyLabel;
    private View mClearLocalCopy;
    private TextView mVirtualCardEnrollmentButton;
    private boolean mVirtualCardEnrollmentButtonShowsUnenroll;
    private AutofillPaymentMethodsDelegate mDelegate;

    @UsedByReflection("AutofillPaymentMethodsFragment.java")
    public AutofillServerCardEditor() {}

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT)) {
            mDelegate = new AutofillPaymentMethodsDelegate(Profile.getLastUsedRegularProfile());
        }
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final View v = super.onCreateView(inflater, container, savedInstanceState);
        if (mCard == null) {
            getActivity().finish();
            return v;
        }

        ((TextView) v.findViewById(R.id.title)).setText(mCard.getObfuscatedNumber());
        ((TextView) v.findViewById(R.id.summary))
                .setText(mCard.getFormattedExpirationDate(getActivity()));
        v.findViewById(R.id.edit_server_card).setOnClickListener(view -> {
            RecordHistogram.recordBooleanHistogram(showVirtualCardEnrollmentButton()
                            ? "Autofill.SettingsPage.ButtonClicked.VirtualCard.EditCard"
                            : "Autofill.SettingsPage.ButtonClicked.ServerCard.EditCard",
                    true);
            CustomTabActivity.showInfoPage(
                    getActivity(), ChromeStringConstants.AUTOFILL_MANAGE_WALLET_CARD_URL);
        });

        final LinearLayout virtualCardContainerLayout =
                (LinearLayout) v.findViewById(R.id.virtual_card_ui);
        mVirtualCardEnrollmentButton = v.findViewById(R.id.virtual_card_enrollment_button);
        if (showVirtualCardEnrollmentButton()) {
            virtualCardContainerLayout.setVisibility(View.VISIBLE);
            setVirtualCardEnrollmentButtonLabel(
                    mCard.getVirtualCardEnrollmentState() == VirtualCardEnrollmentState.ENROLLED);
            mVirtualCardEnrollmentButton.setOnClickListener(view -> {
                assert mDelegate
                        != null
                    : "mDelegate must be initialized before making (un)enrolment calls.";
                final ModalDialogManager modalDialogManager = new ModalDialogManager(
                        new AppModalPresenter(getActivity()), ModalDialogType.APP);
                RecordHistogram.recordBooleanHistogram(mVirtualCardEnrollmentButtonShowsUnenroll
                                ? "Autofill.SettingsPage.ButtonClicked.VirtualCard.VirtualCardUnenroll"
                                : "Autofill.SettingsPage.ButtonClicked.VirtualCard.VirtualCardEnroll",
                        true);
                if (!mVirtualCardEnrollmentButtonShowsUnenroll) {
                    mDelegate.offerVirtualCardEnrollment(mCard.getInstrumentId(),
                            result -> showVirtualCardEnrollmentDialog(result, modalDialogManager));
                    // Disable the button until we receive a response from the server.
                    mVirtualCardEnrollmentButton.setEnabled(false);
                } else {
                    AutofillVirtualCardUnenrollmentDialog dialog =
                            new AutofillVirtualCardUnenrollmentDialog(
                                    getActivity(), modalDialogManager, unenrollRequested -> {
                                        if (unenrollRequested) {
                                            mDelegate.unenrollVirtualCard(mCard.getInstrumentId());

                                            // Change button label and behavior to Enroll.
                                            setVirtualCardEnrollmentButtonLabel(false);
                                        }
                                    });
                    dialog.show();
                }
            });
        } else {
            virtualCardContainerLayout.setVisibility(View.GONE);
        }

        mLocalCopyLabel = v.findViewById(R.id.local_copy_label);
        mClearLocalCopy = v.findViewById(R.id.clear_local_copy);

        if (mCard.getIsCached()) {
            mClearLocalCopy.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    PersonalDataManager.getInstance().clearUnmaskedCache(mGUID);
                    removeLocalCopyViews();
                }
            });
        } else {
            removeLocalCopyViews();
        }

        initializeButtons(v);
        return v;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // Ensure that the native AutofillPaymentMethodsDelegateMobile instance is cleaned up.
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT)) {
            mDelegate.cleanup();
        }
    }

    private void showVirtualCardEnrollmentDialog(
            VirtualCardEnrollmentFields virtualCardEnrollmentFields,
            ModalDialogManager modalDialogManager) {
        AutofillVirtualCardEnrollmentDialog dialog =
                new AutofillVirtualCardEnrollmentDialog(getActivity(), modalDialogManager,
                        virtualCardEnrollmentFields, (positiveButtonClicked) -> {
                            if (positiveButtonClicked) {
                                // Silently enroll the virtual card.
                                mDelegate.enrollOfferedVirtualCard();
                                // Update the button label to allow un-enroll.
                                setVirtualCardEnrollmentButtonLabel(true);
                            } else {
                                // Since the user canceled the enrollment dialog, enable the button
                                // again to allow for enrollment.
                                mVirtualCardEnrollmentButton.setEnabled(true);
                            }
                        });
        dialog.show();
    }

    private void removeLocalCopyViews() {
        ViewGroup parent = (ViewGroup) mClearLocalCopy.getParent();
        if (parent == null) return;

        parent.removeView(mLocalCopyLabel);
        parent.removeView(mClearLocalCopy);
    }

    private boolean showVirtualCardEnrollmentButton() {
        return (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT)
                && (mCard.getVirtualCardEnrollmentState() == VirtualCardEnrollmentState.ENROLLED
                        || mCard.getVirtualCardEnrollmentState()
                                == VirtualCardEnrollmentState.UNENROLLED_AND_ELIGIBLE));
    }

    /**
     * Updates the Virtual Card Enrollment button label.
     */
    private void setVirtualCardEnrollmentButtonLabel(boolean isEnrolled) {
        mVirtualCardEnrollmentButtonShowsUnenroll = isEnrolled;
        mVirtualCardEnrollmentButton.setEnabled(true);
        mVirtualCardEnrollmentButton.setText(
                mVirtualCardEnrollmentButtonShowsUnenroll ? R.string.remove : R.string.add);
    }

    @Override
    protected int getLayoutId() {
        return R.layout.autofill_server_card_editor;
    }

    @Override
    protected int getTitleResourceId(boolean isNewEntry) {
        return R.string.autofill_edit_credit_card;
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if (parent == mBillingAddress && position != mInitialBillingAddressPos) {
            ((Button) getView().findViewById(R.id.button_primary)).setEnabled(true);
        }
    }

    @Override
    protected boolean saveEntry() {
        if (mBillingAddress.getSelectedItem() != null
                && mBillingAddress.getSelectedItem() instanceof AutofillProfile) {
            mCard.setBillingAddressId(
                    ((AutofillProfile) mBillingAddress.getSelectedItem()).getGUID());
            PersonalDataManager.getInstance().updateServerCardBillingAddress(mCard);
            SettingsAutofillAndPaymentsObserver.getInstance().notifyOnCreditCardUpdated(mCard);
        }
        return true;
    }

    @Override
    protected boolean getIsDeletable() {
        return false;
    }
}
