// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import android.app.Activity;
import android.app.Dialog;
import android.os.Bundle;
import android.view.View;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.password_manager.R;
import org.chromium.components.browser_ui.widget.MaterialProgressBar;

/**
 * Shows the dialog that informs the user that some operation is ongoing without indicating the
 * progress.
 *
 * <p>PasswordCsvDownloadFlowControllerTest requires the class to be public (and @VisibleForTesting
 * can't be used in top-level classes).
 */
@NullMarked
public class NonCancelableProgressBar extends DialogFragment {
    private final int mTitleStringId;

    public NonCancelableProgressBar() {
        mTitleStringId = R.string.please_wait_progress_message;
    }

    public NonCancelableProgressBar(int titleStringId) {
        mTitleStringId = titleStringId;
    }

    /**
     * Opens the dialog with the progress bar and sets the progress indicator to being
     * indeterminate, because the background operation does not easily allow to signal its own
     * progress.
     */
    @Override
    public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
        Activity activity = getActivity();
        View dialog =
                activity.getLayoutInflater().inflate(R.layout.passwords_progress_dialog, null);
        MaterialProgressBar bar =
                (MaterialProgressBar) dialog.findViewById(R.id.passwords_progress_bar);
        bar.setIndeterminate(true);
        return new AlertDialog.Builder(
                        activity, R.style.ThemeOverlay_BrowserUI_AlertDialog_NoActionBar)
                .setView(dialog)
                .setTitle(mTitleStringId)
                .create();
    }
}
