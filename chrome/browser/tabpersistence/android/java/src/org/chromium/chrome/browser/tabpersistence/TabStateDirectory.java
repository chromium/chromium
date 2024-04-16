// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabpersistence;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;

import java.io.File;

/**
 * Manages the directory where tab state is saved.
 *
 * <p>TODO(crbug.com/40136597): Deduplicate code between tabbed mode and custom tabs.
 */
public class TabStateDirectory {
    private static final String TAG = "tabpersistence";

    /** The name of the base directory where the state is saved. */
    private static final String BASE_STATE_FOLDER = "tabs";

    /** The name of the directory where the state for tabbed mode is saved. */
    @VisibleForTesting public static final String TABBED_MODE_DIRECTORY = "0";

    /** The name of the directory where the state for custom tabs is saved. */
    public static final String CUSTOM_TABS_DIRECTORY = "custom_tabs";

    /** Prevents two state directories from getting created simultaneously. */
    private static final Object TABBED_MODE_DIR_CREATION_LOCK = new Object();

    /** Prevents two state directories from getting created simultaneously. */
    private static final Object CUSTOM_TABS_DIR_CREATION_LOCK = new Object();

    private static File sTabbedModeStateDirectory;
    private static File sCustomTabsStateDirectory;

    /**
     * The folder where the state should be saved to.
     * @return A file representing the directory that contains TabModelSelector states.
     */
    public static File getOrCreateTabbedModeStateDirectory() {
        synchronized (TABBED_MODE_DIR_CREATION_LOCK) {
            if (sTabbedModeStateDirectory == null) {
                sTabbedModeStateDirectory =
                        new File(getOrCreateBaseStateDirectory(), TABBED_MODE_DIRECTORY);
                if (!sTabbedModeStateDirectory.exists() && !sTabbedModeStateDirectory.mkdirs()) {
                    Log.e(TAG, "Failed to create state folder: " + sTabbedModeStateDirectory);
                }
            }
        }
        return sTabbedModeStateDirectory;
    }

    /**
     * The folder where the state should be saved to.
     * @return A file representing the directory that contains TabModelSelector states.
     */
    public static File getOrCreateCustomTabModeStateDirectory() {
        synchronized (CUSTOM_TABS_DIR_CREATION_LOCK) {
            if (sCustomTabsStateDirectory == null) {
                sCustomTabsStateDirectory =
                        new File(getOrCreateBaseStateDirectory(), CUSTOM_TABS_DIRECTORY);
                if (!sCustomTabsStateDirectory.exists() && !sCustomTabsStateDirectory.mkdirs()) {
                    Log.e(TAG, "Failed to create state folder: " + sCustomTabsStateDirectory);
                }
            }
        }
        return sCustomTabsStateDirectory;
    }

    private static class BaseStateDirectoryHolder {
        // Not final for tests.
        private static File sDirectory;

        static {
            sDirectory =
                    ContextUtils.getApplicationContext()
                            .getDir(BASE_STATE_FOLDER, Context.MODE_PRIVATE);
        }
    }

    /**
     * Directory containing all data for TabModels.  Each subdirectory stores info about different
     * TabModelSelectors, including metadata about each TabModel and TabStates for each of their
     * tabs.
     *
     * @return The parent state directory.
     */
    public static File getOrCreateBaseStateDirectory() {
        return BaseStateDirectoryHolder.sDirectory;
    }

    /** Sets where the base state directory is in tests. */
    public static void setBaseStateDirectoryForTests(File directory) {
        var oldValue = BaseStateDirectoryHolder.sDirectory;
        BaseStateDirectoryHolder.sDirectory = directory;
        ResettersForTesting.register(() -> BaseStateDirectoryHolder.sDirectory = oldValue);
    }

    public static void resetTabbedModeStateDirectoryForTesting() {
        sTabbedModeStateDirectory = null;
    }
}
