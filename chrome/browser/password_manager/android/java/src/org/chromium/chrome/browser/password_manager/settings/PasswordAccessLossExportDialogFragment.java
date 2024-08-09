// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.app.Dialog;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.chrome.browser.password_manager.R;

/**
 * The modal dialog fragment, which explains the users who have lost the access to the password
 * settings (due to migration errors or outdated GMS Core), that they need to export their
 * passwords. This class also owns and performs the export flow.
 */
public class PasswordAccessLossExportDialogFragment extends DialogFragment {
    /** Constructs the dialog message. */
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        return new AlertDialog.Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                .setView(
                        getLayoutInflater()
                                .inflate(R.layout.password_access_loss_export_dialog_view, null))
                .create();
    }
}
