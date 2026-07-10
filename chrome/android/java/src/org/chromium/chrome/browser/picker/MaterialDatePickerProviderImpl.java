// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.picker;

import android.content.Context;

import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.datepicker.CalendarConstraints;
import com.google.android.material.datepicker.DateValidatorPointForward;
import com.google.android.material.datepicker.MaterialDatePicker;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.ui.base.MaterialDatePickerProvider;

@ServiceImpl(MaterialDatePickerProvider.class)
@NullMarked
public class MaterialDatePickerProviderImpl implements MaterialDatePickerProvider {
    private static final String TAG = "MaterialDatePicker";

    @Override
    public boolean showDatePicker(
            Context context,
            long min,
            long max,
            Callback<Long> selectionCallback,
            Runnable onDismiss) {
        if (!(context instanceof AppCompatActivity)) {
            Log.w(TAG, "Cannot show MaterialDatePicker: context is not an AppCompatActivity");
            return false;
        }

        // Convert min/max to CalendarConstraints (The "M3 Normalizer")
        CalendarConstraints constraints =
                new CalendarConstraints.Builder()
                        .setStart(min)
                        .setEnd(max)
                        .setValidator(DateValidatorPointForward.from(min))
                        .build();
        MaterialDatePicker<Long> picker =
                MaterialDatePicker.Builder.datePicker().setCalendarConstraints(constraints).build();
        picker.addOnPositiveButtonClickListener(
                selection -> {
                    selectionCallback.onResult(selection);
                });
        picker.addOnNegativeButtonClickListener(
                view -> {
                    onDismiss.run();
                });
        picker.addOnCancelListener(
                dialog -> {
                    onDismiss.run();
                });
        picker.addOnDismissListener(
                dialog -> {
                    onDismiss.run();
                });
        picker.show(
                ((AppCompatActivity) context).getSupportFragmentManager(), "MaterialDatePicker");
        return true;
    }
}
