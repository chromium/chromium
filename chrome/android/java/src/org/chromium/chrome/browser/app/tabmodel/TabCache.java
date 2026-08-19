// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;
import static org.chromium.chrome.browser.tab.Tab.INVALID_TAB_ID;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;

import java.io.File;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Pure multi-tab FlatBuffer storage engine. Manages asynchronous FlatBuffer serialization,
 * zero-copy mmap deserialization, CipherFactory incognito encryption, and multi-key preloading. Has
 * zero dependency on TabModelSelector or UI observables.
 *
 * <p>Note: TabCache cannot be profile-scoped since it is meant to be available pre-native. Storage
 * is logically partitioned per arbitrary tag and regular/incognito state via {@link TabCacheKey}
 * and {@link CipherFactory} rather than native {@code Profile} objects. Do NOT introduce
 * dependencies on {@code Profile}.
 */
@NullMarked
public class TabCache {
    private static final String TAG = "tab_cache";

    /** The name of the base directory where the state is saved. */
    private static final String CACHE_DIR_NAME = "active_tabs";

    private static @Nullable File sCacheDirectory;
    private static @Nullable SequencedTaskRunner sTaskRunner;
    private static final Object sCacheDirLock = new Object();

    // Global counter used to invalidate outdated tasks.
    private static final AtomicInteger sClearCounter = new AtomicInteger(0);

    private final @Nullable CipherFactory mCipherFactory;
    private final Map<TabCacheKey, FutureTask<@Nullable LoadedTabState>> mPreloadTasks =
            new HashMap<>();

    /**
     * @param cipherFactory The {@link CipherFactory} used to encrypt incognito tab state files.
     */
    public TabCache(@Nullable CipherFactory cipherFactory) {
        mCipherFactory = cipherFactory;
    }

    /**
     * Saves the tab state for a given cache key.
     *
     * @param key The cache key to save into.
     * @param tabId The ID of the tab.
     * @param tabState The tab state to save.
     */
    public void saveTabState(TabCacheKey key, @TabId int tabId, @Nullable TabState tabState) {
        assertOnUiThread();
        cancelTask(key);

        boolean isOffTheRecord = key.isIncognito();
        assert !isOffTheRecord || mCipherFactory != null;

        String fileName = key.getFileName();
        if (tabState == null || tabState.contentsState == null) {
            deleteFileAndPref(fileName);
            return;
        }

        getSharedPreferences().edit().putInt(fileName, tabId).apply();
        int currClearCount = sClearCounter.get();
        getTaskRunner()
                .execute(
                        () -> {
                            if (currClearCount != sClearCounter.get()) return;

                            File file = new File(getOrCreateCacheDirectory(), fileName);
                            TabStateFileManager.saveStateInternal(
                                    file, tabState, isOffTheRecord, mCipherFactory);
                        });
    }

    /**
     * Saves the tab into the given cache key.
     *
     * @param key The cache key to save into.
     * @param tab The tab to save.
     */
    public void saveTab(TabCacheKey key, Tab tab) {
        assertOnUiThread();
        TabState tabState = TabStateExtractor.from(tab);
        saveTabState(key, tab.getId(), tabState);
    }

    /**
     * Initiates asynchronous preloading of the tab associated with the given cache key.
     *
     * @param key The cache key to preload.
     */
    public void preloadTab(TabCacheKey key) {
        assertOnUiThread();
        if (key.isIncognito() && mCipherFactory == null) {
            return;
        }
        if (mPreloadTasks.containsKey(key)) {
            return;
        }
        Callable<@Nullable LoadedTabState> callable = () -> restoreTab(key);
        FutureTask<@Nullable LoadedTabState> task = new FutureTask<>(callable);
        mPreloadTasks.put(key, task);
        getTaskRunner().execute(task);
    }

