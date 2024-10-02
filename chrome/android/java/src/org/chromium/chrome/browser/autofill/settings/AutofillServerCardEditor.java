// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.chromium.chrome.browser.autofill.AutofillUiUtils.getCardIcon;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.UsedByReflection;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Server credit card settings. */
public class AutofillServerCardEditor extends AutofillCreditCardEditor {
    private static final String AUTOFILL_MANAGE_PAYMENTS_CARDS_URL =
            "https://pay.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods";
    private static final String AUTOFILL_MANAGE_PAYMENTS_CARDS_SANDBOX_URL =
            "https://pay.sandbox.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods";
    private static final String SETTINGS_PAGE_ENROLLMENT_HISTOGRAM_TEXT =
            "Autofill.VirtualCard.SettingsPageEnrollment";

    private TextView mVirtualCardEnrollmentButton;
    private boolean mVirtualCardEnrollmentButtonShowsUnenroll;
    private AutofillPaymentMethodsDelegate mDelegate;
    private boolean mAwaitingUpdateVirtualCardEnrollmentResponse;
    private boolean mServerCardEditorClosed;
    private Callback<Boolean> mVirtualCardEnrollmentUpdateResponseCallback;
    private Callback<String> mServerCardEditLinkOpenerCallback =
            url -> CustomTabActivity.showInfoPage(getActivity(), url);

    // Enum to represent the types of cards that show info in a server card editor page.
    @IntDef({CardType.SERVER_CARD, CardType.VIRTUAL_CARD})
    @Retention(RetentionPolicy.SOURCE)
    private @interface CardType {
        int SERVER_CARD = 1;
        int VIRTUAL_CARD = 2;
    }

