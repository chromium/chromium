// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager.ui;

import android.content.Context;
import android.icu.text.NumberFormat;

import java.util.Locale;

/** Provides methods to stringify task properties. */
class PropertyStringifier {
    // Following methods stringifies the given task property. See task_manager_table_model.cc
    // for the corresponding C++ implementations that are used in non-Android platforms.

    static String getMemoryUsageText(Context context, long usageBytes) {
        if (usageBytes == -1) {
            return naString(context);
        }
        long usageKb = usageBytes / 1024;
        String number = NumberFormat.getInstance(Locale.getDefault()).format(usageKb).toString();
        return context.getString(R.string.task_manager_mem_cell_text).replace("$1", number);
    }

    static String getCpuUsageText(Context context, float cpuUsage) {
        if (Float.isNaN(cpuUsage)) {
            return naString(context);
        }
        return String.format(Locale.getDefault(), "%.1f", cpuUsage);
    }

    private static String naString(Context context) {
        return context.getString(R.string.task_manager_na_cell_text);
    }
}
