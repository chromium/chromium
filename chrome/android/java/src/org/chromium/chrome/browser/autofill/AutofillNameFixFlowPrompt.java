// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.content.Context;
import android.text.Editable;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.TextView;
import android.widget.TextView.BufferType;

import androidx.annotation.Nullable;
import androidx.core.text.TextUtilsCompat;
import androidx.core.view.ViewCompat;

import org.chromium.chrome.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Locale;
/**
 * Prompt that asks users to confirm user's name before saving card to Google.
 */
public class AutofillNameFixFlowPrompt implements TextWatcher, ModalDialogProperties.Controller {
    /**
     * An interface to handle the interaction with
     * an AutofillNameFixFlowPrompt object.
     */
    public interface AutofillNameFixFlowPromptDelegate {
        /**
         * Called whenever the dialog is dismissed.
         */
        void onPromptDismissed();

        /**
         * Called when user accepted/confirmed the prompt.
         *
         * @param name Card holder name.
         */
        void onUserAccept(String name);

        /**
         * Called when link in legal lines is clicked.
         */
        void onLinkClicked(String url);
    }

    /**
     * Create a dialog prompt for the use of infobar. This prompt does not include legal lines.
     *
     * @param context The current context.
     * @param delegate A {@link AutofillNameFixFlowPromptDelegate} to handle events.
     * @param title Title of the prompt.
     * @param inferredName Name inferred from the account. Empty string for user to fill in.
     * @param confirmButtonLabel Label for the confirm button.
     * @param drawableId Drawable id on the title.
     * @return A {@link AutofillNameFixFlowPrompt} to confirm name.
     */
    public static AutofillNameFixFlowPrompt createAsInfobarFixFlowPrompt(Context context,
            AutofillNameFixFlowPromptDelegate delegate, String title, String inferredName,
            String confirmButtonLabel, int drawableId) {
        return new AutofillNameFixFlowPrompt(
                context, delegate, title, inferredName, confirmButtonLabel, drawableId, false);
    }

    /**
     * Create a dialog prompt for the use of message. This prompt should include legal lines.
     *
     * @param context The current context.
     * @param delegate A {@link AutofillNameFixFlowPromptDelegate} to handle events.
     * @param title Title of the prompt.
     * @param inferredName Name inferred from the account. Empty string for user to fill in.
     * @param confirmButtonLabel Label for the confirm button.
     * @return A {@link AutofillNameFixFlowPrompt} to confirm name.
     */
    public static AutofillNameFixFlowPrompt createAsMessageFixFlowPrompt(Context context,
            AutofillNameFixFlowPromptDelegate delegate, String title, String inferredName,
            String confirmButtonLabel, String cardLabel) {
        return new AutofillNameFixFlowPrompt(
                context, delegate, title, inferredName, confirmButtonLabel, cardLabel);
    }

    private final AutofillNameFixFlowPromptDelegate mDelegate;
    private final PropertyModel mDialogModel;

    private final View mDialogView;
    private final EditText mUserNameInput;
    private final ImageView mNameFixFlowTooltipIcon;
    private PopupWindow mNameFixFlowTooltipPopup;

    private ModalDialogManager mModalDialogManager;
    private Context mContext;

    /**
     * Fix flow prompt to confirm user name before saving the card to Google.
     */
    private AutofillNameFixFlowPrompt(Context context, AutofillNameFixFlowPromptDelegate delegate,
            String title, String inferredName, String confirmButtonLabel, int drawableId,
            boolean filledConfirmButton) {
        mDelegate = delegate;
        LayoutInflater inflater = LayoutInflater.from(context);
        mDialogView = inflater.inflate(R.layout.autofill_name_fixflow, null);

        mUserNameInput = (EditText) mDialogView.findViewById(R.id.cc_name_edit);
        mUserNameInput.setText(inferredName, BufferType.EDITABLE);
        mNameFixFlowTooltipIcon = (ImageView) mDialogView.findViewById(R.id.cc_name_tooltip_icon);

        // Do not show tooltip if inferred name is empty.
        if (TextUtils.isEmpty(inferredName)) {
            mNameFixFlowTooltipIcon.setVisibility(View.GONE);
        } else {
            mNameFixFlowTooltipIcon.setOnClickListener((view) -> onTooltipIconClicked());
        }

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.TITLE, title)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, confirmButtonLabel)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, context.getResources(),
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                                inferredName.isEmpty())
                        .with(ModalDialogProperties.PRIMARY_BUTTON_FILLED, filledConfirmButton);
        if (drawableId != 0) {
            builder.with(ModalDialogProperties.TITLE_ICON, context, drawableId);
        }
        mDialogModel = builder.build();

        // Hitting the "submit" button on the software keyboard should submit, unless the name field
        // is empty.
        mUserNameInput.setOnEditorActionListener((view, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_DONE) {
                if (mUserNameInput.getText().toString().trim().length() != 0) {
                    onClick(mDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
                }
                return true;
            }
            return false;
        });
    }

    private AutofillNameFixFlowPrompt(Context context, AutofillNameFixFlowPromptDelegate delegate,
            String title, String inferredName, String confirmButtonLabel, String cardLabel) {
        this(context, delegate, title, inferredName, confirmButtonLabel, /*drawableId=*/0, true);
        mDialogView.findViewById(R.id.cc_details).setVisibility(View.VISIBLE);
        TextView detailsMasked = mDialogView.findViewById(R.id.cc_details_masked);
        detailsMasked.setText(cardLabel);
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
        mUserNameInput.addTextChangedListener(this);
    }

    public void setLegalMessageLine(LegalMessageLine line) {
        SpannableString text = new SpannableString(line.text);
        for (final LegalMessageLine.Link link : line.links) {
            String url = link.url;
            text.setSpan(new ClickableSpan() {
                @Override
                public void onClick(View view) {
                    mDelegate.onLinkClicked(url);
                }
            }, link.start, link.end, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        }
        TextView legalMessage = mDialogView.findViewById(R.id.legal_message);
        legalMessage.setText(text);
        legalMessage.setMovementMethod(LinkMovementMethod.getInstance());
        legalMessage.setVisibility(View.VISIBLE);
    }

    protected void dismiss(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mDialogModel, dismissalCause);
    }

    @Override
    public void afterTextChanged(Editable s) {
        mDialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                mUserNameInput.getText().toString().trim().isEmpty());
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {}

    /**
     * Handle tooltip icon clicked. If tooltip is already opened, don't show another. Otherwise
     * create a new one.
     */
    private void onTooltipIconClicked() {
        if (mNameFixFlowTooltipPopup != null) return;

        mNameFixFlowTooltipPopup = new PopupWindow(mContext);
        Runnable dismissAction = () -> {
            mNameFixFlowTooltipPopup = null;
        };
        boolean isLeftToRight = TextUtilsCompat.getLayoutDirectionFromLocale(Locale.getDefault())
                == ViewCompat.LAYOUT_DIRECTION_LTR;
        AutofillUiUtils.showTooltip(mContext, mNameFixFlowTooltipPopup,
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
                isLeftToRight ? mUserNameInput : mNameFixFlowTooltipIcon, dismissAction);
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            mDelegate.onUserAccept(mUserNameInput.getText().toString());
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
