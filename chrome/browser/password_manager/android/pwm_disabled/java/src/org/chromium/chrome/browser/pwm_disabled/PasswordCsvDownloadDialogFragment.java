// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import android.app.Dialog;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts.CreateDocument;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.base.Callback;

/**
 * Dialog offering the users the option to download their unmigrated passwords. It has the ability
 * to open an activity that asks the user where it wants to save the CSV and then pass back the URI
 * to the destination file.
 */
public class PasswordCsvDownloadDialogFragment extends DialogFragment {
    private Callback<Uri> mOnDestinationFileCreatedCallback;
    private View mDialogView;
    ActivityResultLauncher<String> mCreateFileOnDisk;

    public void setView(View dialogView) {
        mDialogView = dialogView;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // TODO(crbug.com/378653384): Recover gracefully if the fragment is re-created.
        mCreateFileOnDisk =
                registerForActivityResult(
                        new CreateDocument("text/csv"), this::onDestinationDocumentCreated);
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        return new AlertDialog.Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                .setView(mDialogView)
                .create();
    }

    public void runCreateFileOnDiskIntent(Callback<Uri> onDestinationFileCreated) {
        mOnDestinationFileCreatedCallback = onDestinationFileCreated;
        mCreateFileOnDisk.launch(
                getResources().getString(R.string.password_manager_default_export_filename));
    }

    private void onDestinationDocumentCreated(Uri destinationFileUri) {
        mOnDestinationFileCreatedCallback.onResult(destinationFileUri);
    }
}
