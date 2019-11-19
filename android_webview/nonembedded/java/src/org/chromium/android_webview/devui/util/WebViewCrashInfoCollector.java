// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui.util;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.crash.CrashInfo;
import org.chromium.android_webview.common.crash.CrashInfo.UploadState;
import org.chromium.android_webview.common.crash.SystemWideCrashDirectories;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Aggregates webview crash info from different sources into one single list.
 * This list may be used to be displayed in a Crash UI.
 */
public class WebViewCrashInfoCollector {
    private final CrashInfoLoader[] mCrashInfoLoaders;

    public WebViewCrashInfoCollector() {
        CrashFileManager crashFileManager =
                new CrashFileManager(SystemWideCrashDirectories.getOrCreateWebViewCrashDir());

        mCrashInfoLoaders = new CrashInfoLoader[] {
                new UploadedCrashesInfoLoader(crashFileManager.getCrashUploadLogFile()),
                new UnuploadedFilesStateLoader(crashFileManager),

                new WebViewCrashLogParser(SystemWideCrashDirectories.getWebViewCrashLogDir())};
    }

    /**
     * Aggregates crashes from different resources and removes duplicates.
     * Crashes are sorted by most recent (crash capture time).
     *
     * @param limit the max size of crashes to be returned, if negative to return all crashes.
     * @return list of size {@code limit} or less, sorted by the most recent.
     */
    public List<CrashInfo> loadCrashesInfo(int limit) {
        List<CrashInfo> allCrashes = new ArrayList<>();
        for (CrashInfoLoader loader : mCrashInfoLoaders) {
            allCrashes.addAll(loader.loadCrashesInfo());
        }
        allCrashes = mergeDuplicates(allCrashes);
        sortByMostRecent(allCrashes);

        if (limit < 0 || limit >= allCrashes.size()) return allCrashes;
        return allCrashes.subList(0, limit);
    }

    /**
     * Merge duplicate crashes (crashes which have the same local-id) into one object.
     */
    @VisibleForTesting
    public static List<CrashInfo> mergeDuplicates(List<CrashInfo> crashesList) {
        Map<String, CrashInfo> crashInfoMap = new HashMap<>();
        for (CrashInfo c : crashesList) {
            CrashInfo previous = crashInfoMap.get(c.localId);
            if (previous != null) {
                mergeCrashInfo(previous, c);
            } else {
                crashInfoMap.put(c.localId, c);
            }
        }
        return new ArrayList<CrashInfo>(crashInfoMap.values());
    }

    /**
     * Sort the list by most recent capture time, if capture time is equal or is unknown (-1),
     * upload time will be used.
     */
    @VisibleForTesting
    public static void sortByMostRecent(List<CrashInfo> list) {
        Collections.sort(list, (a, b) -> {
            if (a.captureTime != b.captureTime) return a.captureTime < b.captureTime ? 1 : -1;
            if (a.uploadTime != b.uploadTime) return a.uploadTime < b.uploadTime ? 1 : -1;
            return 0;
        });
    }

    private static <T> T getFirstNonNull(T a, T b) {
        return a != null ? a : b;
    }

    /**
     * Merge values from the second object into the first object if the value in the first object is
     * {@code null}.
     */
    @VisibleForTesting
    public static void mergeCrashInfo(CrashInfo a, CrashInfo b) {
        // localId is not merged since it's two CrashInfo objects should be only merged if they have
        // the same localId.
        a.captureTime = a.captureTime != -1 ? a.captureTime : b.captureTime;
        a.uploadId = getFirstNonNull(a.uploadId, b.uploadId);
        a.uploadTime = a.uploadTime != -1 ? a.uploadTime : b.uploadTime;
        a.packageName = getFirstNonNull(a.packageName, b.packageName);
        a.variations = getFirstNonNull(a.variations, b.variations);

        // When merging two CrashInfos if one of the two UploadStates is UPLOADED then the merged
        // object will have an UPLOADED state regardless of the order. Difference in UploadState my
        // be caused by the file suffix not updated or deleted by the time
        // UnuploadedFilesStateLoader parses the crash directory.
        if (a.uploadState != null && b.uploadState != null) {
            if (a.uploadState == UploadState.UPLOADED || b.uploadState == UploadState.UPLOADED) {
                a.uploadState = UploadState.UPLOADED;
            } else {
                assert a.uploadState == b.uploadState;
            }
        } else {
            a.uploadState = getFirstNonNull(a.uploadState, b.uploadState);
        }
        // Since capture time may be the last time the crash file is modified, the oldest capture
        // time will be used regardless of the merging order.
        if (a.captureTime != -1 && b.captureTime != -1) {
            a.captureTime = Math.min(a.captureTime, b.captureTime);
        } else {
            a.captureTime = a.captureTime != -1 ? a.captureTime : b.captureTime;
        }
    }
}
