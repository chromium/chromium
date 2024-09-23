// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.app.Dialog;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts.CreateDocument;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.chrome.browser.password_manager.R;

/**
 * The modal dialog fragment, which explains the users who have lost the access to the password
 * settings (due to migration errors or outdated GMS Core), that they need to export their
 * passwords. This class also owns and performs the export flow.
 */
public class PasswordAccessLossExportDialogFragment extends DialogFragment {
    public interface Delegate {
        void onDocumentCreated(Uri uri);

        void onResume();
    }

    private View mDialogView;
    private Delegate mDelegate;
    ActivityResultLauncher<String> mCreateFileOnDisk;

    public void setView(View dialogView) {
        mDialogView = dialogView;
    }

    public void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Do not show this fragment if it's being re-created
        if (mDelegate == null) {
            dismiss();
            return;
        }

        mCreateFileOnDisk =
                registerForActivityResult(
                        new CreateDocument("text/csv"), mDelegate::onDocumentCreated);
    }

    /** Constructs the dialog message. */
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        return new AlertDialog.Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                .setView(mDialogView)
                .create();
    }

    public void runCreateFileOnDiskIntent() {
        mCreateFileOnDisk.launch(
                getResources().getString(R.string.password_manager_default_export_filename));
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mDelegate == null) return;
        mDelegate.onResume();
    }
}
