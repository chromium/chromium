// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui.util;

import androidx.annotation.VisibleForTesting;

import org.json.JSONException;

import org.chromium.android_webview.nonembedded.crash.CrashInfo;
import org.chromium.base.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Parses WebView crash JSON log files which contain crash info keys extracted from crash minidump. */
public class WebViewCrashLogParser extends CrashInfoLoader {
    private static final String TAG = "WebViewCrashUI";

    // 30 days to match org.chromium.components.minidump_uploader.CrashFileManager minidump reports
    // max age.
    private static final long MAX_CRASH_REPORT_AGE_MILLIS = TimeUnit.DAYS.toMillis(30);

    private File mLogDir;

    /** @param logDir the directory where WebView store crash logs. */
    public WebViewCrashLogParser(File logDir) {
        mLogDir = logDir;
    }

    /**
     * Load crash info from WebView crash logs under WebView crash log directory.
     *
     * @return list of crashes info
     */
    @Override
    public List<CrashInfo> loadCrashesInfo() {
        List<CrashInfo> infoList = new ArrayList<>();

        if (!mLogDir.exists() || !mLogDir.isDirectory()) return infoList;

        File[] logFiles = mLogDir.listFiles();
        for (File logFile : logFiles) {
            // Ignore non-json files.
            if (!logFile.isFile() || !logFile.getName().endsWith(".json")) continue;
            // Delete old crash report logs.
            long ageInMillis = System.currentTimeMillis() - logFile.lastModified();
            if (ageInMillis > MAX_CRASH_REPORT_AGE_MILLIS) {
                logFile.delete();
                continue;
            }
            try {
                CrashInfo crashInfo = CrashInfo.readFromJsonString(readEntireFile(logFile));
                infoList.add(crashInfo);
            } catch (JSONException e) {
                Log.e(TAG, "Error while reading JSON", e);
            } catch (IOException e) {
                Log.e(TAG, "Error while reading log file", e);
            }
        }

        return infoList;
    }

    @VisibleForTesting
    public static String readEntireFile(File file) throws IOException {
        try (FileInputStream fileInputStream = new FileInputStream(file)) {
            byte[] data = new byte[(int) file.length()];
            fileInputStream.read(data);
            return new String(data);
        }
    }
}
