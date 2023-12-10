// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.app.Dialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.text.InputType;
import android.view.View;
import android.view.WindowManager.LayoutParams;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;

import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager.ReauthScope;

/** Shows the dialog that allows the user to see the compromised credential. */
public class PasswordCheckViewDialogFragment extends PasswordCheckDialogFragment {
    private CompromisedCredential mCredential;

    PasswordCheckViewDialogFragment(Handler handler, CompromisedCredential credential) {
        super(handler);
        mCredential = credential;
    }

    /** Opens the dialog with the compromised credential */
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        getActivity().getWindow().setFlags(LayoutParams.FLAG_SECURE, LayoutParams.FLAG_SECURE);
        View dialogContent =
                getActivity()
                        .getLayoutInflater()
                        .inflate(R.layout.password_check_view_credential_dialog, null);
        TextView passwordView = dialogContent.findViewById(R.id.view_dialog_compromised_password);
        passwordView.setText(mCredential.getPassword());
        passwordView.setInputType(
                InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
                        | InputType.TYPE_TEXT_FLAG_MULTI_LINE);

        ClipboardManager clipboard =
                (ClipboardManager)
                        getActivity()
                                .getApplicationContext()
                                .getSystemService(Context.CLIPBOARD_SERVICE);
        ImageButton copyButton = dialogContent.findViewById(R.id.view_dialog_copy_button);
        copyButton.setClickable(true);
        copyButton.setOnClickListener(
                unusedView -> {
                    ClipData clip = ClipData.newPlainText("password", mCredential.getPassword());
                    clipboard.setPrimaryClip(clip);
                });

        AlertDialog viewDialog =
                new AlertDialog.Builder(getActivity())
                        .setTitle(mCredential.getDisplayOrigin())
                        .setNegativeButton(R.string.close, mHandler)
                        .setView(dialogContent)
                        .create();
        return viewDialog;
    }

    @Override
    public void onResume() {
        super.onResume();
        if (!ReauthenticationManager.authenticationStillValid(ReauthScope.ONE_AT_A_TIME)) {
            // If the page was idle (e.g. screenlocked for a few minutes), close the dialog to
            // ensure the user goes through reauth again.
            dismiss();
        }
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        getActivity().getWindow().clearFlags(LayoutParams.FLAG_SECURE);
    }
}
