// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.text.Editable;
import android.text.InputFilter;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextWatcher;
import android.text.style.ImageSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.TextView.BufferType;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.ErrorType;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Calendar;

/**
 * A prompt that bugs users to enter their CVC when unmasking a Wallet instrument (credit card).
 */
public class CardUnmaskPrompt
        implements TextWatcher, OnClickListener, ModalDialogProperties.Controller {
    private static CardUnmaskObserverForTest sObserverForTest;

    private final CardUnmaskPromptDelegate mDelegate;
    private PropertyModel mDialogModel;
    private boolean mShouldRequestExpirationDate;

    private final View mMainView;
    private final TextView mInstructions;
    private final TextView mTitleView;
    private final TextView mNoRetryErrorMessage;
    private final EditText mCardUnmaskInput;
    private final EditText mMonthInput;
    private final EditText mYearInput;
    private final View mExpirationContainer;
    private final TextView mNewCardLink;
    private final TextView mErrorMessage;
    private final CheckBox mUseScreenlockCheckbox;
    private final ViewGroup mControlsContainer;
    private final View mVerificationOverlay;
    private final ProgressBar mVerificationProgressBar;
    private final TextView mVerificationView;
    private final long mSuccessMessageDurationMilliseconds;
    private final int mGooglePayDrawableId;
    private final boolean mIsCardLocal;
    private final boolean mIsVirtualCard;

    private int mThisYear;
    private int mThisMonth;
    private boolean mValidationWaitsForCalendarTask;
    private ModalDialogManager mModalDialogManager;
    private Context mContext;

    private boolean mDidFocusOnMonth;
    private boolean mDidFocusOnYear;
    private boolean mDidFocusOnCvc;

    /**
     * An interface to handle the interaction with an CardUnmaskPrompt object.
     */
    public interface CardUnmaskPromptDelegate {
        /**
         * Called when the dialog has been dismissed.
         */
        void dismissed();

        /**
         * Returns whether |userResponse| represents a valid value.
         * @param userResponse A CVC entered by the user.
         */
        boolean checkUserInputValidity(String userResponse);

        /**
         * Called when the user has entered a value and pressed "verify".
         * @param cvc The value the user entered (a CVC), or an empty string if the user canceled.
         * @param month The value the user selected for expiration month, if any.
         * @param year The value the user selected for expiration month, if any.
         * @param enableFidoAuth The value the user selected for the use lockscreen checkbox.
         */
        void onUserInput(String cvc, String month, String year, boolean enableFidoAuth);

        /**
         * Called when the "New card?" link has been clicked.
         * The controller will call update() in response.
         */
        void onNewCardLinkClicked();

        /**
         * Returns the expected length of the CVC for the card.
         */
        int getExpectedCvcLength();
    }

    /**
     * A test-only observer for the unmasking prompt.
     */
    public interface CardUnmaskObserverForTest {
        /**
         * Called when typing the CVC input is possible.
         */
        void onCardUnmaskPromptReadyForInput(CardUnmaskPrompt prompt);

        /**
         * Called when clicking "Verify" or "Continue" (the positive button) is possible.
         */
        void onCardUnmaskPromptReadyToUnmask(CardUnmaskPrompt prompt);

        /**
         * Called when the input values in the unmask prompt have been validated.
         */
        void onCardUnmaskPromptValidationDone(CardUnmaskPrompt prompt);

        /**
         * Called when submitting through the soft keyboard was disallowed.
         */
        void onCardUnmaskPromptSubmitRejected(CardUnmaskPrompt prompt);
    }

    public CardUnmaskPrompt(Context context, CardUnmaskPromptDelegate delegate, String title,
            String instructions, String confirmButtonLabel, int cvcDrawableId,
            int googlePayDrawableId, boolean isCardLocal, boolean isVirtualCard,
            boolean shouldRequestExpirationDate, boolean defaultToStoringLocally,
            boolean shouldOfferWebauthn, boolean defaultUseScreenlockChecked,
            long successMessageDurationMilliseconds) {
        mDelegate = delegate;
        mGooglePayDrawableId = googlePayDrawableId;
        mIsCardLocal = isCardLocal;
        mIsVirtualCard = isVirtualCard;

        LayoutInflater inflater = LayoutInflater.from(context);
        View v = inflater.inflate(R.layout.autofill_card_unmask_prompt, null);
        mInstructions = (TextView) v.findViewById(R.id.instructions);
        mInstructions.setText(instructions);
        mTitleView = (TextView) v.findViewById(R.id.title);

        mMainView = v;
        mNoRetryErrorMessage = (TextView) v.findViewById(R.id.no_retry_error_message);
        mCardUnmaskInput = (EditText) v.findViewById(R.id.card_unmask_input);
        mMonthInput = (EditText) v.findViewById(R.id.expiration_month);
        mYearInput = (EditText) v.findViewById(R.id.expiration_year);
        mExpirationContainer = v.findViewById(R.id.expiration_container);
        mNewCardLink = (TextView) v.findViewById(R.id.new_card_link);
        mNewCardLink.setOnClickListener(this);
        mErrorMessage = (TextView) v.findViewById(R.id.error_message);
        mUseScreenlockCheckbox = (CheckBox) v.findViewById(R.id.use_screenlock_checkbox);
        mUseScreenlockCheckbox.setChecked(defaultUseScreenlockChecked);
        if (!shouldOfferWebauthn) {
            mUseScreenlockCheckbox.setVisibility(View.GONE);
            mUseScreenlockCheckbox.setChecked(false);
        }
        mControlsContainer = (ViewGroup) v.findViewById(R.id.controls_container);
        mVerificationOverlay = v.findViewById(R.id.verification_overlay);
        mVerificationProgressBar = (ProgressBar) v.findViewById(R.id.verification_progress_bar);
        mVerificationView = (TextView) v.findViewById(R.id.verification_message);
        mSuccessMessageDurationMilliseconds = successMessageDurationMilliseconds;
        ((ImageView) v.findViewById(R.id.cvc_hint_image)).setImageResource(cvcDrawableId);

        Resources resources = context.getResources();
        String modalDialogTitle = null;
        if (isCardLocal) {
            mTitleView.setVisibility(View.GONE);
            modalDialogTitle = title;
        } else {
            updateTitleForCustomView(title, context);
        }
        PropertyModel.Builder dialogModelBuilder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.CUSTOM_VIEW, v)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, confirmButtonLabel)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, resources,
                                R.string.cancel);
        if (modalDialogTitle != null) {
            dialogModelBuilder.with(ModalDialogProperties.TITLE, modalDialogTitle);
        }
        mDialogModel = dialogModelBuilder.build();

        mShouldRequestExpirationDate = shouldRequestExpirationDate;
        mThisYear = -1;
        mThisMonth = -1;
        if (mShouldRequestExpirationDate) {
            new CalendarTask().executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }

        // Set the max length of the CVC field.
        mCardUnmaskInput.setFilters(
                new InputFilter[] {new InputFilter.LengthFilter(mDelegate.getExpectedCvcLength())});

        // Hitting the "submit" button on the software keyboard should submit the form if valid.
        mCardUnmaskInput.setOnEditorActionListener((v14, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_DONE) {
                if (!mDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED)) {
                    onClick(mDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
                } else if (sObserverForTest != null) {
                    sObserverForTest.onCardUnmaskPromptSubmitRejected(this);
                }
                return true;
            }
            return false;
        });

        // Create the listeners to be notified when the user focuses out the input fields.
        mCardUnmaskInput.setOnFocusChangeListener((v13, hasFocus) -> {
            mDidFocusOnCvc = true;
            validate();
        });
        mMonthInput.setOnFocusChangeListener((v12, hasFocus) -> {
            mDidFocusOnMonth = true;
            validate();
        });
        mYearInput.setOnFocusChangeListener((v1, hasFocus) -> {
            mDidFocusOnYear = true;
            validate();
        });
    }

    /**
     * Avoids disk reads for timezone when getting the default instance of Calendar.
     */
    private class CalendarTask extends AsyncTask<Calendar> {
        @Override
        protected Calendar doInBackground() {
            return Calendar.getInstance();
        }

        @Override
        protected void onPostExecute(Calendar result) {
            mThisYear = result.get(Calendar.YEAR);
            mThisMonth = result.get(Calendar.MONTH) + 1;
            if (mValidationWaitsForCalendarTask) validate();
        }
    }

    /**
     * Show the dialog.
     *
     * @param activity The current activity, used for context. When null, the method does nothing.
     * @param modalDialogManager Used to display modal dialogs. When null, the method does nothing.
     */
    public void show(@Nullable Activity activity, @Nullable ModalDialogManager modalDialogManager) {
        if (activity == null || modalDialogManager == null) return;

        mContext = activity;
        mModalDialogManager = modalDialogManager;

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);

        showExpirationDateInputsInputs();

        // Override the View.OnClickListener so that pressing the positive button doesn't dismiss
        // the dialog.
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true);
        mCardUnmaskInput.addTextChangedListener(this);
        mCardUnmaskInput.post(() -> setInitialFocus());
    }

    public void update(String title, String instructions, boolean shouldRequestExpirationDate) {
        if (mIsCardLocal) {
            mDialogModel.set(ModalDialogProperties.TITLE, title);
        } else {
            updateTitleForCustomView(title, mContext);
            mDialogModel.set(ModalDialogProperties.CUSTOM_VIEW, mMainView);
        }
        mInstructions.setText(instructions);
        mShouldRequestExpirationDate = shouldRequestExpirationDate;
        if (mShouldRequestExpirationDate && (mThisYear == -1 || mThisMonth == -1)) {
            new CalendarTask().executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
        showExpirationDateInputsInputs();
    }

    private void updateTitleForCustomView(String title, Context context) {
        Drawable mInlineTitleIcon = context.getDrawable(mGooglePayDrawableId);
        // The first character will be replaced by the logo, and the consecutive spaces after
        // are used as padding.
        SpannableString titleWithLogo = new SpannableString("   " + title);
        // How much the original logo should scale up in size to match height of text.
        float scaleFactor = mTitleView.getTextSize() / mInlineTitleIcon.getIntrinsicHeight();
        mInlineTitleIcon.setBounds(
                /* left */ 0, /* top */ 0,
                /* right */ (int) (scaleFactor * mInlineTitleIcon.getIntrinsicWidth()),
                /* bottom */ (int) (scaleFactor * mInlineTitleIcon.getIntrinsicHeight()));
        titleWithLogo.setSpan(new ImageSpan(mInlineTitleIcon, ImageSpan.ALIGN_CENTER),
                /* start */ 0,
                /* end */ 1,
                /* flags */ Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        mTitleView.setText(titleWithLogo, BufferType.SPANNABLE);
    }

    public void dismiss(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mDialogModel, dismissalCause);
    }

    public void disableAndWaitForVerification() {
        setInputsEnabled(false);
        setOverlayVisibility(View.VISIBLE);
        mVerificationProgressBar.setVisibility(View.VISIBLE);
        mVerificationView.setText(R.string.autofill_card_unmask_verification_in_progress);
        mVerificationView.announceForAccessibility(mVerificationView.getText());
        clearInputError();
    }

    public void verificationFinished(String errorMessage, boolean allowRetry) {
        if (errorMessage != null) {
            setOverlayVisibility(View.GONE);
            if (allowRetry) {
                AutofillUiUtils.showErrorMessage(errorMessage, mErrorMessage);
                setInputsEnabled(true);
                setInitialFocus();

                if (!mShouldRequestExpirationDate && !mIsVirtualCard) {
                    mNewCardLink.setVisibility(View.VISIBLE);
                } else {
                    mNewCardLink.setVisibility(View.GONE);
                }
            } else {
                clearInputError();
                setNoRetryError(errorMessage);
            }
        } else {
            Runnable dismissRunnable = () -> dismiss(DialogDismissalCause.ACTION_ON_CONTENT);
            if (mSuccessMessageDurationMilliseconds > 0) {
                mVerificationProgressBar.setVisibility(View.GONE);
                mMainView.findViewById(R.id.verification_success).setVisibility(View.VISIBLE);
                mVerificationView.setText(R.string.autofill_card_unmask_verification_success);
                mVerificationView.announceForAccessibility(mVerificationView.getText());
                new Handler().postDelayed(dismissRunnable, mSuccessMessageDurationMilliseconds);
            } else {
                new Handler().post(dismissRunnable);
            }
        }
    }

    @Override
    public void afterTextChanged(Editable s) {
        validate();
    }

    /**
     * Validates the values of the input fields to determine whether the submit button should be
     * enabled. Also displays a detailed error message and highlights the fields for which the value
     * is wrong. Finally checks whether the focuse should move to the next field.
     */
    private void validate() {
        @ErrorType int errorType = getExpirationAndCvcErrorType();
        mDialogModel.set(
                ModalDialogProperties.POSITIVE_BUTTON_DISABLED, errorType != ErrorType.NONE);
        AutofillUiUtils.showDetailedErrorMessage(errorType, mContext, mErrorMessage);
        AutofillUiUtils.updateColorForInputs(
                errorType, mContext, mMonthInput, mYearInput, mCardUnmaskInput);
        moveFocus(errorType);

        if (sObserverForTest != null) {
            sObserverForTest.onCardUnmaskPromptValidationDone(this);

            if (!mDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED)) {
                sObserverForTest.onCardUnmaskPromptReadyToUnmask(this);
            }
        }
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {}

    @Override
    public void onClick(View v) {
        assert v == mNewCardLink;
        onNewCardLinkClicked();
    }

    private void showExpirationDateInputsInputs() {
        if (!mShouldRequestExpirationDate || mExpirationContainer.getVisibility() == View.VISIBLE) {
            return;
        }

        mExpirationContainer.setVisibility(View.VISIBLE);
        mCardUnmaskInput.setEms(3);
        mMonthInput.addTextChangedListener(this);
        mYearInput.addTextChangedListener(this);
    }

    private void onNewCardLinkClicked() {
        mDelegate.onNewCardLinkClicked();
        assert mShouldRequestExpirationDate;
        mNewCardLink.setVisibility(View.GONE);
        mCardUnmaskInput.setText(null);
        clearInputError();
        mMonthInput.requestFocus();
    }

    private void setInitialFocus() {
        InputMethodManager imm =
                (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        View view = mShouldRequestExpirationDate ? mMonthInput : mCardUnmaskInput;
        view.requestFocus();
        imm.showSoftInput(view, InputMethodManager.SHOW_IMPLICIT);
        view.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
        if (sObserverForTest != null) {
            sObserverForTest.onCardUnmaskPromptReadyForInput(this);
        }
    }

    /**
     * Moves the focus to the next field based on the value of the fields and the specified type of
     * error found for the unmask field(s).
     *
     * @param errorType The type of error detected.
     */
    private void moveFocus(@ErrorType int errorType) {
        if (errorType == ErrorType.NOT_ENOUGH_INFO) {
            if (mMonthInput.isFocused()
                    && mMonthInput.getText().length() == AutofillUiUtils.EXPIRATION_FIELDS_LENGTH) {
                // The user just finished typing in the month field and there are no validation
                // errors.
                if (mYearInput.getText().length() == AutofillUiUtils.EXPIRATION_FIELDS_LENGTH) {
                    // Year was already filled, move focus to CVC field.
                    mCardUnmaskInput.requestFocus();
                    mDidFocusOnCvc = true;
                } else {
                    // Year was not filled, move focus there.
                    mYearInput.requestFocus();
                    mDidFocusOnYear = true;
                }
            } else if (mYearInput.isFocused()
                    && mYearInput.getText().length() == AutofillUiUtils.EXPIRATION_FIELDS_LENGTH) {
                // The user just finished typing in the year field and there are no validation
                // errors. Move focus to CVC field.
                mCardUnmaskInput.requestFocus();
                mDidFocusOnCvc = true;
            }
        }
    }

    /**
     * Determines what type of error, if any, is present in the cvc and expiration date fields of
     * the prompt.
     *
     * @return The ErrorType value representing the type of error found for the unmask fields.
     */
    @ErrorType private int getExpirationAndCvcErrorType() {
        @ErrorType
        int errorType = ErrorType.NONE;

        if (mShouldRequestExpirationDate) {
            errorType = AutofillUiUtils.getExpirationDateErrorType(
                    mMonthInput, mYearInput, mDidFocusOnMonth, mDidFocusOnYear);
        }

        // If the CVC is valid, return the error type determined so far.
        if (isCvcValid()) return errorType;

        if (mDidFocusOnCvc && !mCardUnmaskInput.isFocused()) {
            // The CVC is invalid and the user has typed in the CVC field, but is not focused on it
            // now. Add the CVC error to the current error.
            if (errorType == ErrorType.NONE || errorType == ErrorType.NOT_ENOUGH_INFO) {
                errorType = ErrorType.CVC;
            } else {
                errorType = ErrorType.CVC_AND_EXPIRATION;
            }
        } else {
            // The CVC is invalid but the user is not done with the field.
            // If no other errors were detected, set that there is not enough information.
            if (errorType == ErrorType.NONE) errorType = ErrorType.NOT_ENOUGH_INFO;
        }

        return errorType;
    }

    /**
     * Makes a call to the native code to determine if the value in the CVC input field is valid.
     *
     * @return Whether the CVC is valid.
     */
    private boolean isCvcValid() {
        return mDelegate.checkUserInputValidity(mCardUnmaskInput.getText().toString());
    }

    /**
     * Sets the enabled state of the main contents, and hides or shows the verification overlay.
     * @param enabled True if the inputs should be useable, false if the verification overlay
     *        obscures them.
     */
    private void setInputsEnabled(boolean enabled) {
        mCardUnmaskInput.setEnabled(enabled);
        mMonthInput.setEnabled(enabled);
        mYearInput.setEnabled(enabled);
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, !enabled);
    }

    /**
     * Updates the verification overlay and main contents such that the overlay has |visibility|.
     * @param visibility A View visibility enumeration value.
     */
    private void setOverlayVisibility(int visibility) {
        mVerificationOverlay.setVisibility(visibility);
        mControlsContainer.setAlpha(1f);
        boolean contentsShowing = visibility == View.GONE;
        if (!contentsShowing) {
            int durationMs = 250;
            mVerificationOverlay.setAlpha(0f);
            mVerificationOverlay.animate().alpha(1f).setDuration(durationMs);
            mControlsContainer.animate().alpha(0f).setDuration(durationMs);
        }
        ViewCompat.setImportantForAccessibility(mControlsContainer,
                contentsShowing ? View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
                                : View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        mControlsContainer.setDescendantFocusability(
                contentsShowing ? ViewGroup.FOCUS_BEFORE_DESCENDANTS
                                : ViewGroup.FOCUS_BLOCK_DESCENDANTS);
    }

    /**
     * Removes the error message on the inputs.
     */
    private void clearInputError() {
        AutofillUiUtils.clearInputError(mErrorMessage);
        // Remove the highlight on the input fields.
        AutofillUiUtils.updateColorForInputs(
                ErrorType.NONE, mContext, mMonthInput, mYearInput, mCardUnmaskInput);
    }

    /**
     * Displays an error that indicates the user can't retry.
     */
    private void setNoRetryError(String message) {
        mNoRetryErrorMessage.setText(message);
        mNoRetryErrorMessage.setVisibility(View.VISIBLE);
        mNoRetryErrorMessage.announceForAccessibility(message);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            mDelegate.onUserInput(mCardUnmaskInput.getText().toString(),
                    mMonthInput.getText().toString(),
                    Integer.toString(AutofillUiUtils.getFourDigitYear(mYearInput)),
                    mUseScreenlockCheckbox.isChecked());
        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        mDelegate.dismissed();
        mDialogModel = null;
    }

    @VisibleForTesting
    public static void setObserverForTest(CardUnmaskObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
    }

    @VisibleForTesting
    public PropertyModel getDialogForTest() {
        return mDialogModel;
    }

    @VisibleForTesting
    public String getErrorMessage() {
        return mErrorMessage.getText().toString();
    }
}
