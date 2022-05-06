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

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.VirtualCardDialogLink;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.payments.SettingsAutofillAndPaymentsObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Server credit card settings.
 */
public class AutofillServerCardEditor extends AutofillCreditCardEditor {
    private static final String SETTINGS_PAGE_ENROLLMENT_HISTOGRAM_TEXT =
            "Autofill.VirtualCard.SettingsPageEnrollment";

    private View mLocalCopyLabel;
    private View mClearLocalCopy;
    private TextView mVirtualCardEnrollmentButton;
    private boolean mVirtualCardEnrollmentButtonShowsUnenroll;
    private AutofillPaymentMethodsDelegate mDelegate;

    // Enum to represent the types of cards that show info in a server card editor page.
    @IntDef({CardType.SERVER_CARD, CardType.VIRTUAL_CARD})
    @Retention(RetentionPolicy.SOURCE)
    private @interface CardType {
        int SERVER_CARD = 1;
        int VIRTUAL_CARD = 2;
    }

    // Enum to represent the buttons in a server card editor page.
    @IntDef({ButtonType.EDIT_CARD, ButtonType.VIRTUAL_CARD_ENROLL,
            ButtonType.VIRTUAL_CARD_UNENROLL})
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
        RecordHistogram.recordBooleanHistogram("Autofill.SettingsPage.ButtonClicked."
                        + getCardType(cardType) + "." + getButtonType(buttonType),
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
            logServerCardEditorButtonClicks(showVirtualCardEnrollmentButton()
                            ? CardType.VIRTUAL_CARD
                            : CardType.SERVER_CARD,
                    ButtonType.EDIT_CARD);
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
                logServerCardEditorButtonClicks(CardType.VIRTUAL_CARD,
                        mVirtualCardEnrollmentButtonShowsUnenroll ? ButtonType.VIRTUAL_CARD_UNENROLL
                                                                  : ButtonType.VIRTUAL_CARD_ENROLL);
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
        Callback<String> onEducationTextLinkClicked =
                url -> onLinkClicked(url, VirtualCardDialogLink.EDUCATION_TEXT);
        Callback<String> onGoogleLegalMessageLinkClicked =
                url -> onLinkClicked(url, VirtualCardDialogLink.GOOGLE_LEGAL_MESSAGE);
        Callback<String> onIssuerLegalMessageLinkClicked =
                url -> onLinkClicked(url, VirtualCardDialogLink.ISSUER_LEGAL_MESSAGE);
        Callback<Integer> resultHandler = dismissalCause -> {
            if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                logSettingsPageEnrollmentDialogUserSelection(true);
                // Silently enroll the virtual card.
                mDelegate.enrollOfferedVirtualCard();
                // Update the button label to allow un-enroll.
                setVirtualCardEnrollmentButtonLabel(true);
            } else {
                // There can be cases where the user did not explicitly cancel the
                // enrollment, but the dialog was closed.
                if (dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                    logSettingsPageEnrollmentDialogUserSelection(false);
                }
                // Since the user canceled the enrollment dialog, enable the button
                // again to allow for enrollment.
                mVirtualCardEnrollmentButton.setEnabled(true);
            }
        };
        AutofillVirtualCardEnrollmentDialog dialog = new AutofillVirtualCardEnrollmentDialog(
                getActivity(), modalDialogManager, virtualCardEnrollmentFields,
                getActivity().getString(
                        R.string.autofill_virtual_card_enrollment_accept_button_label),
                getActivity().getString(R.string.no_thanks), onEducationTextLinkClicked,
                onGoogleLegalMessageLinkClicked, onIssuerLegalMessageLinkClicked, resultHandler);
        dialog.show();
    }

    private void onLinkClicked(String url, @VirtualCardDialogLink int virtualCardDialogLink) {
        RecordHistogram.recordEnumeratedHistogram(
                SETTINGS_PAGE_ENROLLMENT_HISTOGRAM_TEXT + ".LinkClicked", virtualCardDialogLink,
                VirtualCardDialogLink.NUM_ENTRIES);
        CustomTabActivity.showInfoPage(getActivity(), url);
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
        mVirtualCardEnrollmentButton.setText(mVirtualCardEnrollmentButtonShowsUnenroll
                        ? R.string.autofill_card_editor_virtual_card_turn_off_button_label
                        : R.string.autofill_card_editor_virtual_card_turn_on_button_label);
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
