// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.support.v7.app.AlertDialog;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.ui.base.WindowAndroid;

/**
 * The auto sign-in first run experience dialog is shown instead of usual auto sign-in snackbar
 * when the user first encounters the auto sign-in feature.
 */
public class AutoSigninFirstRunDialog
        implements DialogInterface.OnClickListener, DialogInterface.OnDismissListener {
    private final Context mContext;
    private final String mTitle;
    private final String mExplanation;
    private final int mExplanationLinkStart;
    private final int mExplanationLinkEnd;
    private final String mOkButtonText;
    private final String mTurnOffButtonText;
    private long mNativeAutoSigninFirstRunDialog;
    private AlertDialog mDialog;
    private boolean mWasDismissedByNative;

    private AutoSigninFirstRunDialog(Context context, long nativeAutoSigninFirstRunDialog,
            String title, String explanation, int explanationLinkStart, int explanationLinkEnd,
            String okButtonText, String turnOffButtonText) {
        mNativeAutoSigninFirstRunDialog = nativeAutoSigninFirstRunDialog;
        mContext = context;
        mTitle = title;
        mExplanation = explanation;
        mExplanationLinkStart = explanationLinkStart;
        mExplanationLinkEnd = explanationLinkEnd;
        mOkButtonText = okButtonText;
        mTurnOffButtonText = turnOffButtonText;
    }

    @CalledByNative
    private static AutoSigninFirstRunDialog createAndShowDialog(WindowAndroid windowAndroid,
            long nativeAutoSigninFirstRunDialog, String title, String explanation,
            int explanationLinkStart, int explanationLinkEnd, String okButtonText,
            String turnOffButtonText) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return null;

        AutoSigninFirstRunDialog dialog = new AutoSigninFirstRunDialog(activity,
                nativeAutoSigninFirstRunDialog, title, explanation, explanationLinkStart,
                explanationLinkEnd, okButtonText, turnOffButtonText);
        dialog.show();
        return dialog;
    }

    private void show() {
        final AlertDialog.Builder builder =
                new AlertDialog.Builder(mContext, R.style.Theme_Chromium_AlertDialog)
                        .setTitle(mTitle)
                        .setPositiveButton(mOkButtonText, this)
                        .setNegativeButton(mTurnOffButtonText, this);
        View view = LayoutInflater.from(mContext).inflate(
                R.layout.auto_sign_in_first_run_dialog, null);
        TextView summaryView = (TextView) view.findViewById(R.id.summary);

        if (mExplanationLinkStart != mExplanationLinkEnd && mExplanationLinkEnd != 0) {
            SpannableString spanableExplanation = new SpannableString(mExplanation);
            spanableExplanation.setSpan(new ClickableSpan() {
                @Override
                public void onClick(View view) {
                    AutoSigninFirstRunDialogJni.get().onLinkClicked(
                            mNativeAutoSigninFirstRunDialog, AutoSigninFirstRunDialog.this);
                    mDialog.dismiss();
                }
            }, mExplanationLinkStart, mExplanationLinkEnd, Spanned.SPAN_INCLUSIVE_INCLUSIVE);
            summaryView.setText(spanableExplanation);
            summaryView.setMovementMethod(LinkMovementMethod.getInstance());
        } else {
            summaryView.setText(mExplanation);
            summaryView.setMovementMethod(LinkMovementMethod.getInstance());
        }
        builder.setView(view);

        mDialog = builder.create();
        mDialog.setCanceledOnTouchOutside(false);
        mDialog.setOnDismissListener(this);
        mDialog.show();
    }

    @Override
    public void onClick(DialogInterface dialog, int whichButton) {
        if (whichButton == DialogInterface.BUTTON_NEGATIVE) {
            AutoSigninFirstRunDialogJni.get().onTurnOffClicked(
                    mNativeAutoSigninFirstRunDialog, AutoSigninFirstRunDialog.this);
        } else if (whichButton == DialogInterface.BUTTON_POSITIVE) {
            AutoSigninFirstRunDialogJni.get().onOkClicked(
                    mNativeAutoSigninFirstRunDialog, AutoSigninFirstRunDialog.this);
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        destroy();
    }

    private void destroy() {
        assert mNativeAutoSigninFirstRunDialog != 0;
        AutoSigninFirstRunDialogJni.get().destroy(
                mNativeAutoSigninFirstRunDialog, AutoSigninFirstRunDialog.this);
        mNativeAutoSigninFirstRunDialog = 0;
        mDialog = null;
    }

    @CalledByNative
    private void dismissDialog() {
        assert !mWasDismissedByNative;
        mWasDismissedByNative = true;
        mDialog.dismiss();
    }

    @NativeMethods
    interface Natives {
        void onTurnOffClicked(
                long nativeAutoSigninFirstRunDialogAndroid, AutoSigninFirstRunDialog caller);
        void onOkClicked(
                long nativeAutoSigninFirstRunDialogAndroid, AutoSigninFirstRunDialog caller);
        void destroy(long nativeAutoSigninFirstRunDialogAndroid, AutoSigninFirstRunDialog caller);
        void onLinkClicked(
                long nativeAutoSigninFirstRunDialogAndroid, AutoSigninFirstRunDialog caller);
    }
}
