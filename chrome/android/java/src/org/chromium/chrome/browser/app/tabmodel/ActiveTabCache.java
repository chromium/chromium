// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.base.ThreadUtils.assertOnUiThread;
import static org.chromium.chrome.browser.tabpersistence.TabStateFileManager.FLATBUFFER_PREFIX;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;

import java.io.File;

/**
 * Responsible for caching the active tab's state. This allows for loading the active tab's state
 * pre-native, which the database is unable to do.
 */
@NullMarked
public class ActiveTabCache {
    private static final String TAG = "active_tab_cache";

    /** The name of the base directory where the state is saved. */
    private static final String CACHE_DIR_NAME = "active_tabs";

    private static final String REGULAR_SUFFIX = "_regular";
    private static final String INCOGNITO_SUFFIX = "_incognito";
    private static @Nullable File sActiveTabDirectory;

    public Callback<@Nullable Tab> mOnRegularActiveTabChanged =
            tab -> onActiveTabChanged(/* isModelOtr= */ false, tab);
    public Callback<@Nullable Tab> mOnIncognitoActiveTabChanged =
            tab -> onActiveTabChanged(/* isModelOtr= */ true, tab);

    private final TabModelSelector mTabModelSelector;
    private final @Nullable CipherFactory mCipherFactory;

    private final String mRegularTabFileName;
    private final String mIncognitoTabFileName;

    /**
     * @param windowTag The tag for the window being tracked.
     */
    public ActiveTabCache(
            String windowTag, TabModelSelector selector, @Nullable CipherFactory cipherFactory) {
        mTabModelSelector = selector;
        mCipherFactory = cipherFactory;

        mRegularTabFileName = getFileName(windowTag, /* incognito= */ false);
        mIncognitoTabFileName = getFileName(windowTag, /* incognito= */ true);

        if (cipherFactory == null) {
            clearActiveTab(/* incognito= */ true);
        }
    }

    /**
     * Saves the active tab's state to the cache.
     *
     * <p>Note that there is one file per window/otr-status combination, so we atomically "swap" the
     * active tab each time it is updated.
     *
     * @param tab The active tab.
     * @param cipherFactory The cipher factory for encrypting incognito tab state.
     */
    public void saveActiveTab(Tab tab, @Nullable CipherFactory cipherFactory) {
        assertOnUiThread();

        boolean isOffTheRecord = tab.isOffTheRecord();
        assert !isOffTheRecord || cipherFactory != null;

        String fileName = isOffTheRecord ? mIncognitoTabFileName : mRegularTabFileName;
        File file = new File(getOrCreateCacheDirectory(), fileName);
        TabState tabState = TabStateExtractor.from(tab);
        if (tabState == null) {
            return;
        }

        TabStateFileManager.saveStateInternal(file, tabState, isOffTheRecord, cipherFactory);
    }

    /**
     * Restores the active tab from the cache. If it doesn't exist or failed to restore, return
     * null.
     *
     * @param isOffTheRecord Whether to restore the incognito active tab.
     * @param cipherFactory The cipher factory for decrypting incognito tab state.
     */
    public @Nullable TabState restoreActiveTab(
            boolean isOffTheRecord, @Nullable CipherFactory cipherFactory) {
        assertOnUiThread();
        assert !isOffTheRecord || cipherFactory != null;

        String fileName = isOffTheRecord ? mIncognitoTabFileName : mRegularTabFileName;
        File file = new File(getOrCreateCacheDirectory(), fileName);
        if (!file.exists()) return null;

        return TabStateFileManager.restoreTabStateInternal(file, isOffTheRecord, cipherFactory);
    }

    public void startTracking(boolean incognito) {
        TabModel model = mTabModelSelector.getModel(incognito);

        NullableObservableSupplier<Tab> currentTabSupplier = model.getCurrentTabSupplier();
        Callback<@Nullable Tab> onActiveTabChanged = getActiveTabChangedCallback(incognito);

        currentTabSupplier.addSyncObserver(onActiveTabChanged);
        onActiveTabChanged.onResult(currentTabSupplier.get());
    }

    public void stopTracking(boolean incognito) {
        TabModel model = mTabModelSelector.getModel(incognito);
        NullableObservableSupplier<Tab> currentTabSupplier = model.getCurrentTabSupplier();
        currentTabSupplier.removeObserver(getActiveTabChangedCallback(incognito));
    }

    /**
     * Clears the active tab cache for the given incognito state.
     *
     * @param incognito Whether to clear the incognito or regular active tab.
     */
    public void clearActiveTab(boolean incognito) {
        String fileName = incognito ? mIncognitoTabFileName : mRegularTabFileName;
        deleteFile(fileName);
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

        deleteFile(regularFileName);
        deleteFile(incognitoFileName);
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

    private static File getCacheDirectory() {
        return ContextUtils.getApplicationContext().getDir(CACHE_DIR_NAME, Context.MODE_PRIVATE);
    }

    private static String getFileName(String windowTag, boolean incognito) {
        String suffix = incognito ? INCOGNITO_SUFFIX : REGULAR_SUFFIX;
        // This prefix is required to ensure FlatBuffer is used during serialization and
        // deserialization.
        return FLATBUFFER_PREFIX + windowTag + suffix;
    }

    private static void deleteFile(String fileName) {
        assertOnUiThread();
        File file = new File(getCacheDirectory(), fileName);
        if (file.exists() && !file.delete()) {
            Log.e(TAG, "Failed to delete cache file: " + file);
        }
    }

    private void onActiveTabChanged(boolean isModelOtr, @Nullable Tab tab) {
        if (tab == null) {
            clearActiveTab(isModelOtr);
        } else {
            boolean isOffTheRecord = tab.isOffTheRecord();
            assert isModelOtr == isOffTheRecord;
            saveActiveTab(tab, isModelOtr ? mCipherFactory : null);
        }
    }

    private Callback<@Nullable Tab> getActiveTabChangedCallback(boolean incognito) {
        return incognito ? mOnIncognitoActiveTabChanged : mOnRegularActiveTabChanged;
    }
}
