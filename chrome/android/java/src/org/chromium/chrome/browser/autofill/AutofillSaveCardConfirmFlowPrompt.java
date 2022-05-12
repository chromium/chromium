// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.view.View;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Prompt that asks users to confirm card before saving card to Google.
 */
public class AutofillSaveCardConfirmFlowPrompt extends AutofillSaveCardPromptBase {
    /**
     * An interface to handle the interaction with an AutofillSaveCardConfirmFlowPrompt object.
     */
    public interface AutofillSaveCardConfirmFlowPromptDelegate
            extends AutofillSaveCardPromptBaseDelegate {
        /**
         * Called when user accepted/confirmed the prompt.
         */
        void onUserConfirmedCard();
    }

    /**
     * Create a dialog prompt for the use of message. This dialog prompt includes legal lines.
     *
     * @param context The current context.
     * @param delegate A {@link AutofillSaveCardConfirmFlowPromptDelegate} to handle events.
     * @param title Title of the dialog prompt.
     * @param cardLabel Label representing a card which will be saved.
     * @param cardholderAccount The Google account where a card will be saved.
     * @param confirmButtonLabel Label for the confirm button.
     * @return A {@link AutofillSaveCardConfirmFlowPrompt} to confirm saving card.
     */
    public static AutofillSaveCardConfirmFlowPrompt createPrompt(Context context,
            AutofillSaveCardConfirmFlowPromptDelegate delegate, String title, String cardLabel,
            String cardholderAccount, String confirmButtonLabel) {
        return new AutofillSaveCardConfirmFlowPrompt(
                context, delegate, title, cardLabel, cardholderAccount, confirmButtonLabel);
    }

    private final AutofillSaveCardConfirmFlowPromptDelegate mDelegate;

    /**
     * Confirm flow prompt before saving the card to Google.
     */
    private AutofillSaveCardConfirmFlowPrompt(Context context,
            AutofillSaveCardConfirmFlowPromptDelegate delegate, String title, String cardLabel,
            String cardholderAccount, String confirmButtonLabel) {
        super(context, delegate, 0, title, 0, cardholderAccount, confirmButtonLabel, true);
        mDelegate = delegate;
        TextView cardDetailsMasked = (TextView) mDialogView.findViewById(R.id.cc_details_masked);
        cardDetailsMasked.setText(cardLabel);
        mDialogView.findViewById(R.id.message_divider).setVisibility(View.VISIBLE);
        mDialogView.findViewById(R.id.google_pay_logo).setVisibility(View.VISIBLE);
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, false);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            mDelegate.onUserConfirmedCard();
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        mDelegate.onPromptDismissed();
    }
}
