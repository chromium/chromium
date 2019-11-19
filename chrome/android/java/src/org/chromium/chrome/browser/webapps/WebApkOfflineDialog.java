// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.app.Dialog;
import android.content.DialogInterface;
import android.support.v7.app.AlertDialog;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.ui.UiUtils;

/**
 * A dialog to notify user of network errors while loading WebAPK's start URL.
 */
public class WebApkOfflineDialog {
    private Dialog mDialog;

    /** Returns whether the dialog is showing. */
    public boolean isShowing() {
        return mDialog != null && mDialog.isShowing();
    }

    /**
     * Shows dialog to notify user of network error.
     * @param activity Activity that will be used for {@link Dialog#show()}.
     * @param errorMessage
     */
    public void show(final Activity activity, String errorMessage) {
        AlertDialog.Builder builder = new UiUtils.CompatibleAlertDialogBuilder(
                activity, R.style.Theme_Chromium_AlertDialog);
        builder.setMessage(errorMessage)
                .setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        ApiCompatibilityUtils.finishAndRemoveTask(activity);
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
