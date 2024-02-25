// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.DialogInterface;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;

public class LaunchFailedActivity extends AppCompatActivity {
    /**
     * Tracks whether an exception has been reported.
     *
     * We do this once per process, to avoid spamming if the user keeps trying to relaunch but don't
     * bother with persistence since a few repeated attempts at different time intervals will be
     * spaced out.
     */
    private static boolean sNotified;

    @Override
    protected void onStart() {
        super.onStart();

        if (!sNotified) {
            sNotified = true;
            // Report a simulated exception to identify problematic configurations.
            ChromePureJavaExceptionReporter.reportJavaException(
                    new Throwable("Invalid configuration"));
        }

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setMessage(getString(R.string.update_needed))
                .setPositiveButton(
                        getString(R.string.ok),
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int id) {
                                finish();
                            }
                        });
        builder.create().show();
    }
}
