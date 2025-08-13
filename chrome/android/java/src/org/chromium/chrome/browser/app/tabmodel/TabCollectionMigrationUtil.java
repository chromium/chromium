// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * A class to manage a shared preference that maps a tab list metadata filename to a boolean,
 * indicating if tab collections were active for that file during its last launch.
 */
@NullMarked
public class TabCollectionMigrationUtil {
    private static final String TAB_COLLECTION_MIGRATION_UTIL_SHARED_PREFS =
            "tab_collection_migration_util_shared_prefs";

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(
                        TAB_COLLECTION_MIGRATION_UTIL_SHARED_PREFS, Context.MODE_PRIVATE);
    }

    /**
     * @param metadataFileName The file name containing the tab metadata.
     * @return Whether the last launch for the given file used tab collections.
     */
    public static boolean wasTabCollectionsActiveForMetadataFile(String metadataFileName) {
        return getSharedPreferences().getBoolean(metadataFileName, false);
    }

    /**
     * Updates the shared preference for a given metadata file to the state of the {@link
     * ChromeFeatureList#TAB_COLLECTIONS_ANDROID} flag.
     *
     * @param metadataFileName The file name containing the tab metadata.
     */
    public static void setTabCollectionsActiveForMetadataFile(String metadataFileName) {
        SharedPreferences.Editor editor = getSharedPreferences().edit();
        editor.putBoolean(metadataFileName, ChromeFeatureList.sTabCollectionAndroid.isEnabled());
        editor.apply();
    }
}
