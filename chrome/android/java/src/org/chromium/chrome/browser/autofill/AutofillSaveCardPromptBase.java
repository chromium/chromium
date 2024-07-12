// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Base class for creating autofill save card prompts that support displaying legal message line.
 */
public abstract class AutofillSaveCardPromptBase implements ModalDialogProperties.Controller {
    private final AutofillSaveCardPromptBaseDelegate mBaseDelegate;

    protected PropertyModel mDialogModel;
    protected ModalDialogManager mModalDialogManager;
    protected Context mContext;
    protected View mDialogView;
    private SpannableStringBuilder mSpannableStringBuilder;

    interface AutofillSaveCardPromptBaseDelegate {
        /** Called when a link is clicked. */
        void onLinkClicked(String url);

        /** Called whenever the dialog is dismissed. */
        void onPromptDismissed();

        /**
         * Called when the dialog is dismissed neither because the user accepted/confirmed the
         * prompt or it was dismissed by native code.
         */
        void onUserDismiss();
    }

    /**
     * @param context The {@link Context} to inflate layout xml.
     * @param delegate A {@link AutofillSaveCardPromptBaseDelegate} to handle events.
     * @param contentLayoutId The content of the prompt dialog. Set 0 to make content empty.
     * @param customTitleLayoutId Layout id for a custom title and icon view.
     * @param title Title of the prompt dialog.
     * @param titleIcon Icon near the title. Set 0 to ignore this icon.
     * @param confirmButtonLabel The text of confirm button.
     * @param filledConfirmButton Whether to use a button of filled style.
     */
    protected AutofillSaveCardPromptBase(
            Context context,
            AutofillSaveCardPromptBaseDelegate delegate,
            @LayoutRes int contentLayoutId,
            @LayoutRes int customTitleLayoutId,
            String title,
            @DrawableRes int titleIcon,
            String confirmButtonLabel,
            boolean filledConfirmButton) {
        mBaseDelegate = delegate;
        LayoutInflater inflater = LayoutInflater.from(context);
        mDialogView = inflater.inflate(R.layout.autofill_save_card_base_layout, null);
        boolean useCustomTitleView =
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList
                                        .AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK)
                        && customTitleLayoutId != Resources.ID_NULL;

        if (useCustomTitleView) {
            ViewStub stub = mDialogView.findViewById(R.id.title_with_icon_stub);
            stub.setLayoutResource(customTitleLayoutId);
            stub.inflate();
        }

        if (contentLayoutId != 0) {
            ViewStub stub = mDialogView.findViewById(R.id.autofill_save_card_content_stub);
            stub.setLayoutResource(contentLayoutId);
            stub.inflate();
        }

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mDialogView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, confirmButtonLabel)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                context.getResources(),
                                R.string.cancel)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                filledConfirmButton
                                        ? ModalDialogProperties.ButtonStyles
                                                .PRIMARY_FILLED_NEGATIVE_OUTLINE
                                        : ModalDialogProperties.ButtonStyles
                                                .PRIMARY_OUTLINE_NEGATIVE_OUTLINE);

        updateTitleView(useCustomTitleView, title, titleIcon, builder, context);
        mDialogModel = builder.build();
        mContext = context;
    }

    /**
     * Show the dialog.
     *
     * @param activity The current activity, used for context. When null, the method does nothing.
     * @param modalDialogManager Used to display modal dialogs. When null, the method does nothing.
     */
    public final void show(
            @Nullable Activity activity, @Nullable ModalDialogManager modalDialogManager) {
        if (activity == null || modalDialogManager == null) return;

        if (mSpannableStringBuilder != null) {
            TextView legalMessage = mDialogView.findViewById(R.id.legal_message);
            legalMessage.setText(mSpannableStringBuilder);
            legalMessage.setVisibility(View.VISIBLE);
            legalMessage.setMovementMethod(LinkMovementMethod.getInstance());
        }

        mContext = activity;
        mModalDialogManager = modalDialogManager;
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Updates the title and icon view. If AUTOFILL_ENABLE_MOVING_GPAY_LOGO_TO_THE_RIGHT_ON_CLANK
     * feature is enabled, sets title and icon in the customView otherwise uses
     * PropertyModel.Builder for title and icon.
     *
     * @param useCustomTitleView Indicates true/false to use custom title view.
     * @param title Title of the prompt dialog.
     * @param titleIcon Icon near the title.
     * @param builder The PropertyModel.Builder instance.
     * @param context The {@link Context} to inflate layout xml.
     */
    private void updateTitleView(
            boolean useCustomTitleView,
            String title,
            @DrawableRes int titleIcon,
            PropertyModel.Builder builder,
            Context context) {
        if (useCustomTitleView) {
            TextView titleView = mDialogView.findViewById(R.id.title);
            titleView.setText(title);

            if (titleIcon != Resources.ID_NULL) {
                ImageView iconView = mDialogView.findViewById(R.id.title_icon);
                iconView.setImageResource(titleIcon);
            } else {
                mDialogView.findViewById(R.id.title_icon).setVisibility(View.GONE);
            }
        } else {
            builder.with(ModalDialogProperties.TITLE, title);
            if (titleIcon != Resources.ID_NULL) {
                builder.with(ModalDialogProperties.TITLE_ICON, context, titleIcon);
            }
        }
    }

    public void addLegalMessageLine(LegalMessageLine line) {
        if (mSpannableStringBuilder == null) {
            mSpannableStringBuilder = new SpannableStringBuilder();
        } else {
            // If this isn't the first line, append a new line before the legal message.
            mSpannableStringBuilder.append("\n\n");
        }
        int offset = mSpannableStringBuilder.length();
        mSpannableStringBuilder.append(line.text);
        for (final LegalMessageLine.Link link : line.links) {
            String url = link.url;
            mSpannableStringBuilder.setSpan(
                    new ClickableSpan() {
                        @Override
                        public void onClick(View view) {
                            mBaseDelegate.onLinkClicked(url);
                        }
                    },
                    link.start + offset,
                    link.end + offset,
                    Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        }
    }

    public void dismiss(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mDialogModel, dismissalCause);
    }
}