    // Enum to represent the buttons in a server card editor page.
    @IntDef({
        ButtonType.EDIT_CARD,
        ButtonType.VIRTUAL_CARD_ENROLL,
        ButtonType.VIRTUAL_CARD_UNENROLL
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ButtonType {
        int EDIT_CARD = 1;
        int VIRTUAL_CARD_ENROLL = 2;
        int VIRTUAL_CARD_UNENROLL = 3;
    }

    private static String getCardType(@CardType int type) {
        switch (type) {
            case CardType.SERVER_CARD:
                return "ServerCard";
            case CardType.VIRTUAL_CARD:
                return "VirtualCard";
            default:
                return "None";
        }
    }

    private static String getButtonType(@ButtonType int type) {
        switch (type) {
            case ButtonType.EDIT_CARD:
                return "EditCard";
            case ButtonType.VIRTUAL_CARD_ENROLL:
                return "VirtualCardEnroll";
            case ButtonType.VIRTUAL_CARD_UNENROLL:
                return "VirtualCardUnenroll";
            default:
                return "None";
        }
    }

    private static void logServerCardEditorButtonClicks(
            @CardType int cardType, @ButtonType int buttonType) {
        RecordHistogram.recordBooleanHistogram(
                "Autofill.SettingsPage.ButtonClicked."
                        + getCardType(cardType)
                        + "."
                        + getButtonType(buttonType),
                true);
    }

    private static void logSettingsPageEnrollmentDialogUserSelection(boolean accepted) {
        RecordHistogram.recordBooleanHistogram(SETTINGS_PAGE_ENROLLMENT_HISTOGRAM_TEXT, accepted);
    }

    @UsedByReflection("AutofillPaymentMethodsFragment.java")
    public AutofillServerCardEditor() {}

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mDelegate = new AutofillPaymentMethodsDelegate(getProfile());
        mVirtualCardEnrollmentUpdateResponseCallback =
                isUpdateSuccessful -> {
                    // If the server card editor page was closed when the server call was in
                    // progress, cleanup the delegate. Else, update the enrollment button.
                    if (mServerCardEditorClosed) {
                        mDelegate.cleanup();
                    } else {
                        // Mark completion of the server call.
                        mAwaitingUpdateVirtualCardEnrollmentResponse = false;
                        if (isUpdateSuccessful) {
                            // Update the button label.
                            setVirtualCardEnrollmentButtonLabel(
                                    !mVirtualCardEnrollmentButtonShowsUnenroll);
                        } else {
                            // If update was not successful, enable the button so users can try
                            // again.
                            mVirtualCardEnrollmentButton.setEnabled(true);
                        }
                    }
                };
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final View v = super.onCreateView(inflater, container, savedInstanceState);
        if (mCard == null) {
            SettingsNavigationFactory.createSettingsNavigation().finishCurrentSettings(this);
            return v;
        }

        // Set card icon. It can be either a custom card art or the network icon.
        ImageView cardIconContainer = v.findViewById(R.id.settings_page_card_icon);
        cardIconContainer.setImageDrawable(
                getCardIcon(
                        getContext(),
                        PersonalDataManagerFactory.getForProfile(getProfile()),
                        mCard.getCardArtUrl(),
                        mCard.getIssuerIconDrawableId(),
                        ImageSize.LARGE,
                        ChromeFeatureList.isEnabled(
                                ChromeFeatureList.AUTOFILL_ENABLE_CARD_ART_IMAGE)));

        ((TextView) v.findViewById(R.id.settings_page_card_name))
                .setText(mCard.getCardNameForAutofillDisplay());
        ((TextView) v.findViewById(R.id.card_last_four))
                .setText(mCard.getObfuscatedLastFourDigits());
        ((TextView) v.findViewById(R.id.settings_page_card_expiration))
                .setText(mCard.getFormattedExpirationDate(getActivity()));
        v.findViewById(R.id.edit_server_card)
                .setOnClickListener(
                        view -> {
                            logServerCardEditorButtonClicks(
                                    showVirtualCardEnrollmentButton()
                                            ? CardType.VIRTUAL_CARD
                                            : CardType.SERVER_CARD,
                                    ButtonType.EDIT_CARD);
                            mServerCardEditLinkOpenerCallback.onResult(getEditCardLink());
                        });

        final LinearLayout virtualCardContainerLayout = v.findViewById(R.id.virtual_card_ui);
        mVirtualCardEnrollmentButton = v.findViewById(R.id.virtual_card_enrollment_button);
        if (showVirtualCardEnrollmentButton()) {
            virtualCardContainerLayout.setVisibility(View.VISIBLE);
            setVirtualCardEnrollmentButtonLabel(
                    mCard.getVirtualCardEnrollmentState() == VirtualCardEnrollmentState.ENROLLED);
            mVirtualCardEnrollmentButton.setOnClickListener(
                    view -> {
                        assert mDelegate != null
                                : "mDelegate must be initialized before making (un)enrolment"
                                        + " calls.";
                        final ModalDialogManager modalDialogManager =
                                new ModalDialogManager(
                                        new AppModalPresenter(getActivity()), ModalDialogType.APP);
                        logServerCardEditorButtonClicks(
                                CardType.VIRTUAL_CARD,
                                mVirtualCardEnrollmentButtonShowsUnenroll
                                        ? ButtonType.VIRTUAL_CARD_UNENROLL
                                        : ButtonType.VIRTUAL_CARD_ENROLL);
                        // TODO(@vishwasuppoor): Show a blocking progress dialog
                        // (crbug.com/1327467).
                        // Disable the button until we receive a response from the server.
                        mVirtualCardEnrollmentButton.setEnabled(false);
                        if (!mVirtualCardEnrollmentButtonShowsUnenroll) {
                            mDelegate.initVirtualCardEnrollment(
                                    mCard.getInstrumentId(),
                                    result ->
                                            showVirtualCardEnrollmentDialog(
                                                    result, modalDialogManager));
                        } else {
                            showVirtualCardUnenrollmentDialog(modalDialogManager);
                        }
                    });
        } else {
            virtualCardContainerLayout.setVisibility(View.GONE);
        }

        initializeButtons(v);
        return v;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // Ensure that the native AutofillPaymentMethodsDelegateMobile instance is cleaned up.
        // If a server call is in progress, do not cleanup the delegate yet.
        if (mAwaitingUpdateVirtualCardEnrollmentResponse) {
            // Mark that the server card editor page was closed, so when the server call is
            // completed, the delegate can be cleaned up.
            mServerCardEditorClosed = true;
            return;
        }
        mDelegate.cleanup();
    }

    private void showVirtualCardEnrollmentDialog(
            VirtualCardEnrollmentFields virtualCardEnrollmentFields,
            ModalDialogManager modalDialogManager) {
        AutofillVirtualCardEnrollmentDialog.LinkClickCallback onLinkClicked =
                (url, virtualCardEnrollmentLinkType) -> {
                    RecordHistogram.recordEnumeratedHistogram(
                            SETTINGS_PAGE_ENROLLMENT_HISTOGRAM_TEXT + ".LinkClicked",
                            virtualCardEnrollmentLinkType,
                            VirtualCardEnrollmentLinkType.MAX_VALUE + 1);
                    CustomTabActivity.showInfoPage(getActivity(), url);
                };
        Callback<Integer> resultHandler =
                dismissalCause -> {
                    if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                        logSettingsPageEnrollmentDialogUserSelection(true);
                        // Silently enroll the virtual card.
                        mDelegate.enrollOfferedVirtualCard(
                                mVirtualCardEnrollmentUpdateResponseCallback);
                        // Turn the flag on indicating that a server call is in progress.
                        mAwaitingUpdateVirtualCardEnrollmentResponse = true;
                    } else {
                        logSettingsPageEnrollmentDialogUserSelection(false);
                        // Since the user canceled the enrollment dialog, enable the button
                        // again to allow for enrollment.
                        mVirtualCardEnrollmentButton.setEnabled(true);
                    }
                };
        AutofillVirtualCardEnrollmentDialog dialog =
                new AutofillVirtualCardEnrollmentDialog(
                        getActivity(),
                        modalDialogManager,
                        PersonalDataManagerFactory.getForProfile(getProfile()),
                        virtualCardEnrollmentFields,
                        getActivity()
                                .getString(
                                        R.string
                                                .autofill_virtual_card_enrollment_accept_button_label),
                        getActivity().getString(R.string.no_thanks),
                        onLinkClicked,
                        resultHandler);
        dialog.show();
    }

