// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.ErrorType;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Prompt that asks users to confirm the expiration date before saving card to Google.
 * TODO(crbug.com/848955)
 * - Confirm if the month and year needs to be pre-populated in case partial data is available.
 */
public class AutofillExpirationDateFixFlowPrompt
        implements TextWatcher, ModalDialogProperties.Controller {
    /**
     * An interface to handle the interaction with an AutofillExpirationDateFixFlowPrompt object.
     */
    public interface AutofillExpirationDateFixFlowPromptDelegate {
        /**
         * Called when dialog is dismissed.
         */
        void onPromptDismissed();

        /**
         * Called when user accepted/confirmed the prompt.
         *
         * @param month expiration date month.
         * @param year expiration date year.
         */
        void onUserAccept(String month, String year);
    }

    private final AutofillExpirationDateFixFlowPromptDelegate mDelegate;
    private final PropertyModel mDialogModel;

    private final View mDialogView;
    private final EditText mMonthInput;
    private final EditText mYearInput;
    private final TextView mErrorMessage;
    private final TextView mCardDetailsMasked;

    private ModalDialogManager mModalDialogManager;
    private Context mContext;

    private boolean mDidFocusOnMonth;
    private boolean mDidFocusOnYear;

    /**
     * Fix flow prompt to confirm expiration date before saving the card to Google.
     */
    public AutofillExpirationDateFixFlowPrompt(Context context,
            AutofillExpirationDateFixFlowPromptDelegate delegate, String title,
            String confirmButtonLabel, int drawableId, String cardLabel) {
        mDelegate = delegate;
        LayoutInflater inflater = LayoutInflater.from(context);
        mDialogView = inflater.inflate(R.layout.autofill_expiration_date_fix_flow, null);
        mErrorMessage = (TextView) mDialogView.findViewById(R.id.error_message);
        mCardDetailsMasked = (TextView) mDialogView.findViewById(R.id.cc_details_masked);
        mCardDetailsMasked.setText(cardLabel);

        mMonthInput = (EditText) mDialogView.findViewById(R.id.cc_month_edit);
        mMonthInput.addTextChangedListener(this);
        mMonthInput.setOnFocusChangeListener((view, hasFocus) -> {
            mDidFocusOnMonth |= hasFocus;
        });

        mYearInput = (EditText) mDialogView.findViewById(R.id.cc_year_edit);
        mYearInput.addTextChangedListener(this);
        mYearInput.setOnFocusChangeListener((view, hasFocus) -> {
            mDidFocusOnYear |= hasFocus;
        });

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, confirmButtonLabel)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, context.getResources(),
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true);
        if (drawableId != 0) {
            builder.with(ModalDialogProperties.TITLE_ICON, context, drawableId);
        }
        mDialogModel = builder.build();
    }

    /**
     * Show the dialog. If activity is null this method will not do anything.
     */
    public void show(ChromeActivity activity) {
        if (activity == null) {
            return;
        }

        mContext = activity;
        mModalDialogManager = activity.getModalDialogManager();
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    protected void dismiss(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mDialogModel, dismissalCause);
    }

    @Override
    public void afterTextChanged(Editable s) {
        validate();
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {}

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            String monthString = mMonthInput.getText().toString().trim();
            String yearString = mYearInput.getText().toString().trim();
            mDelegate.onUserAccept(monthString, yearString);
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        // Do not call dismissed on the delegate if dialog was dismissed either because the user
        // accepted to save the card or was dismissed by native code.
        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                && dismissalCause != DialogDismissalCause.DISMISSED_BY_NATIVE) {
            mDelegate.onPromptDismissed();
        }
    }

    /**
     * Validates the values of the input fields to determine whether the submit button should be
     * enabled. Also displays a detailed error message and highlights the fields for which the value
     * is wrong. Finally checks whether the focus should move to the next field.
     */
    private void validate() {
        @ErrorType
        int errorType = AutofillUiUtils.getExpirationDateErrorType(
                mMonthInput, mYearInput, mDidFocusOnMonth, mDidFocusOnYear);
        mDialogModel.set(
                ModalDialogProperties.POSITIVE_BUTTON_DISABLED, errorType != ErrorType.NONE);
        AutofillUiUtils.showDetailedErrorMessage(errorType, mContext, mErrorMessage);
        AutofillUiUtils.updateColorForInputs(
                errorType, mContext, mMonthInput, mYearInput, /*cvcInput=*/null);
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
