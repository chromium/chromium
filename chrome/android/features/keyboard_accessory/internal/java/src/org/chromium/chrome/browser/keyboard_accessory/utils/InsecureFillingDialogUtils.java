// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.utils;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.keyboard_accessory.R;

/**
 * Provides a method to display a warning dialog when filling a password is requested in a
 * non-secure text field.
 */
public class InsecureFillingDialogUtils {
    /**
     * Displays an alert dialog to warn user that the filling is insecure. May not be displayed if
     * {@param context} is not valid.
     *
     * @param context The Android Context used to display the dialog.
     */
    public static void showWarningDialog(Context context) {
        Activity activity = ContextUtils.activityFromContext(context);
        if (ActivityUtils.isActivityFinishingOrDestroyed(activity)) return;
        AlertDialog.Builder builder =
                new AlertDialog.Builder(context, R.style.ThemeOverlay_BrowserUI_AlertDialog);
        LayoutInflater inflater = LayoutInflater.from(builder.getContext());

        View dialogBody = inflater.inflate(R.layout.confirmation_dialog_view, null);

        TextView titleView = dialogBody.findViewById(R.id.confirmation_dialog_title);
        titleView.setText(R.string.passwords_not_secure_filling);

        TextView messageView = dialogBody.findViewById(R.id.confirmation_dialog_message);
        messageView.setText(R.string.passwords_not_secure_filling_details);

        builder.setView(dialogBody).setPositiveButton(R.string.ok, null).create().show();
    }

    private InsecureFillingDialogUtils() {}
}
