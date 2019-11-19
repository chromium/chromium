// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.content.Context;
import android.os.Environment;
import android.os.StatFs;
import android.util.Pair;

import org.chromium.base.CollectionUtil;
import org.chromium.base.LocaleUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.util.ConversionUtils;

import java.io.File;
import java.util.Map;

/** Grabs feedback about the current system. */
@JNINamespace("chrome::android")
public class SystemInfoFeedbackSource extends AsyncFeedbackSourceAdapter<StatFs> {
    // AsyncFeedbackSourceAdapter implementation.
    @Override
    protected StatFs doInBackground(Context context) {
        File directory = Environment.getDataDirectory();
        if (!directory.exists()) return null;

        return new StatFs(directory.getPath());
    }

    @Override
    public Map<String, String> getFeedback() {
        Map<String, String> feedback = CollectionUtil.newHashMap(
                Pair.create(
                        "CPU Architecture", SystemInfoFeedbackSourceJni.get().getCpuArchitecture()),
                Pair.create("Available Memory (MB)",
                        Integer.toString(SystemInfoFeedbackSourceJni.get().getAvailableMemoryMB())),
                Pair.create("Total Memory (MB)",
                        Integer.toString(SystemInfoFeedbackSourceJni.get().getTotalMemoryMB())),
                Pair.create("GPU Vendor", SystemInfoFeedbackSourceJni.get().getGpuVendor()),
                Pair.create("GPU Model", SystemInfoFeedbackSourceJni.get().getGpuModel()),
                Pair.create("UI Locale", LocaleUtils.getDefaultLocaleString()));

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
        String getCpuArchitecture();
        String getGpuVendor();
        String getGpuModel();
        int getAvailableMemoryMB();
        int getTotalMemoryMB();
    }
}