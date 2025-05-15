// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Dialog;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts.CreateDocument;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.base.Callback;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Dialog offering the users the option to download their unmigrated passwords. It has the ability
 * to open an activity that asks the user where it wants to save the CSV and then pass back the URI
 * to the destination file.
 */
@NullMarked
public class PasswordCsvDownloadDialogFragment extends DialogFragment {
    private @MonotonicNonNull Callback<Uri> mOnDestinationDocumentCreatedCallback;
    private @MonotonicNonNull View mDialogView;
    ActivityResultLauncher<String> mCreateFileOnDisk;

    @Initializer
    void initialize(View dialogView, Callback<Uri> onDestinationDocumentCreatedCallback) {
        mDialogView = dialogView;
        mOnDestinationDocumentCreatedCallback = onDestinationDocumentCreatedCallback;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (mDialogView == null) {
            // A null view means that the fragment has been restored after the activity was
            // temporarily destroyed, so the whole component needs to be reinitialized.
            PasswordCsvDownloadFlowControllerFactory.reinitializeComponent(getActivity(), this);
        }

        assertNonNull(mOnDestinationDocumentCreatedCallback);
        mCreateFileOnDisk =
                registerForActivityResult(
                        new CreateDocument("text/csv"),
                        (Uri uri) -> mOnDestinationDocumentCreatedCallback.onResult(uri));
    }

    @Override
    public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
        // In case the fragment is being recreated after the activity was temporarily destroyed,
        // the same view was already attached to a parent. De-attach it, before recreating
        // the dialog.
        assumeNonNull(mDialogView);
        if (mDialogView.getParent() != null) {
            ((ViewGroup) mDialogView.getParent()).removeView(mDialogView);
        }

        return new AlertDialog.Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                .setView(mDialogView)
                .create();
    }

    void runCreateFileOnDiskIntent() {
        mCreateFileOnDisk.launch(
                getResources().getString(R.string.password_manager_default_export_filename));
    }
}
