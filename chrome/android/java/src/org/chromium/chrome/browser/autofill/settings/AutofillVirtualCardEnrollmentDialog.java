// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/** Dialog shown to the user to enroll a credit card into the virtual card feature. */
public class AutofillVirtualCardEnrollmentDialog {
    /** The interface that implements the action to be performed when links are clicked. */
    @FunctionalInterface
    public interface LinkClickCallback {
        void call(String url, @VirtualCardEnrollmentLinkType int virtualCardEnrollmentLinkType);
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final PersonalDataManager mPersonalDataManager;
    private final VirtualCardEnrollmentFields mVirtualCardEnrollmentFields;
    private final String mAcceptButtonText;
    private final String mDeclineButtonText;
    private final LinkClickCallback mOnLinkClicked;
    private final Callback<Integer> mResultHandler;
    private PropertyModel mDialogModel;

    public AutofillVirtualCardEnrollmentDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            PersonalDataManager personalDataManager,
            VirtualCardEnrollmentFields virtualCardEnrollmentFields,
            String acceptButtonText,
            String declineButtonText,
            LinkClickCallback onLinkClicked,
            Callback<Integer> resultHandler) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mPersonalDataManager = personalDataManager;
        mVirtualCardEnrollmentFields = virtualCardEnrollmentFields;
        mAcceptButtonText = acceptButtonText;
        mDeclineButtonText = declineButtonText;
        mOnLinkClicked = onLinkClicked;
        mResultHandler = resultHandler;
    }

    public void show() {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(ModalDialogProperties.CUSTOM_VIEW, getCustomViewForModalDialog())
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mAcceptButtonText)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, mDeclineButtonText)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new SimpleModalDialogController(
                                        mModalDialogManager, mResultHandler));
        mDialogModel = builder.build();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    private View getCustomViewForModalDialog() {
        View customView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.virtual_card_enrollment_dialog, null);

        TextView titleTextView = customView.findViewById(R.id.dialog_title);
        AutofillUiUtils.inlineTitleStringWithLogo(
                mContext,
                titleTextView,
                mContext.getString(R.string.autofill_virtual_card_enrollment_dialog_title_label),
                R.drawable.google_pay_with_divider);

        TextView virtualCardEducationTextView =
                customView.findViewById(R.id.virtual_card_education);
        virtualCardEducationTextView.setText(
                AutofillUiUtils.getSpannableStringWithClickableSpansToOpenLinksInCustomTabs(
                        mContext,
                        R.string.autofill_virtual_card_enrollment_dialog_education_text,
                        ChromeStringConstants.AUTOFILL_VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL,
                        url ->
                                mOnLinkClicked.call(
                                        url,
                                        VirtualCardEnrollmentLinkType
                                                .VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK)));
        virtualCardEducationTextView.setMovementMethod(LinkMovementMethod.getInstance());

        TextView googleLegalMessageTextView = customView.findViewById(R.id.google_legal_message);
        googleLegalMessageTextView.setText(
                AutofillUiUtils.getSpannableStringForLegalMessageLines(
                        mContext,
                        mVirtualCardEnrollmentFields.getGoogleLegalMessages(),
                        /* underlineLinks= */ false,
                        url ->
                                mOnLinkClicked.call(
                                        url,
                                        VirtualCardEnrollmentLinkType
                                                .VIRTUAL_CARD_ENROLLMENT_GOOGLE_PAYMENTS_TOS_LINK)));
        googleLegalMessageTextView.setMovementMethod(LinkMovementMethod.getInstance());

        TextView issuerLegalMessageTextView = customView.findViewById(R.id.issuer_legal_message);
        issuerLegalMessageTextView.setText(
                AutofillUiUtils.getSpannableStringForLegalMessageLines(
                        mContext,
                        mVirtualCardEnrollmentFields.getIssuerLegalMessages(),
                        /* underlineLinks= */ false,
                        url ->
                                mOnLinkClicked.call(
                                        url,
                                        VirtualCardEnrollmentLinkType
                                                .VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK)));
        issuerLegalMessageTextView.setMovementMethod(LinkMovementMethod.getInstance());

        AutofillUiUtils.addCardDetails(
                mContext,
                mPersonalDataManager,
                customView,
                mVirtualCardEnrollmentFields.getCardName(),
                mVirtualCardEnrollmentFields.getCardNumber(),
                mContext.getString(
                        R.string.autofill_virtual_card_enrollment_dialog_card_container_title),
                mVirtualCardEnrollmentFields.getCardArtUrl(),
                mVirtualCardEnrollmentFields.getNetworkIconId(),
                ImageSize.LARGE,
                R.dimen.virtual_card_enrollment_dialog_card_container_issuer_icon_margin_end,
                /* cardNameAndNumberTextAppearance= */ R.style.TextAppearance_TextLarge_Primary,
                /* cardLabelTextAppearance= */ R.style.TextAppearance_TextMedium_Secondary,
                /* showCustomIcon= */ AutofillUiUtils.shouldShowCustomIcon(
                        mVirtualCardEnrollmentFields.getCardArtUrl(), /* isVirtualCard= */ true));

        return customView;
    }
}
