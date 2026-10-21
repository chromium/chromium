// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.ui.base.WindowAndroid;

/**
 * The auto sign-in first run experience dialog is shown instead of usual auto sign-in snackbar when
 * the user first encounters the auto sign-in feature.
 */
@NullMarked
public class AutoSigninFirstRunDialog
        implements DialogInterface.OnClickListener, DialogInterface.OnDismissListener {
    @JniType("::AutoSigninFirstRunDialogAndroid")
    interface NativeAutoSigninFirstRunDialogAndroid extends JniTypeToken {}

    private final Context mContext;
    private final String mTitle;
    private final String mExplanation;
    private final String mOkButtonText;
    private final String mTurnOffButtonText;
    private @Nullable JniRawPtr<NativeAutoSigninFirstRunDialogAndroid>
            mNativeAutoSigninFirstRunDialog;
    private @Nullable AlertDialog mDialog;
    private boolean mWasDismissedByNative;

    private AutoSigninFirstRunDialog(
            Context context,
            JniRawPtr<NativeAutoSigninFirstRunDialogAndroid> nativeAutoSigninFirstRunDialog,
            String title,
            String explanation,
            String okButtonText,
            String turnOffButtonText) {
        mNativeAutoSigninFirstRunDialog = nativeAutoSigninFirstRunDialog;
        mContext = context;
        mTitle = title;
        mExplanation = explanation;
        mOkButtonText = okButtonText;
        mTurnOffButtonText = turnOffButtonText;
    }

    @CalledByNative
    static @Nullable AutoSigninFirstRunDialog createAndShowDialog(
            WindowAndroid windowAndroid,
            JniRawPtr<NativeAutoSigninFirstRunDialogAndroid> nativeAutoSigninFirstRunDialog,
            @JniType("std::u16string") String title,
            @JniType("std::u16string") String explanation,
            @JniType("std::u16string") String okButtonText,
            @JniType("std::u16string") String turnOffButtonText) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            nativeAutoSigninFirstRunDialog.release();
            return null;
        }

        AutoSigninFirstRunDialog dialog =
                new AutoSigninFirstRunDialog(
                        activity,
                        nativeAutoSigninFirstRunDialog,
                        title,
                        explanation,
                        okButtonText,
                        turnOffButtonText);
        dialog.show();
        return dialog;
    }

    private void show() {
        final AlertDialog.Builder builder =
                new AlertDialog.Builder(mContext, R.style.ThemeOverlay_BrowserUI_AlertDialog)
                        .setTitle(mTitle)
                        .setPositiveButton(mOkButtonText, this)
                        .setNegativeButton(mTurnOffButtonText, this);
        View view =
                LayoutInflater.from(mContext).inflate(R.layout.auto_sign_in_first_run_dialog, null);
        TextView summaryView = view.findViewById(R.id.summary);
        summaryView.setText(mExplanation);
        summaryView.setMovementMethod(LinkMovementMethod.getInstance());
        builder.setView(view);

        mDialog = builder.create();
        mDialog.setCanceledOnTouchOutside(false);
        mDialog.setOnDismissListener(this);
        mDialog.show();
    }

    @Override
    public void onClick(DialogInterface dialog, int whichButton) {
        assert mNativeAutoSigninFirstRunDialog != null;
        if (whichButton == DialogInterface.BUTTON_NEGATIVE) {
            AutoSigninFirstRunDialogJni.get().onTurnOffClicked(mNativeAutoSigninFirstRunDialog);
        } else if (whichButton == DialogInterface.BUTTON_POSITIVE) {
            AutoSigninFirstRunDialogJni.get().onOkClicked(mNativeAutoSigninFirstRunDialog);
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        destroy();
    }

    private void destroy() {
        assert mNativeAutoSigninFirstRunDialog != null;
        AutoSigninFirstRunDialogJni.get().destroy(mNativeAutoSigninFirstRunDialog);
        mNativeAutoSigninFirstRunDialog.release();
        mNativeAutoSigninFirstRunDialog = null;
        mDialog = null;
    }

    @CalledByNative
    private void dismissDialog() {
        assert !mWasDismissedByNative;
        mWasDismissedByNative = true;
        assumeNonNull(mDialog);
        mDialog.dismiss();
    }

    @NativeMethods
    interface Natives {
        void onTurnOffClicked(JniPtr<NativeAutoSigninFirstRunDialogAndroid> self);

        void onOkClicked(JniPtr<NativeAutoSigninFirstRunDialogAndroid> self);

        void destroy(JniPtr<NativeAutoSigninFirstRunDialogAndroid> self);
    }
}
