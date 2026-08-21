// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;
import static org.chromium.chrome.browser.tab.Tab.INVALID_TAB_ID;

import android.util.ArrayMap;

import org.chromium.base.StrictModeContext;
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
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;

/**
 * Pure multi-tab FlatBuffer storage engine. Manages asynchronous FlatBuffer serialization,
 * zero-copy mmap deserialization, CipherFactory incognito encryption, and multi-key preloading. Has
 * zero dependency on TabModelSelector or UI observables.
 *
 * <p>Storage is logically partitioned per arbitrary string tag (managed by {@link
 * TabCacheDirScope}) and regular/incognito state via {@link TabCacheKey} and {@link CipherFactory}.
 */
@NullMarked
public class TabCache {
    private final TabCacheDirScope mDirScope;
    private final @Nullable CipherFactory mCipherFactory;
    private final ArrayMap<TabCacheKey, FutureTask<@Nullable LoadedTabState>> mPreloadTasks =
            new ArrayMap<>();

    /**
     * Package-private constructor. Use {@link TabCacheManager#create(String, CipherFactory)} to
     * instantiate.
     *
     * @param dirScope The directory scope managing storage and tasks for this cache.
     * @param cipherFactory The {@link CipherFactory} used to encrypt incognito tab state files.
     */
    /* package */ TabCache(TabCacheDirScope dirScope, @Nullable CipherFactory cipherFactory) {
        assertOnUiThread();
        mDirScope = dirScope;
        mCipherFactory = cipherFactory;
    }

    /** Returns the tag used for this cache instance. */
    public String getTag() {
        return mDirScope.getTag();
    }

    /** Returns the directory scope associated with this cache instance. */
    /* package */ TabCacheDirScope getDirScope() {
        return mDirScope;
    }

    /**
     * Saves the tab state for a given cache key.
     *
     * @param key The {@link TabCacheKey} to save the tab state for.
     * @param tab The tab whose state is being saved.
     */
    public void saveTab(TabCacheKey key, Tab tab) {
        assertOnUiThread();
        saveTabState(key, tab.getId(), TabStateExtractor.from(tab));
    }

    /**
     * Saves the tab state for a given cache key.
     *
     * @param key The {@link TabCacheKey} to save the tab state for.
     * @param tabId The ID of the tab whose state is being saved.
     * @param tabState The state of the tab being saved.
     */
    public void saveTabState(TabCacheKey key, @TabId int tabId, @Nullable TabState tabState) {
        assertOnUiThread();
        cancelTask(key);

        boolean isOffTheRecord = key.isIncognito();
        assert !isOffTheRecord || mCipherFactory != null;

        String fileName = key.getFileName();
        if (tabState == null || tabState.contentsState == null) {
            mDirScope.deleteFileAndPref(fileName);
            return;
        }

        mDirScope.getSharedPreferences().edit().putInt(fileName, tabId).apply();
        int currClearCount = mDirScope.getClearCounter().get();
        mDirScope
                .getTaskRunner()
                .execute(
                        () -> {
                            if (currClearCount != mDirScope.getClearCounter().get()) return;

                            File file = new File(mDirScope.getOrCreateCacheDirectory(), fileName);
                            TabStateFileManager.saveStateInternal(
                                    file, tabState, isOffTheRecord, mCipherFactory);
                        });
    }

    /**
     * Preloads the tab state for a given key on a background thread.
     *
     * @param key The key to preload.
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
        mDirScope.getTaskRunner().execute(task);
    }

    /**
     * Gets the preloaded tab state for a given key, or loads it immediately if not preloaded.
     *
     * @param key The key to get or load.
     * @return The loaded tab state, or null if loading failed.
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
        File file = new File(mDirScope.getOrCreateCacheDirectory(), fileName);
        if (!file.exists()) return null;

        @TabId int tabId = mDirScope.getSharedPreferences().getInt(fileName, INVALID_TAB_ID);
        if (tabId == INVALID_TAB_ID) return null;

        TabState tabState =
                TabStateFileManager.restoreTabStateInternal(
                        file, /* isEncrypted= */ incognito, mCipherFactory);
        if (tabState == null) return null;

        return new LoadedTabState(tabId, tabState);
    }

    /**
     * Clears the tab cache for the given key.
     *
     * @param key The key to clear.
     */
    public void clear(TabCacheKey key) {
        assertOnUiThread();
        cancelTask(key);
        mDirScope.deleteFileAndPref(key.getFileName());
    }

    /** Clears all pending preloading tasks. */
    public void cancelAllTasks() {
        assertOnUiThread();
        for (FutureTask<@Nullable LoadedTabState> task : mPreloadTasks.values()) {
            task.cancel(/* mayInterruptIfRunning= */ false);
        }
        mPreloadTasks.clear();
    }

    private void cancelTask(TabCacheKey key) {
        FutureTask<@Nullable LoadedTabState> task = mPreloadTasks.remove(key);
        if (task != null) {
            task.cancel(/* mayInterruptIfRunning= */ false);
        }
    }

    /** Clears all cached tabs and files for this TabCache instance. */
    public void clearAll() {
        assertOnUiThread();
        cancelAllTasks();
        mDirScope.clearAll();
    }

    /**
     * Static cleanup for a specific cache key.
     *
     * @param tag The tag used as the cache directory and SharedPreferences name.
     * @param key The cache key to clean up.
     */
    public static void cleanup(String tag, TabCacheKey key) {
        assertOnUiThread();
        TabCacheManager.getOrCreateDirScope(tag).deleteFileAndPref(key.getFileName());
    }

    /**
     * Clears all tab cache state for the given tag.
     *
     * @param tag The tag used as the cache directory and SharedPreferences name.
     */
    public static void clearGlobalState(String tag) {
        assertOnUiThread();
        TabCacheManager.getOrCreateDirScope(tag).clearAll();
    }
}
