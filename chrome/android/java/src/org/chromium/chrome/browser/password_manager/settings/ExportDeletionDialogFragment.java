// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.chrome.R;

/**
 * An alert dialog that offers to delete passwords which were exported.
 */
public class ExportDeletionDialogFragment extends DialogFragment {
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        return new AlertDialog
                .Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_AlertDialog_NoActionBar)
                .setPositiveButton(R.string.exported_passwords_delete_button, this::onButtonClick)
                .setNegativeButton(R.string.cancel, this::onButtonClick)
                .setMessage(getActivity().getResources().getString(
                        R.string.exported_passwords_deletion_dialog_text))
                .setTitle(getActivity().getResources().getString(
                        R.string.exported_passwords_deletion_dialog_title))
                .create();
    }

    private void onButtonClick(DialogInterface unused, int button) {
        // TODO(crbug.com/1455971): Wire password deletion/dialog dismissal
        // depending ont he button.
    }
}
