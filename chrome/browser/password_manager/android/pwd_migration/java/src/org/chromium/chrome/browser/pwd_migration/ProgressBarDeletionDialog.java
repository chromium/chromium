// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.app.Activity;
import android.app.Dialog;
import android.os.Bundle;
import android.view.View;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.components.browser_ui.widget.MaterialProgressBar;

/**
 * Shows the dialog that informs the user about the progress of password deletion.
 */
public class ProgressBarDeletionDialog extends DialogFragment {
    /**
     * Opens the dialog with the progress bar and sets the progress indicator to being
     * indeterminate, because the background operation does not easily allow to signal its own
     * progress.
     */
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        Activity activity = getActivity();
        View dialog =
                activity.getLayoutInflater().inflate(R.layout.passwords_progress_dialog, null);
        MaterialProgressBar bar =
                (MaterialProgressBar) dialog.findViewById(R.id.passwords_progress_bar);
        bar.setIndeterminate(true);
        return new AlertDialog
                .Builder(activity, R.style.ThemeOverlay_BrowserUI_AlertDialog_NoActionBar)
                .setView(dialog)
                .setTitle(R.string.clear_browsing_data_progress_message)
                .create();
    }
}
