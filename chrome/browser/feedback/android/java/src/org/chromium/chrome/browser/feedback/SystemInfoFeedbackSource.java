// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.content.Context;
import android.os.Environment;
import android.os.StatFs;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.LocaleUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.ConversionUtils;

import java.io.File;
import java.util.HashMap;
import java.util.Map;

/** Grabs feedback about the current system. */
@JNINamespace("chrome::android")
@NullMarked
public class SystemInfoFeedbackSource extends AsyncFeedbackSourceAdapter<StatFs> {
    // AsyncFeedbackSourceAdapter implementation.
    @Override
    protected @Nullable StatFs doInBackground(Context context) {
        File directory = Environment.getDataDirectory();
        if (!directory.exists()) return null;

        return new StatFs(directory.getPath());
    }

    @Override
    public Map<String, String> getFeedback() {
        HashMap<String, String> feedback = new HashMap();
        feedback.put("CPU Architecture", SystemInfoFeedbackSourceJni.get().getCpuArchitecture());
        feedback.put(
                "Available Memory (MB)",
                Integer.toString(SystemInfoFeedbackSourceJni.get().getAvailableMemoryMB()));
        feedback.put(
                "Total Memory (MB)",
                Integer.toString(SystemInfoFeedbackSourceJni.get().getTotalMemoryMB()));
        feedback.put("GPU Vendor", SystemInfoFeedbackSourceJni.get().getGpuVendor());
        feedback.put("GPU Model", SystemInfoFeedbackSourceJni.get().getGpuModel());
        feedback.put("UI Locale", LocaleUtils.getDefaultLocaleString());

        StatFs statFs = getResult();
        if (statFs != null) {
            long blockSize = statFs.getBlockSizeLong();
            long availSpace =
                    ConversionUtils.bytesToMegabytes(statFs.getAvailableBlocksLong() * blockSize);
            long totalSpace =
                    ConversionUtils.bytesToMegabytes(statFs.getBlockCountLong() * blockSize);

            feedback.put("Available Storage (MB)", Long.toString(availSpace));
            feedback.put("Total Storage (MB)", Long.toString(totalSpace));
        }

        return feedback;
    }

    @NativeMethods
    interface Natives {
        @JniType("std::string")
        String getCpuArchitecture();

        @JniType("std::string")
        String getGpuVendor();

        @JniType("std::string")
        String getGpuModel();

        int getAvailableMemoryMB();

        int getTotalMemoryMB();
    }
}
