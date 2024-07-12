// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;
import android.content.res.Resources;
import android.text.Editable;
import android.text.TextUtils;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.TextView;
import android.widget.TextView.BufferType;

import androidx.core.text.TextUtilsCompat;
import androidx.core.view.ViewCompat;

import org.chromium.chrome.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;

import java.util.Locale;

/** Prompt that asks users to confirm user's name before saving card to Google. */
public class AutofillNameFixFlowPrompt extends AutofillSaveCardPromptBase
        implements EmptyTextWatcher {
    /**
     * An interface to handle the interaction with
     * an AutofillNameFixFlowPrompt object.
     */
    public interface AutofillNameFixFlowPromptDelegate extends AutofillSaveCardPromptBaseDelegate {
        /**
         * Called when user accepted/confirmed the prompt.
         *
         * @param name Card holder name.
         */
        void onUserAcceptCardholderName(String name);
    }

    /**
     * Create a dialog prompt for the use of infobar. This prompt does not include legal lines.
     *
     * @param context The current context.
     * @param delegate A {@link AutofillNameFixFlowPromptDelegate} to handle events.
     * @param inferredName Name inferred from the account. Empty string for user to fill in.
     * @param title Title of the prompt.
     * @param drawableId Drawable id on the title.
     * @param confirmButtonLabel Label for the confirm button.
     * @return A {@link AutofillNameFixFlowPrompt} to confirm name.
     */
    public static AutofillNameFixFlowPrompt createAsInfobarFixFlowPrompt(
            Context context,
            AutofillNameFixFlowPromptDelegate delegate,
            String inferredName,
            String title,
            int drawableId,
            String confirmButtonLabel) {
        return new AutofillNameFixFlowPrompt(
                context, delegate, inferredName, title, drawableId, confirmButtonLabel, false);
    }

    private final AutofillNameFixFlowPromptDelegate mDelegate;

    private final EditText mUserNameInput;
    private final ImageView mNameFixFlowTooltipIcon;
    private PopupWindow mNameFixFlowTooltipPopup;

    /** Fix flow prompt to confirm user name before saving the card to Google. */
    private AutofillNameFixFlowPrompt(
            Context context,
            AutofillNameFixFlowPromptDelegate delegate,
            String inferredName,
            String title,
            int drawableId,
            String confirmButtonLabel,
            boolean filledConfirmButton) {
        super(
                context,
                delegate,
                R.layout.autofill_name_fixflow,
                /* customTitleLayoutId= */ Resources.ID_NULL,
                title,
                drawableId,
                confirmButtonLabel,
                filledConfirmButton);
        mDelegate = delegate;
        // Dialog of infobar doesn't show any details of the cc.
        mDialogView.findViewById(R.id.cc_details).setVisibility(View.GONE);
        mUserNameInput = mDialogView.findViewById(R.id.cc_name_edit);
        mUserNameInput.setText(inferredName, BufferType.EDITABLE);
        mNameFixFlowTooltipIcon = mDialogView.findViewById(R.id.cc_name_tooltip_icon);
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, inferredName.isEmpty());

        // Do not show tooltip if inferred name is empty.
        if (TextUtils.isEmpty(inferredName)) {
            mNameFixFlowTooltipIcon.setVisibility(View.GONE);
        } else {
            mNameFixFlowTooltipIcon.setOnClickListener((view) -> onTooltipIconClicked());
        }

        // Hitting the "submit" button on the software keyboard should submit, unless the name field
        // is empty.
        mUserNameInput.setOnEditorActionListener(
                (view, actionId, event) -> {
                    if (actionId == EditorInfo.IME_ACTION_DONE) {
                        if (mUserNameInput.getText().toString().trim().length() != 0) {
                            onClick(mDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
                        }
                        return true;
                    }
                    return false;
                });
        mUserNameInput.addTextChangedListener(this);
    }

    @Override
    public void afterTextChanged(Editable s) {
        mDialogModel.set(
                ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                mUserNameInput.getText().toString().trim().isEmpty());
    }

    /**
     * Handle tooltip icon clicked. If tooltip is already opened, don't show another. Otherwise
     * create a new one.
     */
    private void onTooltipIconClicked() {
        if (mNameFixFlowTooltipPopup != null) return;

        mNameFixFlowTooltipPopup = new PopupWindow(mContext);
        Runnable dismissAction =
                () -> {
                    mNameFixFlowTooltipPopup = null;
                };
        boolean isLeftToRight =
                TextUtilsCompat.getLayoutDirectionFromLocale(Locale.getDefault())
                        == ViewCompat.LAYOUT_DIRECTION_LTR;
        AutofillUiUtils.showTooltip(
                mContext,
                mNameFixFlowTooltipPopup,
                R.string.autofill_save_card_prompt_cardholder_name_tooltip,
                new AutofillUiUtils.OffsetProvider() {
                    @Override
                    public int getXOffset(TextView textView) {
                        int xOffset =
                                mNameFixFlowTooltipIcon.getLeft() - textView.getMeasuredWidth();
                        return Math.max(0, xOffset);
                    }

                    @Override
                    public int getYOffset(TextView textView) {
                        return 0;
                    }
                },
                // If the layout is right to left then anchor on the edit text field else anchor on
                // the tooltip icon, which would be on the left.
                isLeftToRight ? mUserNameInput : mNameFixFlowTooltipIcon,
                dismissAction);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            mDelegate.onUserAcceptCardholderName(mUserNameInput.getText().toString());
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
}
