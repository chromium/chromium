// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.ProfileProvider;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Pattern;

/**
 * Handles the Custom Tab specific behaviors of cookies persistence for off the record Custom tabs.
 */
public class CustomTabCookiesFetcher extends CookiesFetcher {
    @VisibleForTesting static final String COOKIE_FILE_PREFIX = "COOKIES_";
    @VisibleForTesting static final String COOKIE_FILE_EXTENSION = ".DAT";

    private static final Pattern COOKIE_MATCHER =
            Pattern.compile(
                    Pattern.quote(COOKIE_FILE_PREFIX)
                            + "(?:\\d+)"
                            + Pattern.quote(COOKIE_FILE_EXTENSION));

    private static final String TAG = "CCTCookiesFetcher";

    private final int mTaskId;

    /**
     * @param taskId The task ID that the owning Custom Tab is in.
     * @see {@link CookiesFetcher}.
     */
    public CustomTabCookiesFetcher(
            ProfileProvider profileProvider, CipherFactory cipherFactory, int taskId) {
        super(profileProvider, cipherFactory);
        mTaskId = taskId;
    }

    @VisibleForTesting
    @Override
    protected File getCookieDir() {
        return super.getCookieDir();
    }

    @Override
    protected String fetchFileName() {
        return COOKIE_FILE_PREFIX + mTaskId + COOKIE_FILE_EXTENSION;
    }

    @Override
    protected boolean isLegacyFileApplicable() {
        // Legacy cookie file was never used for Custom tab activities.
        return false;
    }

    @Override
    public void restoreCookies(@NonNull Runnable restoreCompletedAction) {
        super.restoreCookies(restoreCompletedAction);

        PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, this::cleanupUnneededCookieFiles);
    }

    /** Handle deleting any outdated cookie files. */
    private void cleanupUnneededCookieFiles() {
        ThreadUtils.assertOnBackgroundThread();
        List<File> cctCookieFiles = getCctCookieFiles(getCookieDir());
        if (cctCookieFiles.isEmpty()) return;

        List<File> deletableFiles =
                CustomTabFileUtils.getFilesForDeletion(System.currentTimeMillis(), cctCookieFiles);
        if (deletableFiles.isEmpty()) return;
        for (File deletableFile : deletableFiles) {
            if (!deletableFile.delete()) {
                Log.e(TAG, "Failed to delete file: " + deletableFile);
            }
        }
    }

    @VisibleForTesting
    static List<File> getCctCookieFiles(File cookieDirectory) {
        List<java.io.File> cctCookieFiles = new ArrayList<>();
        if (!cookieDirectory.exists()) return cctCookieFiles;

        File[] cookieFiles = cookieDirectory.listFiles();
        for (File cookieFile : cookieFiles) {
            String fileName = cookieFile.getName();
            if (!COOKIE_MATCHER.matcher(fileName).matches()) continue;
            cctCookieFiles.add(cookieFile);
        }
        return cctCookieFiles;
    }
}
