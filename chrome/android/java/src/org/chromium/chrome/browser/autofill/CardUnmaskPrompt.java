// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.os.Build;
import android.os.Handler;
import android.support.annotation.IntDef;
import android.support.v4.view.MarginLayoutParamsCompat;
import android.support.v4.view.ViewCompat;
import android.text.Editable;
import android.text.InputFilter;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.modaldialog.DialogDismissalCause;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Calendar;

/**
 * A prompt that bugs users to enter their CVC when unmasking a Wallet instrument (credit card).
 */
public class CardUnmaskPrompt implements TextWatcher, OnClickListener, ModalDialogView.Controller {
    private static CardUnmaskObserverForTest sObserverForTest;

    private final CardUnmaskPromptDelegate mDelegate;
    private final ModalDialogView mDialog;
    private boolean mShouldRequestExpirationDate;

    private final View mMainView;
    private final TextView mInstructions;
    private final TextView mNoRetryErrorMessage;
    private final EditText mCardUnmaskInput;
    private final EditText mMonthInput;
    private final EditText mYearInput;
    private final View mExpirationContainer;
    private final TextView mNewCardLink;
    private final TextView mErrorMessage;
    private final CheckBox mStoreLocallyCheckbox;
    private final ImageView mStoreLocallyTooltipIcon;
    private PopupWindow mStoreLocallyTooltipPopup;
    private final ViewGroup mControlsContainer;
    private final View mVerificationOverlay;
    private final ProgressBar mVerificationProgressBar;
    private final TextView mVerificationView;
    private final long mSuccessMessageDurationMilliseconds;

    private int mThisYear;
    private int mThisMonth;
    private boolean mValidationWaitsForCalendarTask;
    private ModalDialogManager mModalDialogManager;
    private Context mContext;

    private String mCvcErrorMessage;
    private String mExpirationMonthErrorMessage;
    private String mExpirationYearErrorMessage;
    private String mExpirationDateErrorMessage;
    private String mCvcAndExpirationErrorMessage;

    private boolean mDidFocusOnMonth;
    private boolean mDidFocusOnYear;
    private boolean mDidFocusOnCvc;

    private static final int EXPIRATION_FIELDS_LENGTH = 2;

