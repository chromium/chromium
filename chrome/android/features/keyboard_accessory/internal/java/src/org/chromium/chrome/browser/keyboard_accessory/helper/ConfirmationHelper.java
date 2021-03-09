// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.helper;

import android.content.Context;
import android.content.DialogInterface;

import androidx.appcompat.app.AlertDialog;

import org.chromium.chrome.browser.keyboard_accessory.R;

import java.lang.ref.WeakReference;

/**
 * Helps to show a confirmation.
 */
public class ConfirmationHelper implements DialogInterface.OnClickListener {
    private final WeakReference<Context> mContext;
    private AlertDialog mConfirmationDialog;
    private Runnable mConfirmedCallback;

    public ConfirmationHelper(WeakReference<Context> context) {
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
     * Shows an dialog to confirm the deletion.
     * @param title A {@link String} used as title.
     * @param message A {@link String} used message body.
     */
    public void showConfirmation(String title, String message, Runnable confirmedCallback) {
        assert title != null;
        assert message != null;
        assert confirmedCallback != null;

        Context context = mContext.get();
        if (context == null) return;
        mConfirmedCallback = confirmedCallback;
        mConfirmationDialog = new AlertDialog.Builder(context, R.style.Theme_Chromium_AlertDialog)
                                      .setTitle(title)
                                      .setMessage(message)
                                      .setNegativeButton(R.string.cancel, null)
                                      .setPositiveButton(R.string.ok, this)
                                      .create();
        mConfirmationDialog.show();
    }
}
