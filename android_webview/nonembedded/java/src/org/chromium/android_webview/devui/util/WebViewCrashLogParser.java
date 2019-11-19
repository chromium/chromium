// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui.util;

import org.json.JSONException;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.base.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Parses WebView crash JSON log files which contain crash info keys extracted from crash minidump.
 */
public class WebViewCrashLogParser extends CrashInfoLoader {
    private static final String TAG = "WebViewCrashUI";

    private File mLogDir;

    /**
     * @param logDir the directory where WebView store crash logs.
     */
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
            try {
                CrashInfo crashInfo = CrashInfo.readFromJsonString(readEntireFile(logFile));
                // TODO(987806) remove the null check.
                if (crashInfo.localId != null) infoList.add(crashInfo);
            } catch (JSONException e) {
                Log.e(TAG, "Error while reading JSON", e);
            } catch (IOException e) {
                Log.e(TAG, "Error while reading log file", e);
            }
        }

        return infoList;
    }

    private static String readEntireFile(File file) throws IOException {
        try (FileInputStream fileInputStream = new FileInputStream(file)) {
            byte[] data = new byte[(int) file.length()];
            fileInputStream.read(data);
            return new String(data);
        }
    }
}
