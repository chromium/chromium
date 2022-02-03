// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.text.SpannableString;
import android.view.View;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * Dialog that confirms whether the user wishes to unenroll their card from Virtual Cards.
 */
public class AutofillVirtualCardUnenrollmentDialog {
    // LINT.IfChange
    private static final String VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL =
            "https://support.google.com/googlepay/answer/11234179?hl=en";
    // LINT.ThenChange(//components/autofill/core/browser/payments/payments_service_url.cc)

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final Callback<Boolean> mResultHandler;

    public AutofillVirtualCardUnenrollmentDialog(Context context,
            ModalDialogManager modalDialogManager, Callback<Boolean> resultHandler) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mResultHandler = resultHandler;
    }

    /**
     * Shows an AutofillVirtualCardUnenrollmentDialog.
     */
    public void show() {
        SimpleModalDialogController modalDialogController =
                new SimpleModalDialogController(mModalDialogManager, result -> {
                    mResultHandler.onResult(result == DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                });
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, modalDialogController)
                        .with(ModalDialogProperties.TITLE,
                                mContext.getString(
                                        R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_title))
                        .with(ModalDialogProperties.MESSAGE, buildUnenrollMessageWithLink())
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getString(
                                        R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_positive_button_label))
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getString(android.R.string.cancel));

        mModalDialogManager.showDialog(builder.build(), ModalDialogManager.ModalDialogType.APP);
    }

    private SpannableString buildUnenrollMessageWithLink() {
        return SpanApplier.applySpans(
                mContext.getString(
                        R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_message,
                        VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL),
                new SpanInfo("<link1>", "</link1>",
                        new NoUnderlineClickableSpan(
                                mContext.getResources(), createOpenMyActivityCallback())));
    }

    private Callback<View> createOpenMyActivityCallback() {
        return (widget) -> {
            CustomTabsIntent customTabIntent =
                    new CustomTabsIntent.Builder().setShowTitle(true).build();
            customTabIntent.intent.setData(Uri.parse(VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL));
            Intent intent = LaunchIntentDispatcher.createCustomTabActivityIntent(
                    mContext, customTabIntent.intent);
            intent.setPackage(mContext.getPackageName());
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
            IntentUtils.addTrustedIntentExtras(intent);
            IntentUtils.safeStartActivity(mContext, intent);
        };
    }
}
