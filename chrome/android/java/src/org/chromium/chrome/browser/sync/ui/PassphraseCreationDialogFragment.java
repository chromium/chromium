// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.Activity;
import android.app.Dialog;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v7.app.AlertDialog;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * Dialog to ask the user to enter a new custom passphrase.
 */
public class PassphraseCreationDialogFragment extends DialogFragment {
    public interface Listener { void onPassphraseCreated(String passphrase); }

    private EditText mEnterPassphrase;
    private EditText mConfirmPassphrase;

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        super.onCreateDialog(savedInstanceState);
        LayoutInflater inflater = getActivity().getLayoutInflater();
        View view = inflater.inflate(R.layout.sync_custom_passphrase, null);
        mEnterPassphrase = (EditText) view.findViewById(R.id.passphrase);
        mConfirmPassphrase = (EditText) view.findViewById(R.id.confirm_passphrase);

        mConfirmPassphrase.setOnEditorActionListener(new OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    tryToSubmitPassphrase();
                }
                return false;
            }
        });

        TextView instructionsView =
                (TextView) view.findViewById(R.id.custom_passphrase_instructions);
        instructionsView.setMovementMethod(LinkMovementMethod.getInstance());
        instructionsView.setText(getInstructionsText());

        AlertDialog dialog =
                new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                        .setView(view)
                        .setTitle(R.string.sync_passphrase_type_custom_dialog_title)
                        .setPositiveButton(R.string.save, null)
                        .setNegativeButton(R.string.cancel, null)
                        .create();
        dialog.getDelegate().setHandleNativeActionModesEnabled(false);
        return dialog;
    }

    private SpannableString getInstructionsText() {
        final Activity activity = getActivity();
        return SpanApplier.applySpans(
                activity.getString(R.string.sync_custom_passphrase),
                new SpanInfo("<learnmore>", "</learnmore>", new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        HelpAndFeedback.getInstance().show(activity,
                                activity.getString(R.string.help_context_change_sync_passphrase),
                                Profile.getLastUsedProfile(), null);
                    }
                }));
    }

    @Override
    public void onStart() {
        super.onStart();
        AlertDialog d = (AlertDialog) getDialog();
        if (d != null) {
            // Override the button's onClick listener. The default gets set in the dialog's
            // onCreate, when it is shown (in super.onStart()), so we have to do this here.
            // Otherwise the dialog will close when the button is clicked regardless of what else we
            // do.
            d.getButton(Dialog.BUTTON_POSITIVE).setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    tryToSubmitPassphrase();
                }
            });
        }
    }

    private void tryToSubmitPassphrase() {
        String passphrase = mEnterPassphrase.getText().toString();
        String confirmPassphrase = mConfirmPassphrase.getText().toString();

        if (!passphrase.equals(confirmPassphrase)) {
            mEnterPassphrase.setError(null);
            mConfirmPassphrase.setError(getString(R.string.sync_passphrases_do_not_match));
            mConfirmPassphrase.requestFocus();
            return;
        } else if (passphrase.isEmpty()) {
            mConfirmPassphrase.setError(null);
            mEnterPassphrase.setError(getString(R.string.sync_passphrase_cannot_be_blank));
            mEnterPassphrase.requestFocus();
            return;
        }

        // The passphrase is not empty and matches.
        ((Listener) getTargetFragment()).onPassphraseCreated(passphrase);
        getDialog().dismiss();
    }
}
