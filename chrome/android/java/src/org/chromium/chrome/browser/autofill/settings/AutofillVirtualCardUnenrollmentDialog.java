// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.AutofillUiUtils;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/** Dialog that confirms whether the user wishes to unenroll their card from Virtual Cards. */
public class AutofillVirtualCardUnenrollmentDialog {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final Callback<Boolean> mResultHandler;

    public AutofillVirtualCardUnenrollmentDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            Callback<Boolean> resultHandler) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mResultHandler = resultHandler;
    }

    /** Shows an AutofillVirtualCardUnenrollmentDialog. */
    public void show() {
        var context = mContext;
        var resources = context.getResources();
        var modalDialogController =
                new SimpleModalDialogController(
                        mModalDialogManager,
                        result -> {
                            boolean wasClicked =
                                    result == DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
                            RecordHistogram.recordBooleanHistogram(
                                    "Autofill.VirtualCard.SettingsPageUnenrollment", wasClicked);
                            mResultHandler.onResult(wasClicked);
                        });
        var paragraph1 =
                AutofillUiUtils.getSpannableStringWithClickableSpansToOpenLinksInCustomTabs(
                        context,
                        R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_message,
                        ChromeStringConstants.AUTOFILL_VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL,
                        url -> {
                            RecordHistogram.recordEnumeratedHistogram(
                                    "Autofill.VirtualCard.SettingsPageUnenrollment.LinkClicked",
                                    VirtualCardEnrollmentLinkType
                                            .VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
                                    VirtualCardEnrollmentLinkType.MAX_VALUE + 1);
                            CustomTabActivity.showInfoPage(context, url);
                        });
        int unenrollTitle = R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_title;
        int positiveButtonLabel =
                R.string
                        .autofill_credit_card_editor_virtual_card_unenroll_dialog_positive_button_label;
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, modalDialogController)
                        .with(ModalDialogProperties.TITLE, resources, unenrollTitle)
                        .with(ModalDialogProperties.MESSAGE_PARAGRAPH_1, paragraph1)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                positiveButtonLabel)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                android.R.string.cancel);

        mModalDialogManager.showDialog(builder.build(), ModalDialogManager.ModalDialogType.APP);
    }
}
