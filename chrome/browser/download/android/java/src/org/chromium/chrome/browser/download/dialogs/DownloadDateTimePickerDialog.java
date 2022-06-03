// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A date time picker for the user to select download start time. The dialog has two stage:
 * 1. A calendar to let the user to pick the date.
 * 2. A clock to let the user to pick a time.
 */
public interface DownloadDateTimePickerDialog {
    /**
     * The controller that receives events from the date time picker.
     */
    interface Controller {
        /**
         * Called when the user picked the time from date picker and time picker.
         * @param time The time the user picked as a unix timestamp.
         */
        void onDateTimePicked(long time);

        /**
         * The user canceled date time picking flow.
         */
        void onDateTimePickerCanceled();
    }

    /**
     * Initializes the download date time picker dialog.
     * @param controller The controller that receives events from the date time picker.
     */
    void initialize(@NonNull Controller controller);

    /**
     * Shows the date time picker.
     * @param context The {@link Context} for the date time picker.
     * @param modalDialogManager Used to show/dismiss modal dialog.
     * @param model The model that defines the application data used to update the UI view.
     */
    void showDialog(Context context, ModalDialogManager modalDialogManager, PropertyModel model);

    /**
     * Destroys the download date time picker dialog. Usually called before the associated activity
     * is destroyed.
     */
    void destroy();
}
