// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.app.TimePickerDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.widget.Button;
import android.widget.TimePicker;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.download.R;

/**
 * A {@link TimePickerDialog} subclass for download time selection. Works on all Android versions.
 */
class DownloadTimePickerDialog extends TimePickerDialog {
    interface Controller {
        void onDownloadTimePicked(int hourOfDay, int minute);
        void onDownloadTimePickerCanceled();
    }

    private int mHourOfDay;
    private int mMinute;
    private final Controller mController;
    private boolean mButtonClicked;

    DownloadTimePickerDialog(
            Context context, @NonNull Controller controller, int hourOfDay, int minute) {
        super(context, R.style.Theme_DownloadDateTimePickerDialog, null, hourOfDay, minute,
                false /*is24HourView*/);
        mHourOfDay = hourOfDay;
        mMinute = minute;
        mController = controller;
        setOnDismissListener((dialogInterface) -> {
            if (mButtonClicked) return;
            mController.onDownloadTimePickerCanceled();
        });
    }

    // TimePickerDialog overrides.
    @Override
    public void show() {
        super.show();

        // Override button click listeners. Notice the default behavior varies on different Android
        // versions.
        Button button = getButton(DialogInterface.BUTTON_POSITIVE);
        assert button != null;
        button.setText(R.string.download_date_time_picker_next_text);
        button.setOnClickListener((view) -> {
            mButtonClicked = true;
            mController.onDownloadTimePicked(mHourOfDay, mMinute);
            dismiss();
        });

        button = getButton(DialogInterface.BUTTON_NEGATIVE);
        button.setText(R.string.cancel);
        button.setOnClickListener((view) -> {
            mButtonClicked = true;
            mController.onDownloadTimePickerCanceled();
            dismiss();
        });
    }

    @Override
    public void onTimeChanged(TimePicker view, int hourOfDay, int minute) {
        mHourOfDay = hourOfDay;
        mMinute = minute;
    }
}