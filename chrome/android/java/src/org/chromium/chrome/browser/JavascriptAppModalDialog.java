// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.DialogInterface;
import android.support.v7.app.AlertDialog;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.Log;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.jsdialog.JavascriptModalDialogView;
import org.chromium.chrome.browser.modaldialog.DialogDismissalCause;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.ui.base.WindowAndroid;

/**
 * A dialog shown via JavaScript. This can be an alert dialog, a prompt dialog, a confirm dialog,
 * or an onbeforeunload dialog.
 */
public class JavascriptAppModalDialog
        implements DialogInterface.OnClickListener, ModalDialogView.Controller {
    private static final String TAG = "JSAppModalDialog";

    private final String mTitle;
    private final String mMessage;
    private final int mPositiveButtonTextId;
    private final int mNegativeButtonTextId;
    private final boolean mShouldShowSuppressCheckBox;
    private long mNativeDialogPointer;
    private AlertDialog mDialog;
    private CheckBox mSuppressCheckBox;
    private TextView mPromptTextView;

    private ModalDialogManager mModalDialogManager;
    private JavascriptModalDialogView mDialogView;

    private JavascriptAppModalDialog(String title, String message,
            int positiveButtonTextId, int negativeButtonTextId,
            boolean shouldShowSuppressCheckBox) {
        mTitle = title;
        mMessage = message;
        mPositiveButtonTextId = positiveButtonTextId;
        mNegativeButtonTextId = negativeButtonTextId;
        mShouldShowSuppressCheckBox = shouldShowSuppressCheckBox;
    }

    @CalledByNative
    public static JavascriptAppModalDialog createAlertDialog(String title, String message,
            boolean shouldShowSuppressCheckBox) {
        return new JavascriptAppModalDialog(title, message, R.string.ok, 0,
                shouldShowSuppressCheckBox);
    }

    @CalledByNative
    public static JavascriptAppModalDialog createConfirmDialog(String title, String message,
            boolean shouldShowSuppressCheckBox) {
        return new JavascriptAppModalDialog(title, message, R.string.ok, R.string.cancel,
                shouldShowSuppressCheckBox);
    }

    @CalledByNative
    public static JavascriptAppModalDialog createBeforeUnloadDialog(String title, String message,
            boolean isReload, boolean shouldShowSuppressCheckBox) {
        return new JavascriptAppModalDialog(title, message,
                isReload ? R.string.reload : R.string.leave, R.string.cancel,
                shouldShowSuppressCheckBox);
    }

    @CalledByNative
    public static JavascriptAppModalDialog createPromptDialog(String title, String message,
            boolean shouldShowSuppressCheckBox, String defaultPromptText) {
        return new JavascriptAppPromptDialog(title, message, shouldShowSuppressCheckBox,
                defaultPromptText);
    }

    @CalledByNative
    void showJavascriptAppModalDialog(WindowAndroid window, long nativeDialogPointer) {
        assert window != null;
        Context context = window.getActivity().get();
        // If the activity has gone away, then just clean up the native pointer.
        if (context == null) {
            nativeDidCancelAppModalDialog(nativeDialogPointer, false);
            return;
        }

        // Cache the native dialog pointer so that we can use it to return the response.
        mNativeDialogPointer = nativeDialogPointer;

        if (VrModuleProvider.getDelegate().isInVr()) {
            // Use JavascriptModalDialogView while in VR.
            ChromeActivity activity = (ChromeActivity) window.getActivity().get();
            mModalDialogManager = activity.getModalDialogManager();
            // Only BeforeUnloadDialog should be created this way, so it is safe to set prompt text
            // to null. We also disabled suppress checkbox in VR. In the future, it is possible that
            // we bring it back. See https://crbug.com/830057.
            assert !(this instanceof JavascriptAppPromptDialog);
            mDialogView = JavascriptModalDialogView.create(this, mTitle, mMessage, null, false,
                    mPositiveButtonTextId, mNegativeButtonTextId);
            mModalDialogManager.showDialog(mDialogView, ModalDialogManager.ModalDialogType.TAB);
        } else {
            LayoutInflater inflater = LayoutInflater.from(context);
            ViewGroup layout = (ViewGroup) inflater.inflate(R.layout.js_modal_dialog, null);
            mSuppressCheckBox = (CheckBox) layout.findViewById(R.id.suppress_js_modal_dialogs);
            mPromptTextView = (TextView) layout.findViewById(R.id.js_modal_dialog_prompt);

            prepare(layout);

            AlertDialog.Builder builder = new AlertDialog.Builder(context, R.style.AlertDialogTheme)
                                                  .setView(layout)
                                                  .setTitle(mTitle)
                                                  .setOnCancelListener(dialog -> cancel(false));
            if (mPositiveButtonTextId != 0) builder.setPositiveButton(mPositiveButtonTextId, this);
            if (mNegativeButtonTextId != 0) builder.setNegativeButton(mNegativeButtonTextId, this);

            mDialog = builder.create();
            mDialog.setCanceledOnTouchOutside(false);
            mDialog.getDelegate().setHandleNativeActionModesEnabled(false);
            mDialog.show();
        }
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        switch (which) {
            case DialogInterface.BUTTON_POSITIVE:
                confirm(mPromptTextView.getText().toString(), mSuppressCheckBox.isChecked());
                mDialog.dismiss();
                break;
            case DialogInterface.BUTTON_NEGATIVE:
                cancel(mSuppressCheckBox.isChecked());
                mDialog.dismiss();
                break;
            default:
                Log.e(TAG, "Unexpected button pressed in dialog: " + which);
        }
    }

    @Override
    public void onClick(@ModalDialogView.ButtonType int buttonType) {
        switch (buttonType) {
            case ModalDialogView.ButtonType.POSITIVE:
                mModalDialogManager.dismissDialog(
                        mDialogView, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                break;
            case ModalDialogView.ButtonType.NEGATIVE:
                mModalDialogManager.dismissDialog(
                        mDialogView, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                break;
            default:
                Log.e(TAG, "Unexpected button pressed in dialog: " + buttonType);
        }
    }

    @Override
    public void onDismiss(@DialogDismissalCause int dismissalCause) {
        // TODO(https://crbug.com/874537): Add suppression logic in the refactor and make sure it
        // doesn't break VR.
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                confirm(mDialogView.getPromptText(), false);
                break;
            case DialogDismissalCause.DISMISSED_BY_NATIVE:
                break;
            default:
                cancel(false);
        }
        mDialogView = null;
    }

    protected void prepare(final ViewGroup layout) {
        // Display the checkbox for suppressing dialogs if necessary.
        layout.findViewById(R.id.suppress_js_modal_dialogs).setVisibility(
                mShouldShowSuppressCheckBox ? View.VISIBLE : View.GONE);

        // If the message is null or empty do not display the message text view.
        // Hide parent scroll view instead of text view in order to prevent ui discrepancies.
        if (TextUtils.isEmpty(mMessage)) {
            layout.findViewById(R.id.js_modal_dialog_scroll_view).setVisibility(View.GONE);
        } else {
            ((TextView) layout.findViewById(R.id.js_modal_dialog_message)).setText(mMessage);

            layout.findViewById(R.id.js_modal_dialog_scroll_view)
                    .addOnLayoutChangeListener(
                            (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                                boolean isScrollable =
                                        v.getMeasuredHeight() - v.getPaddingTop()
                                                - v.getPaddingBottom()
                                                < ((ViewGroup) v).getChildAt(0).getMeasuredHeight();

                                v.setFocusable(isScrollable);
                            });
        }
    }

    private void confirm(String promptResult, boolean suppressDialogs) {
        if (mNativeDialogPointer != 0) {
            nativeDidAcceptAppModalDialog(mNativeDialogPointer, promptResult, suppressDialogs);
        }
    }

    private void cancel(boolean suppressDialogs) {
        if (mNativeDialogPointer != 0) {
            nativeDidCancelAppModalDialog(mNativeDialogPointer, suppressDialogs);
        }
    }

    @CalledByNative
    private void dismiss() {
        if (mDialog != null) {
            mDialog.dismiss();
        } else {
            mModalDialogManager.dismissDialog(
                    mDialogView, DialogDismissalCause.DISMISSED_BY_NATIVE);
        }
        mNativeDialogPointer = 0;
    }

    /**
     * Returns the currently showing dialog, null if none is showing.
     */
    @VisibleForTesting
    public static JavascriptAppModalDialog getCurrentDialogForTest() {
        return nativeGetCurrentModalDialog();
    }

    /**
     * Returns the AlertDialog associated with this JavascriptAppPromptDialog.
     */
    @VisibleForTesting
    public AlertDialog getDialogForTest() {
        return mDialog;
    }

    private static class JavascriptAppPromptDialog extends JavascriptAppModalDialog {
        private final String mDefaultPromptText;

        JavascriptAppPromptDialog(String title, String message, boolean shouldShowSuppressCheckBox,
                String defaultPromptText) {
            super(title, message, R.string.ok, R.string.cancel, shouldShowSuppressCheckBox);
            mDefaultPromptText = defaultPromptText;
        }

        @Override
        protected void prepare(ViewGroup layout) {
            super.prepare(layout);
            EditText prompt = (EditText) layout.findViewById(R.id.js_modal_dialog_prompt);
            prompt.setVisibility(View.VISIBLE);

            if (mDefaultPromptText.length() > 0) {
                prompt.setText(mDefaultPromptText);
                prompt.selectAll();
            }
        }
    }

    private native void nativeDidAcceptAppModalDialog(long nativeJavascriptAppModalDialogAndroid,
            String prompt, boolean suppress);
    private native void nativeDidCancelAppModalDialog(long nativeJavascriptAppModalDialogAndroid,
            boolean suppress);
    private static native JavascriptAppModalDialog nativeGetCurrentModalDialog();
}