    @IntDef({ErrorType.EXPIRATION_MONTH, ErrorType.EXPIRATION_YEAR, ErrorType.EXPIRATION_DATE,
            ErrorType.CVC, ErrorType.CVC_AND_EXPIRATION, ErrorType.NOT_ENOUGH_INFO, ErrorType.NONE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ErrorType {
        int EXPIRATION_MONTH = 1;
        int EXPIRATION_YEAR = 2;
        int EXPIRATION_DATE = 3;
        int CVC = 4;
        int CVC_AND_EXPIRATION = 5;
        int NOT_ENOUGH_INFO = 6;
        int NONE = 7;
    }

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
         * @param shouldStoreLocally The state of the "Save locally?" checkbox at the time.
         */
        void onUserInput(String cvc, String month, String year, boolean shouldStoreLocally);

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
    }

    public CardUnmaskPrompt(Context context, CardUnmaskPromptDelegate delegate, String title,
            String instructions, String confirmButtonLabel, int drawableId,
            boolean shouldRequestExpirationDate, boolean canStoreLocally,
            boolean defaultToStoringLocally, long successMessageDurationMilliseconds) {
        mDelegate = delegate;

        LayoutInflater inflater = LayoutInflater.from(context);
        View v = inflater.inflate(R.layout.autofill_card_unmask_prompt, null);
        mInstructions = (TextView) v.findViewById(R.id.instructions);
        mInstructions.setText(instructions);

        mMainView = v;
        mNoRetryErrorMessage = (TextView) v.findViewById(R.id.no_retry_error_message);
        mCardUnmaskInput = (EditText) v.findViewById(R.id.card_unmask_input);
        mMonthInput = (EditText) v.findViewById(R.id.expiration_month);
        mYearInput = (EditText) v.findViewById(R.id.expiration_year);
        mExpirationContainer = v.findViewById(R.id.expiration_container);
        mNewCardLink = (TextView) v.findViewById(R.id.new_card_link);
        mNewCardLink.setOnClickListener(this);
        mErrorMessage = (TextView) v.findViewById(R.id.error_message);
        mStoreLocallyCheckbox = (CheckBox) v.findViewById(R.id.store_locally_checkbox);
        mStoreLocallyCheckbox.setChecked(canStoreLocally && defaultToStoringLocally);
        mStoreLocallyTooltipIcon = (ImageView) v.findViewById(R.id.store_locally_tooltip_icon);
        mStoreLocallyTooltipIcon.setOnClickListener(this);
        if (!canStoreLocally) v.findViewById(R.id.store_locally_container).setVisibility(View.GONE);
        mControlsContainer = (ViewGroup) v.findViewById(R.id.controls_container);
        mVerificationOverlay = v.findViewById(R.id.verification_overlay);
        mVerificationProgressBar = (ProgressBar) v.findViewById(R.id.verification_progress_bar);
        mVerificationView = (TextView) v.findViewById(R.id.verification_message);
        mSuccessMessageDurationMilliseconds = successMessageDurationMilliseconds;
        ((ImageView) v.findViewById(R.id.cvc_hint_image)).setImageResource(drawableId);

        ModalDialogView.Params params = new ModalDialogView.Params();
        params.title = title;
        params.customView = v;
        params.negativeButtonTextId = R.string.cancel;
        params.positiveButtonText = confirmButtonLabel;
        params.cancelOnTouchOutside = false;
        mDialog = new ModalDialogView(this, params);
        mShouldRequestExpirationDate = shouldRequestExpirationDate;
        mThisYear = -1;
        mThisMonth = -1;
        if (mShouldRequestExpirationDate)
            new CalendarTask().executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        // Set the max length of the CVC field.
        mCardUnmaskInput.setFilters(
                new InputFilter[] {new InputFilter.LengthFilter(mDelegate.getExpectedCvcLength())});

        // Hitting the "submit" button on the software keyboard should submit the form if valid.
        mCardUnmaskInput.setOnEditorActionListener((v14, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_DONE) {
                Button positiveButton = mDialog.getButton(ModalDialogView.ButtonType.POSITIVE);
                if (positiveButton.isEnabled()) positiveButton.performClick();
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

        // Load the error messages to show to the user.
        Resources resources = context.getResources();
        mCvcErrorMessage =
                resources.getString(R.string.autofill_card_unmask_prompt_error_try_again_cvc);
        mExpirationMonthErrorMessage = resources.getString(
                R.string.autofill_card_unmask_prompt_error_try_again_expiration_month);
        mExpirationYearErrorMessage = resources.getString(
                R.string.autofill_card_unmask_prompt_error_try_again_expiration_year);
        mExpirationDateErrorMessage = resources.getString(
                R.string.autofill_card_unmask_prompt_error_try_again_expiration_date);
        mCvcAndExpirationErrorMessage = resources.getString(
                R.string.autofill_card_unmask_prompt_error_try_again_cvc_and_expiration);
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
     * Show the dialog. If activity is null this method will not do anything.
     */
    public void show(ChromeActivity activity) {
        if (activity == null) return;

        mContext = activity;
        mModalDialogManager = activity.getModalDialogManager();

        mModalDialogManager.showDialog(mDialog, ModalDialogManager.ModalDialogType.APP);

        showExpirationDateInputsInputs();

        // Override the View.OnClickListener so that pressing the positive button doesn't dismiss
        // the dialog.
        Button verifyButton = mDialog.getButton(ModalDialogView.ButtonType.POSITIVE);
        verifyButton.setEnabled(false);
        mCardUnmaskInput.addTextChangedListener(this);
        mCardUnmaskInput.post(() -> setInitialFocus());
    }

    public void update(String title, String instructions, boolean shouldRequestExpirationDate) {
        mDialog.setTitle(title);
        mInstructions.setText(instructions);
        mShouldRequestExpirationDate = shouldRequestExpirationDate;
        if (mShouldRequestExpirationDate && (mThisYear == -1 || mThisMonth == -1)) {
            new CalendarTask().executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
        showExpirationDateInputsInputs();
    }

    public void dismiss() {
        mModalDialogManager.dismissDialog(mDialog);
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
                showErrorMessage(errorMessage);
                setInputsEnabled(true);
                setInitialFocus();

                if (!mShouldRequestExpirationDate) mNewCardLink.setVisibility(View.VISIBLE);
            } else {
                clearInputError();
                setNoRetryError(errorMessage);
            }
        } else {
            Runnable dismissRunnable = () -> dismiss();
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
        Button positiveButton = mDialog.getButton(ModalDialogView.ButtonType.POSITIVE);

        @ErrorType int errorType = getExpirationAndCvcErrorType();
        positiveButton.setEnabled(errorType == ErrorType.NONE);
        showDetailedErrorMessage(errorType);
        moveFocus(errorType);

        if (sObserverForTest != null) {
            sObserverForTest.onCardUnmaskPromptValidationDone(this);

            if (positiveButton.isEnabled()) {
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
        if (v == mStoreLocallyTooltipIcon) {
            onTooltipIconClicked();
        } else {
            assert v == mNewCardLink;
            onNewCardLinkClicked();
        }
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

    private void onTooltipIconClicked() {
        // Don't show the popup if there's already one showing (or one has been dismissed
        // recently). This prevents a tap on the (?) from hiding and then immediately re-showing
        // the popup.
        if (mStoreLocallyTooltipPopup != null) return;

        mStoreLocallyTooltipPopup = new PopupWindow(mContext);
        TextView text = new TextView(mContext);
        text.setText(R.string.autofill_card_unmask_prompt_storage_tooltip);
        // Width is the dialog's width less the margins and padding around the checkbox and
        // icon.
        text.setWidth(mMainView.getWidth() - ViewCompat.getPaddingStart(mStoreLocallyCheckbox)
                - ViewCompat.getPaddingEnd(mStoreLocallyTooltipIcon)
                - MarginLayoutParamsCompat.getMarginStart((RelativeLayout.LayoutParams)
                        mStoreLocallyCheckbox.getLayoutParams())
                - MarginLayoutParamsCompat.getMarginEnd((RelativeLayout.LayoutParams)
                        mStoreLocallyTooltipIcon.getLayoutParams()));
        text.setTextColor(Color.WHITE);
        Resources resources = mContext.getResources();
        int hPadding = resources.getDimensionPixelSize(
                R.dimen.autofill_card_unmask_tooltip_horizontal_padding);
        int vPadding = resources.getDimensionPixelSize(
                R.dimen.autofill_card_unmask_tooltip_vertical_padding);
        text.setPadding(hPadding, vPadding, hPadding, vPadding);

        mStoreLocallyTooltipPopup.setContentView(text);
        mStoreLocallyTooltipPopup.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        mStoreLocallyTooltipPopup.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        mStoreLocallyTooltipPopup.setOutsideTouchable(true);
        mStoreLocallyTooltipPopup.setBackgroundDrawable(ApiCompatibilityUtils.getDrawable(
                resources, R.drawable.store_locally_tooltip_background));
        mStoreLocallyTooltipPopup.setOnDismissListener(() -> {
            Handler h = new Handler();
            h.postDelayed(() -> mStoreLocallyTooltipPopup = null, 200);
        });
        mStoreLocallyTooltipPopup.showAsDropDown(mStoreLocallyCheckbox,
                ViewCompat.getPaddingStart(mStoreLocallyCheckbox), 0);
        text.announceForAccessibility(text.getText());
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
                    && mMonthInput.getText().length() == EXPIRATION_FIELDS_LENGTH) {
                // The user just finished typing in the month field and there are no validation
                // errors.
                if (mYearInput.getText().length() == EXPIRATION_FIELDS_LENGTH) {
                    // Year was already filled, move focus to CVC field.
                    mCardUnmaskInput.requestFocus();
                    mDidFocusOnCvc = true;
                } else {
                    // Year was not filled, move focus there.
                    mYearInput.requestFocus();
                    mDidFocusOnYear = true;
                }
            } else if (mYearInput.isFocused()
                    && mYearInput.getText().length() == EXPIRATION_FIELDS_LENGTH) {
                // The user just finished typing in the year field and there are no validation
                // errors. Move focus to CVC field.
                mCardUnmaskInput.requestFocus();
                mDidFocusOnCvc = true;
            }
        }
    }

    /**
     * Shows (or removes) the appropriate error message and apply the error filter to the
     * appropriate fields depending on the error type.
     *
     * @param errorType The type of error detected.
     */
    private void showDetailedErrorMessage(@ErrorType int errorType) {
        switch (errorType) {
            case ErrorType.EXPIRATION_MONTH:
                showErrorMessage(mExpirationMonthErrorMessage);
                break;
            case ErrorType.EXPIRATION_YEAR:
                showErrorMessage(mExpirationYearErrorMessage);
                break;
            case ErrorType.EXPIRATION_DATE:
                showErrorMessage(mExpirationDateErrorMessage);
                break;
            case ErrorType.CVC:
                showErrorMessage(mCvcErrorMessage);
                break;
            case ErrorType.CVC_AND_EXPIRATION:
                showErrorMessage(mCvcAndExpirationErrorMessage);
                break;
            case ErrorType.NONE:
            case ErrorType.NOT_ENOUGH_INFO:
            default:
                clearInputError();
                return;
        }

        updateColorForInputs(errorType);
    }

    /**
     * Applies the error filter to the invalid fields based on the errorType.
     *
     * @param errorType The ErrorType value representing the type of error found for the unmask
     *                  fields.
     */
    private void updateColorForInputs(@ErrorType int errorType) {
        // The rest of this code makes L-specific assumptions about the background being used to
        // draw the TextInput.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        ColorFilter filter =
                new PorterDuffColorFilter(ApiCompatibilityUtils.getColor(mContext.getResources(),
                                                  R.color.input_underline_error_color),
                        PorterDuff.Mode.SRC_IN);

        // Decide on what field(s) to apply the filter.
        boolean filterMonth = errorType == ErrorType.EXPIRATION_MONTH
                || errorType == ErrorType.EXPIRATION_DATE
                || errorType == ErrorType.CVC_AND_EXPIRATION;
        boolean filterYear = errorType == ErrorType.EXPIRATION_YEAR
                || errorType == ErrorType.EXPIRATION_DATE
                || errorType == ErrorType.CVC_AND_EXPIRATION;
        boolean filterCvc = errorType == ErrorType.CVC || errorType == ErrorType.CVC_AND_EXPIRATION;

        updateColorForInput(mMonthInput, filterMonth ? filter : null);
        updateColorForInput(mYearInput, filterYear ? filter : null);
        updateColorForInput(mCardUnmaskInput, filterCvc ? filter : null);
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

        if (mShouldRequestExpirationDate) errorType = getExpirationDateErrorType();

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
     * Determines what type of error, if any, is present in the expiration date fields of the
     * prompt.
     *
     * @return The ErrorType value representing the type of error found for the expiration date
     *         unmask fields.
     */
    @ErrorType private int getExpirationDateErrorType() {
        if (mThisYear == -1 || mThisMonth == -1) {
            mValidationWaitsForCalendarTask = true;
            return ErrorType.NOT_ENOUGH_INFO;
        }

        int month = getMonth();
        if (month < 1 || month > 12) {
            if (mMonthInput.getText().length() == EXPIRATION_FIELDS_LENGTH
                    || (!mMonthInput.isFocused() && mDidFocusOnMonth)) {
                // mFinishedTypingMonth = true;
                return ErrorType.EXPIRATION_MONTH;
            }
            return ErrorType.NOT_ENOUGH_INFO;
        }

        int year = getFourDigitYear();
        if (year < mThisYear || year > mThisYear + 10) {
            if (mYearInput.getText().length() == EXPIRATION_FIELDS_LENGTH
                    || (!mYearInput.isFocused() && mDidFocusOnYear)) {
                // mFinishedTypingYear = true;
                return ErrorType.EXPIRATION_YEAR;
            }
            return ErrorType.NOT_ENOUGH_INFO;
        }

        if (year == mThisYear && month < mThisMonth) {
            return ErrorType.EXPIRATION_DATE;
        }

        return ErrorType.NONE;
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
        mStoreLocallyCheckbox.setEnabled(enabled);
        mDialog.getButton(ModalDialogView.ButtonType.POSITIVE).setEnabled(enabled);
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
     * Sets the error message on the inputs.
     * @param message The error message to show.
     */
    private void showErrorMessage(String message) {
        assert message != null;

        // Set the message to display;
        mErrorMessage.setText(message);
        mErrorMessage.setVisibility(View.VISIBLE);

        // A null message is passed in during card verification, which also makes an announcement.
        // Announcing twice in a row may cancel the first announcement.
        mErrorMessage.announceForAccessibility(message);
    }

    /**
     * Removes the error message on the inputs.
     */
    private void clearInputError() {
        mErrorMessage.setText(null);
        mErrorMessage.setVisibility(View.GONE);

        // The rest of this code makes L-specific assumptions about the background being used to
        // draw the TextInput.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        // Remove the highlight on the input fields.
        updateColorForInput(mMonthInput, null);
        updateColorForInput(mYearInput, null);
        updateColorForInput(mCardUnmaskInput, null);
    }

    /**
     * Displays an error that indicates the user can't retry.
     */
    private void setNoRetryError(String message) {
        mNoRetryErrorMessage.setText(message);
        mNoRetryErrorMessage.setVisibility(View.VISIBLE);
        mNoRetryErrorMessage.announceForAccessibility(message);
    }

    /**
     * Sets the stroke color for the given input.
     * @param input The input to modify.
     * @param filter The color filter to apply to the background.
     */
    private void updateColorForInput(EditText input, ColorFilter filter) {
        input.getBackground().mutate().setColorFilter(filter);
    }

    /**
     * @return The expiration year the user entered.
     *         Two digit values (such as 17) will be converted to 4 digit years (such as 2017).
     *         Returns -1 if the input is empty or otherwise not a valid year.
     */
    private int getFourDigitYear() {
        try {
            int year = Integer.parseInt(mYearInput.getText().toString());
            if (year < 0) return -1;
            if (year < 100) year += mThisYear - mThisYear % 100;
            return year;
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    /**
     * @return The expiration month the user entered.
     *         Returns -1 if the input is empty or not a number.
     */
    private int getMonth() {
        try {
            return Integer.parseInt(mMonthInput.getText().toString());
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    @Override
    public void onClick(@ModalDialogView.ButtonType int buttonType) {
        if (buttonType == ModalDialogView.ButtonType.POSITIVE) {
            mDelegate.onUserInput(mCardUnmaskInput.getText().toString(),
                    mMonthInput.getText().toString(), Integer.toString(getFourDigitYear()),
                    mStoreLocallyCheckbox != null && mStoreLocallyCheckbox.isChecked());
        } else if (buttonType == ModalDialogView.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(
                    mDialog, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(@DialogDismissalCause int dismissalCause) {
        mDelegate.dismissed();
    }

    @VisibleForTesting
    public static void setObserverForTest(CardUnmaskObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
    }

    @VisibleForTesting
    public ModalDialogView getDialogForTest() {
        return mDialog;
    }

    @VisibleForTesting
    public String getErrorMessage() {
        return mErrorMessage.getText().toString();
    }
}
