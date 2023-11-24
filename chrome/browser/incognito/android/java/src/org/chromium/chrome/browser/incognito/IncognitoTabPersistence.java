// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import android.util.Pair;

import org.chromium.base.Log;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;

import java.io.File;

/** Manages tab state files for incognito tabs. */
public class IncognitoTabPersistence {
    private static final String TAG = "IncognitoFileDelete";

    /**
     * Deletes files with saved state of incognito tabs.
     * @return whether successful.
     */
    static boolean deleteIncognitoStateFiles() {
        File directory = TabStateDirectory.getOrCreateTabbedModeStateDirectory();
        File[] tabStateFiles = directory.listFiles();
        if (tabStateFiles == null) return true;

        boolean deletionSuccessful = true;
        for (File file : tabStateFiles) {
            Pair<Integer, Boolean> tabInfo =
                    TabStateFileManager.parseInfoFromFilename(file.getName());
            boolean isIncognito = tabInfo != null && tabInfo.second;
            if (isIncognito) {
                deletionSuccessful &= file.delete();
                if (!deletionSuccessful) {
                    Log.e(TAG, "File " + file.getAbsolutePath() + " deletion unsuccessful.");
                }
            }
        }
        return deletionSuccessful;
    }
}
