// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;

import java.lang.ref.WeakReference;

/**
 * Helps to show a confirmation.
 */
public class ConfirmationDialogHelper implements DialogInterface.OnClickListener {
    private final WeakReference<Context> mContext;
    private AlertDialog mConfirmationDialog;
    private Runnable mConfirmedCallback;

    public ConfirmationDialogHelper(WeakReference<Context> context) {
        mContext = context;
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        assert which == DialogInterface.BUTTON_POSITIVE;
        mConfirmedCallback.run();
    }

    /**
     * Hides the dialog.
     */
    public void dismiss() {
        if (mConfirmationDialog != null) mConfirmationDialog.dismiss();
    }

    /**
     * Returns the resources associated with the context used to launch the dialog.
     */
    public Resources getResources() {
        if (mContext.get() == null) return null;
        return mContext.get().getResources();
    }

    /**
     * Shows an dialog to confirm the deletion.
     * @param title A {@link String} used as title.
     * @param message A {@link String} used message body.
     */
    public void showConfirmation(
            String title, String message, int confirmButtonTextId, Runnable confirmedCallback) {
        assert title != null;
        assert message != null;
        assert confirmedCallback != null;

        Context context = mContext.get();
        if (context == null) return;
        mConfirmedCallback = confirmedCallback;
        AlertDialog.Builder builder =
                new AlertDialog.Builder(context, R.style.ThemeOverlay_BrowserUI_AlertDialog);
        LayoutInflater inflater = LayoutInflater.from(builder.getContext());
        View body = inflater.inflate(R.layout.confirmation_dialog_view, null);
        TextView titleView = body.findViewById(R.id.confirmation_dialog_title);
        titleView.setText(title);
        TextView messageView = body.findViewById(R.id.confirmation_dialog_message);
        messageView.setText(message);
        mConfirmationDialog = builder.setView(body)
                                      .setNegativeButton(R.string.cancel, null)
                                      .setPositiveButton(confirmButtonTextId, this)
                                      .create();
        mConfirmationDialog.show();
    }
}