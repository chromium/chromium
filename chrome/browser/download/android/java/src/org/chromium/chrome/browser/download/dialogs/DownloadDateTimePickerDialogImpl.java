// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.app.DatePickerDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.widget.DatePicker;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.chrome.browser.download.R;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Calendar;

/**
 * An implementation of {@link DownloadDateTimePickerDialog} that glues
 * {@link android.app.DatePickerDialog} and {@link android.app.TimePickerDialog} widgets. The user
 * will see the date picker and time picker in a sequence when trying to select a time.
 */
// TODO(xingliu): Add instrumentation test for date/time pickers.
public class DownloadDateTimePickerDialogImpl
        implements DownloadDateTimePickerDialog, DownloadTimePickerDialog.Controller {
    private static final String TAG = "DateTimeDialog";
    private static final long INVALID_TIMESTAMP = -1;
    private DatePickerDialog mDatePickerDialog;
    private boolean mDatePickerButtonClicked;
    private DownloadTimePickerDialog mTimePickerDialog;
    private Controller mController;
    private final Calendar mCalendar = Calendar.getInstance();

    @Override
    public void initialize(@NonNull Controller controller) {
        mController = controller;
    }

    @Override
    public void showDialog(
            Context context, ModalDialogManager modalDialogManager, PropertyModel model) {
        // Reset and compute the initial time.
        long initialTime = DownloadDialogUtils.getLong(model,
                DownloadDateTimePickerDialogProperties.INITIAL_TIME, System.currentTimeMillis());
        mCalendar.setTimeInMillis(initialTime);

        // Reset dialogs.
        destroy();

        // Setup the date picker. Use null DatePickerDialog.OnDateSetListener due to Android API
        // issue.
        mDatePickerDialog = new DatePickerDialog(context,
                R.style.Theme_DownloadDateTimePickerDialog, null, mCalendar.get(Calendar.YEAR),
                mCalendar.get(Calendar.MONTH), mCalendar.get(Calendar.DAY_OF_MONTH));
        long minDate = DownloadDialogUtils.getLong(
                model, DownloadDateTimePickerDialogProperties.MIN_TIME, INVALID_TIMESTAMP);
        long maxDate = DownloadDialogUtils.getLong(
                model, DownloadDateTimePickerDialogProperties.MAX_TIME, INVALID_TIMESTAMP);
        if (minDate > 0) mDatePickerDialog.getDatePicker().setMinDate(minDate);
        if (maxDate > 0) mDatePickerDialog.getDatePicker().setMaxDate(maxDate);

        mDatePickerDialog.setButton(DialogInterface.BUTTON_POSITIVE,
                context.getResources().getString(R.string.download_date_time_picker_next_text),
                this::onDatePickerClicked);
        mDatePickerDialog.setButton(DialogInterface.BUTTON_NEGATIVE,
                context.getResources().getString(R.string.cancel), this::onDatePickerClicked);
        mDatePickerDialog.setOnDismissListener(dialogInterface -> { onDatePickerDismissed(); });

        mTimePickerDialog = new DownloadTimePickerDialog(
                context, this, mCalendar.get(Calendar.HOUR_OF_DAY), mCalendar.get(Calendar.MINUTE));

        // Start the flow.
        mDatePickerDialog.show();
    }

    @Override
    public void destroy() {
        if (mDatePickerDialog != null) mDatePickerDialog.dismiss();
        if (mTimePickerDialog != null) mTimePickerDialog.dismiss();
    }

    private void onDatePickerClicked(DialogInterface dialogInterface, int which) {
        mDatePickerButtonClicked = true;
        switch (which) {
            case DialogInterface.BUTTON_POSITIVE:
                DatePicker datePicker = mDatePickerDialog.getDatePicker();
                mCalendar.set(Calendar.YEAR, datePicker.getYear());
                mCalendar.set(Calendar.MONTH, datePicker.getMonth());
                mCalendar.set(Calendar.DAY_OF_MONTH, datePicker.getDayOfMonth());

                // Show the time picker after the date picker is done.
                mTimePickerDialog.show();
                break;
            case DialogInterface.BUTTON_NEGATIVE:
                onCancel();
                break;
            default:
                Log.e(TAG, "Unsupported button type clicked in date picker, type: %d", which);
        }
    }

    private void onDatePickerDismissed() {
        if (mDatePickerButtonClicked) return;

        onCancel();
    }

    private void onCancel() {
        assert mController != null;
        mCalendar.clear();
        mController.onDateTimePickerCanceled();
    }

    private void onComplete() {
        assert mController != null;
        mController.onDateTimePicked(mCalendar.getTimeInMillis());
        mCalendar.clear();
    }

    // DownloadTimePickerDialog.Controller overrides.
    @Override
    public void onDownloadTimePicked(int hourOfDay, int minute) {
        mCalendar.set(Calendar.HOUR_OF_DAY, hourOfDay);
        mCalendar.set(Calendar.MINUTE, minute);

        onComplete();
    }

    @Override
    public void onDownloadTimePickerCanceled() {
        onCancel();
    }
}
