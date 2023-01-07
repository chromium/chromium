// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.content_creation.reactions.internal.R;
import org.chromium.components.browser_ui.widget.MaterialProgressBar;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Shows the dialog that informs the user about the progress of the reaction GIF generation. Also
 * allows the user to cancel the generation and go back to editing the scene.
 */
public class LightweightReactionsProgressDialog extends DialogFragment {
    private MaterialProgressBar mProgressBar;
    private TextView mProgressPercentage;
    private View.OnClickListener mCancelListener;

    void setCancelProgressListener(View.OnClickListener listener) {
        mCancelListener = listener;
    }

    /**
     * Opens the dialog with the progress bar, hooks up the cancel button handler, sets the
     * appropriate colors, and sets the progress indicator to 0%.
     */
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        Activity activity = getActivity();
        View dialogView =
                activity.getLayoutInflater().inflate(R.layout.reactions_loading_view, null);

        mProgressBar = dialogView.findViewById(R.id.reactions_progress_bar);
        mProgressBar.setBackgroundColor(activity.getColor(R.color.modern_grey_300));
        mProgressBar.setProgressColor(activity.getColor(R.color.modern_white));
        mProgressBar.setContentDescription(
                getActivity().getString(R.string.lightweight_reactions_creating_gif_announcement));
        mProgressPercentage = dialogView.findViewById(R.id.reactions_progress_percentage);
        // Since the progress bar has a content description, the user does not need to be alerted
        // every time the progress bar advances.
        mProgressPercentage.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);

        Button cancelButton = dialogView.findViewById(R.id.reactions_progress_cancel);
        cancelButton.setOnClickListener(mCancelListener);
        setProgress(0);

        AlertDialog alertDialog =
                new AlertDialog
                        .Builder(getActivity(),
                                R.style.ThemeOverlay_BrowserUI_AlertDialog_NoActionBar)
                        .setView(dialogView)
                        .create();
        alertDialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        alertDialog.setCanceledOnTouchOutside(false);
        return alertDialog;
    }

    /**
     * Updates the progress bar and the text to {@code progress}.
     */
    void setProgress(int progress) {
        if (getDialog() != null) {
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
                mProgressPercentage.setText(getActivity().getString(
                        R.string.lightweight_reactions_creating_gif, progress));
                mProgressBar.setProgress(progress);
            });
        }
    }
}
