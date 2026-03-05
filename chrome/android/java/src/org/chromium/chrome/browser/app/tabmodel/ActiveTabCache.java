// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;

import java.io.File;

/**
 * Responsible for caching the active tab's state and index. This allows for loading the active
 * tab's state pre-native, which the database is unable to do.
 */
@NullMarked
public class ActiveTabCache {
    private static final String TAG = "active_tab_cache";

    /** The name of the base directory where the state is saved. */
    private static final String CACHE_DIR_NAME = "active_tabs";

    private static final String REGULAR_SUFFIX = "_regular";
    private static final String INCOGNITO_SUFFIX = "_incognito";
    private static @Nullable File sActiveTabDirectory;

    /** Data class containing information about a cached active tab. */
    public static class CachedActiveTab {
        public final int tabIndex;
        public final TabState tabState;

        public CachedActiveTab(int tabIndex, TabState tabState) {
            this.tabIndex = tabIndex;
            this.tabState = tabState;
        }
    }

    private final String mRegularTabFileName;
    private final String mIncognitoTabFileName;

    /**
     * @param windowTag The tag for the window being tracked.
     */
    public ActiveTabCache(String windowTag) {
        mRegularTabFileName = getFileName(windowTag, /* incognito= */ false);
        mIncognitoTabFileName = getFileName(windowTag, /* incognito= */ true);
    }

    /**
     * Saves the active tab's state and index to the cache.
     *
     * <p>Note that there is one file per window/otr-status combination, so we atomically "swap" the
     * active tab each time it is updated.
     *
     * @param tab The active tab.
     * @param tabIndex The index of the active tab.
     * @param cipherFactory The cipher factory for encrypting incognito tab state.
     */
    public void saveActiveTab(Tab tab, int tabIndex, CipherFactory cipherFactory) {
        assertOnUiThread();

        boolean isOffTheRecord = tab.isOffTheRecord();
        String fileName = isOffTheRecord ? mIncognitoTabFileName : mRegularTabFileName;
        File file = new File(getOrCreateCacheDirectory(), fileName);
        TabState tabState = TabStateExtractor.from(tab);
        if (tabState == null) return;

        TabStateFileManager.saveStateInternal(file, tabState, isOffTheRecord, cipherFactory);

        getSharedPreferences().edit().putInt(fileName, tabIndex).apply();
    }

    /**
     * Restores the active tab from the cache. If it doesn't exist or failed to restore, return
     * null.
     *
     * @param isOffTheRecord Whether to restore the incognito active tab.
     * @param cipherFactory The cipher factory for decrypting incognito tab state.
     */
    public @Nullable CachedActiveTab restoreActiveTab(
            boolean isOffTheRecord, CipherFactory cipherFactory) {
        assertOnUiThread();

        String fileName = isOffTheRecord ? mIncognitoTabFileName : mRegularTabFileName;
        File file = new File(getOrCreateCacheDirectory(), fileName);
        if (!file.exists()) return null;

        TabState tabState =
                TabStateFileManager.restoreTabStateInternal(file, isOffTheRecord, cipherFactory);
        if (tabState == null) return null;

        int tabIndex = getSharedPreferences().getInt(fileName, -1);
        if (tabIndex == -1) return null;

        return new CachedActiveTab(tabIndex, tabState);
    }

    /**
     * Clears the active tab cache for the given incognito state.
     *
     * @param incognito Whether to clear the incognito or regular active tab.
     */
    public void clearActiveTab(boolean incognito) {
        String fileName = incognito ? mIncognitoTabFileName : mRegularTabFileName;
        deleteFileAndPref(fileName);
    }

    /** Clears all active tab cache for the current window. */
    public void clearCurrentWindow() {
        clearActiveTab(false);
        clearActiveTab(true);
    }

    /**
     * Cleans up the active tab cache for the given window tag.
     *
     * @param windowTag The window tag to clean up.
     */
    public static void cleanupWindow(String windowTag) {
        String regularFileName = getFileName(windowTag, false);
        String incognitoFileName = getFileName(windowTag, true);

        deleteFileAndPref(regularFileName);
        deleteFileAndPref(incognitoFileName);
    }

    /** Clears all active tab cache global state. */
    public static void clearGlobalState() {
        assertOnUiThread();

        File directory = getCacheDirectory();
        if (directory.exists()) {
            File[] files = directory.listFiles();
            if (files != null) {
                for (File f : files) {
                    if (!f.delete()) {
                        Log.e(TAG, "Failed to delete file: " + f);
                    }
                }
            }
            if (!directory.delete()) {
                Log.e(TAG, "Failed to delete directory: " + directory);
            }
        }
        sActiveTabDirectory = null;
        getSharedPreferences().edit().clear().apply();
    }

    private static File getOrCreateCacheDirectory() {
        assertOnUiThread();
        if (sActiveTabDirectory == null) {
            sActiveTabDirectory = getCacheDirectory();
            if (!sActiveTabDirectory.exists() && !sActiveTabDirectory.mkdirs()) {
                Log.e(TAG, "Failed to create active tab cache directory: " + sActiveTabDirectory);
            }
        }
        return sActiveTabDirectory;
    }

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(CACHE_DIR_NAME, Context.MODE_PRIVATE);
    }

    private static File getCacheDirectory() {
        return ContextUtils.getApplicationContext().getDir(CACHE_DIR_NAME, Context.MODE_PRIVATE);
    }

    private static String getFileName(String windowTag, boolean incognito) {
        String suffix = incognito ? INCOGNITO_SUFFIX : REGULAR_SUFFIX;
        return windowTag + suffix;
    }

    private static void deleteFileAndPref(String fileName) {
        assertOnUiThread();
        File file = new File(getCacheDirectory(), fileName);
        if (file.exists() && !file.delete()) {
            Log.e(TAG, "Failed to delete cache file: " + file);
        }
        getSharedPreferences().edit().remove(fileName).apply();
    }
}
