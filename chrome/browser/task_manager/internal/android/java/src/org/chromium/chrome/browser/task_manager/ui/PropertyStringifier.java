// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import android.content.Context;

import org.chromium.chrome.browser.task_manager.TaskManagerServiceBridge.GpuMemoryUsage;
import org.chromium.ui.base.BytesFormatting;

import java.text.NumberFormat;
import java.util.Locale;

/** Provides methods to stringify task properties. */
class PropertyStringifier {
    private static final String ZERO_STRING = "0";
    private static final String ASTERISK_STRING = "*";

    // Following methods stringifies the given task property. See task_manager_table_model.cc
    // for the corresponding C++ implementations that are used in non-Android platforms.

    static String getMemoryUsageText(Context context, long usageBytes) {
        if (usageBytes == -1) {
            return naString(context);
        }
        long usageMb = usageBytes / (1024L * 1024L);
        String number = NumberFormat.getInstance(Locale.getDefault()).format(usageMb);
        return context.getString(R.string.task_manager_mem_cell_mb_text, number);
    }

    static String getMemoryUsageText(Context context, GpuMemoryUsage memoryUsage) {
        if (memoryUsage.bytes == -1) {
            return naString(context);
        } else if (memoryUsage.hasDuplicates) {
            return getMemoryUsageText(context, memoryUsage.bytes) + ASTERISK_STRING;
        } else {
            return getMemoryUsageText(context, memoryUsage.bytes);
        }
    }

    static String getCpuUsageText(Context context, float cpuUsage) {
        if (Float.isNaN(cpuUsage)) {
            return naString(context);
        }
        return percentageForUi(cpuUsage);
    }

    static String getNetworkUsageText(Context context, long networkUsage) {
        if (networkUsage == -1) {
            return naString(context);
        }
        if (networkUsage == 0) {
            return ZERO_STRING;
        }
        return BytesFormatting.formatSpeed(networkUsage);
    }

    private static String naString(Context context) {
        return context.getString(R.string.task_manager_na_cell_text);
    }

    private static String percentageForUi(float percentage) {
        NumberFormat formatter = NumberFormat.getPercentInstance(Locale.getDefault());
        formatter.setMinimumFractionDigits(1);
        formatter.setMaximumFractionDigits(1);
        return formatter.format(percentage / 100.0);
    }
}