    /**
     * Restores the tab for the given cache key from cache or preloaded task.
     *
     * @param key The cache key to restore.
     */
    public @Nullable LoadedTabState getPreLoadedTabOrLoad(TabCacheKey key) {
        assertOnUiThread();
        FutureTask<@Nullable LoadedTabState> task = mPreloadTasks.remove(key);
        if (task != null && !task.isCancelled()) {
            try {
                return task.get();
            } catch (ExecutionException | InterruptedException e) {
                // Ignore and attempt to restore on the UI thread.
            }
        }
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return restoreTab(key);
        }
    }

    private @Nullable LoadedTabState restoreTab(TabCacheKey key) {
        boolean incognito = key.isIncognito();
        assert !incognito || mCipherFactory != null;

        String fileName = key.getFileName();
        File file = new File(getOrCreateCacheDirectory(), fileName);
        if (!file.exists()) return null;

        @TabId int tabId = getSharedPreferences().getInt(fileName, INVALID_TAB_ID);
        if (tabId == INVALID_TAB_ID) return null;

        TabState tabState =
                TabStateFileManager.restoreTabStateInternal(file, incognito, mCipherFactory);
        if (tabState == null) return null;

        return new LoadedTabState(tabId, tabState);
    }

    /**
     * Clears the cache for the given cache key.
     *
     * @param key The cache key to clear.
     */
    public void clear(TabCacheKey key) {
        assertOnUiThread();
        cancelTask(key);
        deleteFileAndPref(key.getFileName());
    }

    /** Clears all pending preloading tasks. */
    public void cancelAllTasks() {
        assertOnUiThread();
        for (FutureTask<@Nullable LoadedTabState> task : mPreloadTasks.values()) {
            task.cancel(false);
        }
        mPreloadTasks.clear();
    }

    private void cancelTask(TabCacheKey key) {
        FutureTask<@Nullable LoadedTabState> task = mPreloadTasks.remove(key);
        if (task != null) {
            task.cancel(false);
        }
    }

    /**
     * Static cleanup for a specific cache key.
     *
     * @param key The cache key to clean up.
     */
    public static void cleanup(TabCacheKey key) {
        deleteFileAndPref(key.getFileName());
    }

    /** Clears all tab cache global state. */
    public static void clearGlobalState() {
        assertOnUiThread();

        sClearCounter.incrementAndGet();
        synchronized (sCacheDirLock) {
            sCacheDirectory = null;
        }
        getSharedPreferences().edit().clear().apply();
        getTaskRunner().execute(TabCache::clearGlobalStateInternal);
    }

    private static void clearGlobalStateInternal() {
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
    }

    private static SequencedTaskRunner getTaskRunner() {
        assertOnUiThread();
        if (sTaskRunner == null) {
            sTaskRunner = PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE_MAY_BLOCK);
        }
        return sTaskRunner;
    }

    private static File getOrCreateCacheDirectory() {
        synchronized (sCacheDirLock) {
            if (sCacheDirectory == null) {
                sCacheDirectory = getCacheDirectory();
                if (!sCacheDirectory.exists() && !sCacheDirectory.mkdirs()) {
                    Log.e(TAG, "Failed to create tab cache directory: " + sCacheDirectory);
                }
            }
            return sCacheDirectory;
        }
    }

    private static File getCacheDirectory() {
        return ContextUtils.getApplicationContext().getDir(CACHE_DIR_NAME, Context.MODE_PRIVATE);
    }

    private static void deleteFileAndPref(String fileName) {
        getSharedPreferences().edit().remove(fileName).apply();

        int currClearCount = sClearCounter.get();
        getTaskRunner()
                .execute(
                        () -> {
                            if (currClearCount != sClearCounter.get()) return;

                            File file = new File(getCacheDirectory(), fileName);
                            if (file.exists() && !file.delete()) {
                                Log.e(TAG, "Failed to delete cache file: " + file);
                            }
                        });
    }

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(CACHE_DIR_NAME, Context.MODE_PRIVATE);
    }
}