    private void showVirtualCardUnenrollmentDialog(ModalDialogManager modalDialogManager) {
        AutofillVirtualCardUnenrollmentDialog dialog =
                new AutofillVirtualCardUnenrollmentDialog(
                        getActivity(),
                        modalDialogManager,
                        unenrollRequested -> {
                            if (unenrollRequested) {
                                mDelegate.unenrollVirtualCard(
                                        mCard.getInstrumentId(),
                                        mVirtualCardEnrollmentUpdateResponseCallback);
                                // Turn the flag on indicating that a server call is in
                                // progress.
                                mAwaitingUpdateVirtualCardEnrollmentResponse = true;
                            } else {
                                mVirtualCardEnrollmentButton.setEnabled(true);
                            }
                        });
        dialog.show();
    }

    private boolean showVirtualCardEnrollmentButton() {
        return (mCard.getVirtualCardEnrollmentState() == VirtualCardEnrollmentState.ENROLLED
                || mCard.getVirtualCardEnrollmentState()
                        == VirtualCardEnrollmentState.UNENROLLED_AND_ELIGIBLE);
    }

    /** Updates the Virtual Card Enrollment button label. */
    private void setVirtualCardEnrollmentButtonLabel(boolean isEnrolled) {
        mVirtualCardEnrollmentButtonShowsUnenroll = isEnrolled;
        mVirtualCardEnrollmentButton.setEnabled(true);
        mVirtualCardEnrollmentButton.setText(
                mVirtualCardEnrollmentButtonShowsUnenroll
                        ? R.string.autofill_card_editor_virtual_card_turn_off_button_label
                        : R.string.autofill_card_editor_virtual_card_turn_on_button_label);
    }

    // Returns the URL for managing the card in GPay Web.
    private String getEditCardLink() {
        // Check if sandbox is enabled.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.USE_SANDBOX_WALLET_ENVIRONMENT)) {
            return AUTOFILL_MANAGE_PAYMENTS_CARDS_SANDBOX_URL + "&id=" + mCard.getInstrumentId();
        }
        return AUTOFILL_MANAGE_PAYMENTS_CARDS_URL + "&id=" + mCard.getInstrumentId();
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
            PersonalDataManagerFactory.getForProfile(getProfile())
                    .updateServerCardBillingAddress(mCard);
        }
        return true;
    }

    @Override
    protected boolean getIsDeletable() {
        return false;
    }

    public void setServerCardEditLinkOpenerCallbackForTesting(Callback<String> callback) {
        mServerCardEditLinkOpenerCallback = callback;
    }
}
