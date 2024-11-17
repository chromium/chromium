// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;

import androidx.appcompat.app.AlertDialog;

import org.chromium.chrome.R;

/** A dialog to notify user of network errors while loading WebAPK's start URL. */
public class WebApkOfflineDialog {
    private Dialog mDialog;

    /** Returns whether the dialog is showing. */
    public boolean isShowing() {
        return mDialog != null && mDialog.isShowing();
    }

    /**
     * Shows dialog to notify user of network error.
     *
     * @param activity Activity that will be used for {@link Dialog#show()}.
     */
    public void show(final Activity activity, String errorMessage) {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(activity, R.style.ThemeOverlay_BrowserUI_AlertDialog);
        builder.setMessage(errorMessage)
                .setPositiveButton(
                        R.string.ok,
                        new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                activity.finishAndRemoveTask();
                            }
                        });

        mDialog = builder.create();
        mDialog.setCanceledOnTouchOutside(false);
        mDialog.show();
    }

    /** Closes the dialog. */
    public void cancel() {
        mDialog.cancel();
    }
}
