// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import static org.chromium.components.sync.Passphrase.isExplicitPassphraseType;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.CheckedTextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.components.sync.PassphraseType;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/** Dialog to ask the user select what type of password to use for encryption. */
public class PassphraseTypeDialogFragment extends DialogFragment
        implements DialogInterface.OnClickListener {
    public interface Listener {
        /**
         * Called when the user doesn't have a custom passphrase and taps the option to set up one.
         * This is the only PassphraseType transition allowed by the dialog. Note the passphrase
         * itself hasn't been chosen yet.
         */
        void onChooseCustomPassphraseRequested();
    }

    /** This argument should contain a single value of type {@link PassphraseType}. */
    private static final String ARG_CURRENT_TYPE = "arg_current_type";

    private static final String ARG_IS_CUSTOM_PASSPHRASE_ALLOWED =
            "arg_is_custom_passphrase_allowed";

    public static PassphraseTypeDialogFragment create(
            @PassphraseType int currentType, boolean isCustomPassphraseAllowed) {
        assert currentType >= 0 && currentType <= PassphraseType.MAX_VALUE;
        PassphraseTypeDialogFragment dialog = new PassphraseTypeDialogFragment();
        Bundle args = new Bundle();
        args.putInt(ARG_CURRENT_TYPE, currentType);
        args.putBoolean(ARG_IS_CUSTOM_PASSPHRASE_ALLOWED, isCustomPassphraseAllowed);
        dialog.setArguments(args);
        return dialog;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        final View dialog =
                getActivity().getLayoutInflater().inflate(R.layout.sync_passphrase_types, null);

        final CheckedTextView explicitPassphraseCheckbox =
                dialog.findViewById(R.id.explicit_passphrase_checkbox);
        final CheckedTextView keystorePassphraseCheckbox =
                dialog.findViewById(R.id.keystore_passphrase_checkbox);
        final TextViewWithClickableSpans resetSyncLink = dialog.findViewById(R.id.reset_sync_link);

        if (isExplicitPassphraseType(getArguments().getInt(ARG_CURRENT_TYPE))) {
            explicitPassphraseCheckbox.setChecked(true);
            explicitPassphraseCheckbox.setEnabled(false);
            keystorePassphraseCheckbox.setEnabled(false);
            resetSyncLink.setMovementMethod(LinkMovementMethod.getInstance());
            resetSyncLink.setText(getResetSyncText());
        } else {
            keystorePassphraseCheckbox.setChecked(true);
            resetSyncLink.setVisibility(View.GONE);
            if (getArguments().getBoolean(ARG_IS_CUSTOM_PASSPHRASE_ALLOWED)) {
                explicitPassphraseCheckbox.setOnClickListener(
                        this::onCustomPassphraseCheckboxClicked);
            } else {
                explicitPassphraseCheckbox.setEnabled(false);
            }
        }

        return new AlertDialog.Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                .setNegativeButton(R.string.cancel, this)
                .setTitle(R.string.sync_passphrase_type_title)
                .setView(dialog)
                .create();
    }

    private SpannableString getResetSyncText() {
        return SpanApplier.applySpans(
                getString(R.string.sync_passphrase_encryption_reset_instructions),
                new SpanInfo(
                        "BEGIN_LINK",
                        "END_LINK",
                        new ClickableSpan() {
                            @Override
                            public void onClick(View view) {
                                SyncSettingsUtils.openSyncDashboard(getActivity());
                            }
                        }));
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == DialogInterface.BUTTON_NEGATIVE) {
            dismiss();
        }
    }

    private void onCustomPassphraseCheckboxClicked(View unused) {
        ((Listener) getTargetFragment()).onChooseCustomPassphraseRequested();
        dismiss();
    }
}
