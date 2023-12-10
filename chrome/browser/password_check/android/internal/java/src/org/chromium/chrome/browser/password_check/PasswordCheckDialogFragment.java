// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.content.DialogInterface;
import android.os.Bundle;

import androidx.fragment.app.DialogFragment;

/** Class to create a dialog in the Password check view. */
public class PasswordCheckDialogFragment extends DialogFragment {
    /**
     * This interface combines handling the clicks on the buttons and the general dismissal of the
     * dialog.
     */
    interface Handler extends DialogInterface.OnClickListener {
        /** Handle the dismissal of the dialog.*/
        void onDismiss();
    }

    // This handler is used to answer the user actions on the dialog.
    protected final Handler mHandler;

    PasswordCheckDialogFragment(Handler handler) {
        mHandler = handler;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState != null) {
            dismiss();
            return;
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        if (mHandler != null) mHandler.onDismiss();
    }
}
