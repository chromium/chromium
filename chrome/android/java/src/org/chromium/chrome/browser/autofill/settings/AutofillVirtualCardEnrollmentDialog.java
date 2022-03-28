// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.VirtualCardDialogLink;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/** Dialog shown to the user to enroll a credit card into the virtual card feature. */
public class AutofillVirtualCardEnrollmentDialog {
    private static final String LINK_CLICK_HISTOGRAM =
            "Autofill.VirtualCard.SettingsPageEnrollment.LinkClicked";

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final VirtualCardEnrollmentFields mVirtualCardEnrollmentFields;
    private final Callback<Boolean> mResultHandler;

    public AutofillVirtualCardEnrollmentDialog(Context context,
            ModalDialogManager modalDialogManager,
            VirtualCardEnrollmentFields virtualCardEnrollmentFields,
            Callback<Boolean> resultHandler) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mVirtualCardEnrollmentFields = virtualCardEnrollmentFields;
        mResultHandler = resultHandler;
    }

    public void show() {
        PropertyModel.Builder dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(ModalDialogProperties.CUSTOM_VIEW, getCustomViewForModalDialog())
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getString(
                                        R.string.autofill_virtual_card_enrollment_accept_button_label))
                        .with(ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getString(R.string.no_thanks));
        dialogModel.with(ModalDialogProperties.CONTROLLER,
                new SimpleModalDialogController(mModalDialogManager, (action) -> {
                    RecordHistogram.recordBooleanHistogram(
                            "Autofill.VirtualCard.SettingsPageEnrollment",
                            action == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    mResultHandler.onResult(action == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                }));
        mModalDialogManager.showDialog(dialogModel.build(), ModalDialogManager.ModalDialogType.APP);
    }

    private View getCustomViewForModalDialog() {
        View customView = LayoutInflater.from(mContext).inflate(
                R.layout.virtual_card_enrollment_dialog, null);

        TextView titleTextView = (TextView) customView.findViewById(R.id.dialog_title);
        AutofillUiUtils.inlineTitleStringWithLogo(mContext, titleTextView,
                mContext.getString(R.string.autofill_virtual_card_enrollment_dialog_title_label),
                R.drawable.google_pay_with_divider);

        TextView virtualCardEducationTextView =
                (TextView) customView.findViewById(R.id.virtual_card_education);
        virtualCardEducationTextView.setText(
                AutofillUiUtils.getSpannableStringWithClickableSpansToOpenLinksInCustomTabs(
                        mContext, R.string.autofill_virtual_card_enrollment_dialog_education_text,
                        ChromeStringConstants.AUTOFILL_VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL, url -> {
                            RecordHistogram.recordEnumeratedHistogram(LINK_CLICK_HISTOGRAM,
                                    VirtualCardDialogLink.EDUCATION_TEXT,
                                    VirtualCardDialogLink.NUM_ENTRIES);
                            CustomTabActivity.showInfoPage(mContext, url);
                        }));
        virtualCardEducationTextView.setMovementMethod(LinkMovementMethod.getInstance());

        TextView googleLegalMessageTextView =
                (TextView) customView.findViewById(R.id.google_legal_message);
        googleLegalMessageTextView.setText(AutofillUiUtils.getSpannableStringForLegalMessageLines(
                mContext, mVirtualCardEnrollmentFields.getGoogleLegalMessages(),
                /* underlineLinks= */ false, url -> {
                    RecordHistogram.recordEnumeratedHistogram(LINK_CLICK_HISTOGRAM,
                            VirtualCardDialogLink.GOOGLE_LEGAL_MESSAGE,
                            VirtualCardDialogLink.NUM_ENTRIES);
                    CustomTabActivity.showInfoPage(mContext, url);
                }));
        googleLegalMessageTextView.setMovementMethod(LinkMovementMethod.getInstance());

        TextView issuerLegalMessageTextView =
                (TextView) customView.findViewById(R.id.issuer_legal_message);
        issuerLegalMessageTextView.setText(AutofillUiUtils.getSpannableStringForLegalMessageLines(
                mContext, mVirtualCardEnrollmentFields.getIssuerLegalMessages(),
                /* underlineLinks= */ false, url -> {
                    RecordHistogram.recordEnumeratedHistogram(LINK_CLICK_HISTOGRAM,
                            VirtualCardDialogLink.ISSUER_LEGAL_MESSAGE,
                            VirtualCardDialogLink.NUM_ENTRIES);
                    CustomTabActivity.showInfoPage(mContext, url);
                }));
        issuerLegalMessageTextView.setMovementMethod(LinkMovementMethod.getInstance());

        ((TextView) customView.findViewById(R.id.credit_card_identifier))
                .setText(mVirtualCardEnrollmentFields.getCardIdentifierString());
        ((ImageView) customView.findViewById(R.id.credit_card_issuer_icon))
                .setImageBitmap(mVirtualCardEnrollmentFields.getIssuerCardArt());
        return customView;
    }
}
