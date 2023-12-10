// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui.util;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.nonembedded.crash.CrashInfo;
import org.chromium.android_webview.nonembedded.crash.SystemWideCrashDirectories;
import org.chromium.base.Log;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Aggregates webview crash info from different sources into one single list.
 * This list may be used to be displayed in a Crash UI.
 */
public class WebViewCrashInfoCollector {
    private static final String TAG = "WebViewCrashCollect";

    private final CrashInfoLoader[] mCrashInfoLoaders;

    /** Funcational interface to implement special filters to crashes. */
    public static interface Filter {
        /** @return {@code true} to keep the {@link CrashInfo}, {@code false} to filter it out. */
        public boolean test(CrashInfo c);
    }

    /** A class that creates the CrashInfoLoaders that the collector uses. Allows mocking in tests. */
    @VisibleForTesting
    public static class CrashInfoLoadersFactory {
        public CrashInfoLoader[] create() {
            CrashFileManager crashFileManager =
                    new CrashFileManager(SystemWideCrashDirectories.getOrCreateWebViewCrashDir());

            return new CrashInfoLoader[] {
                new UploadedCrashesInfoLoader(crashFileManager.getCrashUploadLogFile()),
                new UnuploadedFilesStateLoader(crashFileManager),
                new WebViewCrashLogParser(SystemWideCrashDirectories.getWebViewCrashLogDir())
            };
        }
    }

    public WebViewCrashInfoCollector() {
        this(new CrashInfoLoadersFactory());
    }

    @VisibleForTesting
    public WebViewCrashInfoCollector(CrashInfoLoadersFactory loadersFactory) {
        mCrashInfoLoaders = loadersFactory.create();
    }

    /**
     * Aggregates crashes from different resources and removes duplicates.
     * Crashes are sorted by most recent (crash capture time).
     *
     * @return list of crashes, sorted by the most recent.
     */
    public List<CrashInfo> loadCrashesInfo() {
        List<CrashInfo> allCrashes = new ArrayList<>();
        for (CrashInfoLoader loader : mCrashInfoLoaders) {
            allCrashes.addAll(loader.loadCrashesInfo());
        }
        allCrashes = mergeDuplicates(allCrashes);
        sortByMostRecent(allCrashes);

        return allCrashes;
    }

    /**
     * Aggregates crashes from different resources and removes duplicates.
     * Crashes are sorted by most recent (crash capture time).
     *
     * @param filter {@link Filter} object to filter crashes from the list.
     * @return list crashes after applying {@code filter} to each item, sorted by the most recent.
     */
    public List<CrashInfo> loadCrashesInfo(Filter filter) {
        List<CrashInfo> filtered = new ArrayList<>();
        for (CrashInfo info : loadCrashesInfo()) {
            if (filter.test(info)) {
                filtered.add(info);
            }
        }
        return filtered;
    }

    /**
     * Merge duplicate crashes (crashes which have the same local-id) into one object.
     * Removes crashes that have to be hidden.
     */
    @VisibleForTesting
    public static List<CrashInfo> mergeDuplicates(List<CrashInfo> crashesList) {
        Map<String, CrashInfo> crashInfoMap = new HashMap<>();
        Set<String> hiddenCrashes = new HashSet<>();
        for (CrashInfo c : crashesList) {
            if (c.isHidden) {
                hiddenCrashes.add(c.localId);
                continue;
            }
            CrashInfo previous = crashInfoMap.get(c.localId);
            if (previous != null) {
                c = new CrashInfo(previous, c);
            }
            crashInfoMap.put(c.localId, c);
        }

        List<CrashInfo> crashes = new ArrayList<>();
        for (Map.Entry<String, CrashInfo> entry : crashInfoMap.entrySet()) {
            if (!hiddenCrashes.contains(entry.getKey())) {
                crashes.add(entry.getValue());
            }
        }
        return crashes;
    }

    /**
     * Sort the list by most recent capture time, if capture time is equal or is unknown (-1),
     * upload time will be used.
     */
    @VisibleForTesting
    public static void sortByMostRecent(List<CrashInfo> list) {
        Collections.sort(
                list,
                (a, b) -> {
                    if (a.captureTime != b.captureTime) {
                        return a.captureTime < b.captureTime ? 1 : -1;
                    }
                    if (a.uploadTime != b.uploadTime) {
                        return a.uploadTime < b.uploadTime ? 1 : -1;
                    }
                    return 0;
                });
    }

    /** Modify WebView crash JSON log file with the new crash info if the JSON file exists */
    public static void updateCrashLogFileWithNewCrashInfo(CrashInfo crashInfo) {
        File logDir = SystemWideCrashDirectories.getOrCreateWebViewCrashLogDir();
        File[] logFiles = logDir.listFiles();
        for (File logFile : logFiles) {
            // Ignore non-json files.
            if (!logFile.isFile() || !logFile.getName().endsWith(".json")) continue;
            // Ignore unrelated json files
            if (!logFile.getName().contains(crashInfo.localId)) continue;
            tryWritingCrashInfoToLogFile(crashInfo, logFile);
            return;
        }
        // logfile does not exist, so creates and writes to a new logfile
        File newLogFile = SystemWideCrashDirectories.createCrashJsonLogFile(crashInfo.localId);
        tryWritingCrashInfoToLogFile(crashInfo, newLogFile);
    }

    private static void tryWritingCrashInfoToLogFile(CrashInfo crashInfo, File logFile) {
        try {
            FileWriter writer = new FileWriter(logFile);
            try {
                writer.write(crashInfo.serializeToJson());
            } finally {
                writer.close();
            }
        } catch (IOException e) {
            Log.e(TAG, "failed to modify JSON log entry for crash", e);
        }
    }
}
