// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import org.chromium.android_webview.nonembedded.crash.CrashInfo;
import org.chromium.base.Log;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Map;

/** A util class for crash logs. */
public class CrashLoggingUtils {
    private static final String TAG = "CrashLogging";

    /**
     * Writes info about crash in a separate log file for each crash as a JSON Object.
     * Used for both embedded and non-embedded crashes.
     */
    public static boolean writeCrashInfoToLogFile(
            File logFile, File crashFile, Map<String, String> crashInfoMap) {
        try {
            String localId = CrashFileManager.getCrashLocalIdFromFileName(crashFile.getName());
            if (localId == null || crashInfoMap == null) return false;
            CrashInfo crashInfo = new CrashInfo(localId, crashInfoMap);
            crashInfo.captureTime = crashFile.lastModified();

            FileWriter writer = new FileWriter(logFile);
            try {
                writer.write(crashInfo.serializeToJson());
            } finally {
                writer.close();
            }
            return true;
        } catch (IOException e) {
            Log.w(TAG, "failed to write JSON log entry for crash", e);
        }
        return false;
    }
}
