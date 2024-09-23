// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/** File utilities for Custom Tabs. */
public class CustomTabFileUtils {
    /** Threshold where old state files should be deleted (30 days). */
    protected static final long STATE_EXPIRY_THRESHOLD = 30L * 24 * 60 * 60 * 1000;

    /** Maximum number of state files before we should start deleting old ones. */
    protected static final int MAXIMUM_STATE_FILES = 30;

    private CustomTabFileUtils() {}

    /**
     * Given a list of files, determine which are applicable for deletion based on the deletion
     * strategy of Custom Tabs.
     *
     * @param currentTimeMillis The current time in milliseconds ({@link
     *     System#currentTimeMillis()}.
     * @param allFiles The complete list of all files to check.
     * @return The list of files that are applicable for deletion.
     */
    public static List<File> getFilesForDeletion(long currentTimeMillis, List<File> allFiles) {
        Collections.sort(
                allFiles,
                new Comparator<>() {
                    @Override
                    public int compare(File lhs, File rhs) {
                        long lhsModifiedTime = lhs.lastModified();
                        long rhsModifiedTime = rhs.lastModified();

                        // Sort such that older files (those with an lower timestamp number) are at
                        // the end of the sorted listed.
                        return Long.compare(rhsModifiedTime, lhsModifiedTime);
                    }
                });

        List<File> filesApplicableForDeletion = new ArrayList<>();
        for (int i = 0; i < allFiles.size(); i++) {
            File file = allFiles.get(i);
            long fileAge = currentTimeMillis - file.lastModified();
            if (i >= MAXIMUM_STATE_FILES || fileAge >= STATE_EXPIRY_THRESHOLD) {
                filesApplicableForDeletion.add(file);
            }
        }
        return filesApplicableForDeletion;
    }
}
