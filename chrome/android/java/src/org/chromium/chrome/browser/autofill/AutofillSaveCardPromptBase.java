// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.content.Context;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

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

    interface AutofillSaveCardPromptBaseDelegate {
        /**
         * Called when link in legal lines is clicked.
         */
        void onLinkClicked(String url);

        /**
         * Called whenever the dialog is dismissed.
         */
        void onPromptDismissed();

        /**
         * Called when the dialog is dismissed neither because the user accepted/confirmed the
         * prompt or it was dismissed by native code.
         */
        void onUserDismiss();
    }

    protected AutofillSaveCardPromptBase(AutofillSaveCardPromptBaseDelegate delegate) {
        mBaseDelegate = delegate;
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
    }

    public void setLegalMessageLine(LegalMessageLine line) {
        SpannableString text = new SpannableString(line.text);
        for (final LegalMessageLine.Link link : line.links) {
            String url = link.url;
            text.setSpan(new ClickableSpan() {
                @Override
                public void onClick(View view) {
                    mBaseDelegate.onLinkClicked(url);
                }
            }, link.start, link.end, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        }
        TextView legalMessage = mDialogView.findViewById(org.chromium.chrome.R.id.legal_message);
        legalMessage.setText(text);
        legalMessage.setMovementMethod(LinkMovementMethod.getInstance());
        legalMessage.setVisibility(View.VISIBLE);
    }

    public void dismiss(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.dismissDialog(mDialogModel, dismissalCause);
    }
}
