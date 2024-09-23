// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.text.Editable;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.ErrorType;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;

/**
 * Prompt that asks users to confirm the expiration date before saving card to Google.
 * TODO(crbug.com/40579040) - Confirm if the month and year needs to be pre-populated in case
 * partial data is available.
 */
public class AutofillExpirationDateFixFlowPrompt extends AutofillSaveCardPromptBase
        implements EmptyTextWatcher {
    /**
     * An interface to handle the interaction with an AutofillExpirationDateFixFlowPrompt object.
     */
    public interface AutofillExpirationDateFixFlowPromptDelegate
            extends AutofillSaveCardPromptBaseDelegate {
        /**
         * Called when user accepted/confirmed the prompt.
         *
         * @param month expiration date month.
         * @param year expiration date year.
         */
        void onUserAcceptExpirationDate(String month, String year);
    }

    /**
     * Create a prompt dialog for the use of infobar. This dialog does not include legal lines.
     *
     * @param context The current context.
     * @param delegate A {@link AutofillExpirationDateFixFlowPromptDelegate} to handle events.
     * @param title Title of the dialog prompt.
     * @param drawableId Drawable id on the title.
     * @param cardLabel Label representing a card which will be saved.
     * @param confirmButtonLabel Label for the confirm button.
     * @return The prompt to confirm expiration data.
     */
    public static AutofillExpirationDateFixFlowPrompt createAsInfobarFixFlowPrompt(
            Context context,
            AutofillExpirationDateFixFlowPromptDelegate delegate,
            String title,
            int drawableId,
            String cardLabel,
            String confirmButtonLabel) {
        return new AutofillExpirationDateFixFlowPrompt(
                context, delegate, title, drawableId, cardLabel, confirmButtonLabel, false);
    }

    private final AutofillExpirationDateFixFlowPromptDelegate mDelegate;

    private final EditText mMonthInput;
    private final EditText mYearInput;
    private final TextView mErrorMessage;

    private boolean mDidFocusOnMonth;
    private boolean mDidFocusOnYear;

    /** Fix flow prompt to confirm expiration date before saving the card to Google. */
    private AutofillExpirationDateFixFlowPrompt(
            Context context,
            AutofillExpirationDateFixFlowPromptDelegate delegate,
            String title,
            int drawableId,
            String cardLabel,
            String confirmButtonLabel,
            boolean filledConfirmButton) {
        super(
                context,
                delegate,
                R.layout.autofill_expiration_date_fix_flow,
                R.layout.icon_after_title_view,
                title,
                drawableId,
                confirmButtonLabel,
                filledConfirmButton);
        mDelegate = delegate;
        mErrorMessage = mDialogView.findViewById(R.id.error_message);
        // Infobar: show masked card number only.
        TextView cardDetailsMasked = mDialogView.findViewById(R.id.cc_details_masked);
        cardDetailsMasked.setText(cardLabel);
        mDialogView.findViewById(R.id.message_divider).setVisibility(View.GONE);
        mDialogView.findViewById(R.id.google_pay_logo).setVisibility(View.GONE);

        mMonthInput = mDialogView.findViewById(R.id.cc_month_edit);
        mMonthInput.addTextChangedListener(this);
        mMonthInput.setOnFocusChangeListener(
                (view, hasFocus) -> {
                    mDidFocusOnMonth |= hasFocus;
                });

        mYearInput = mDialogView.findViewById(R.id.cc_year_edit);
        mYearInput.addTextChangedListener(this);
        mYearInput.setOnFocusChangeListener(
                (view, hasFocus) -> {
                    mDidFocusOnYear |= hasFocus;
                });
    }

    @Override
    public void afterTextChanged(Editable s) {
        validate();
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            String monthString = mMonthInput.getText().toString().trim();
            String yearString = mYearInput.getText().toString().trim();
            mDelegate.onUserAcceptExpirationDate(monthString, yearString);
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        // Do not call onUserDismiss if dialog was dismissed either because the user
        // accepted to save the card or was dismissed by native code.
        if (dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
            mDelegate.onUserDismiss();
        }
        // Call whenever the dialog is dismissed.
        mDelegate.onPromptDismissed();
    }

    /**
     * Validates the values of the input fields to determine whether the submit button should be
     * enabled. Also displays a detailed error message and highlights the fields for which the value
     * is wrong. Finally checks whether the focus should move to the next field.
     */
    private void validate() {
        @ErrorType
        int errorType =
                AutofillUiUtils.getExpirationDateErrorType(
                        mMonthInput, mYearInput, mDidFocusOnMonth, mDidFocusOnYear);
        mDialogModel.set(
                ModalDialogProperties.POSITIVE_BUTTON_DISABLED, errorType != ErrorType.NONE);
        AutofillUiUtils.showDetailedErrorMessage(errorType, mContext, mErrorMessage);
        AutofillUiUtils.updateColorForInputs(
                errorType, mContext, mMonthInput, mYearInput, /* cvcInput= */ null);
        moveFocus(errorType);
    }

    /**
     * Moves the focus to the next field based on the value of the fields and the specified type of
     * error found for the expiration date field(s).
     *
     * @param errorType The type of error detected.
     */
    private void moveFocus(@ErrorType int errorType) {
        if (mMonthInput.isFocused()
                && mMonthInput.getText().length() == AutofillUiUtils.EXPIRATION_FIELDS_LENGTH) {
            // The user just finished typing in the month field and if there are no errors in the
            // month, then move focus to the year input.
            if (errorType != ErrorType.EXPIRATION_MONTH) {
                mYearInput.requestFocus();
                mDidFocusOnYear = true;
            }
        }
    }
}
