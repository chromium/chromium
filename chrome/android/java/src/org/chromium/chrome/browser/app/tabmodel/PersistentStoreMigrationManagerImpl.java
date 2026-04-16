// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_SHADOW_WRITTEN_STORE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_STORE_MANAGER_VERSION;
import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.allowFullMigration;
import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.fullRollback;
import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.isStorageAuthoritative;
import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.isTabStorageEnabled;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.KeyPrefix;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;

import java.util.Map;

@NullMarked
public class PersistentStoreMigrationManagerImpl implements PersistentStoreMigrationManager {
    /**
     * Incrementing this value will reset all manager prefs and forces all windows to be reset to a
     * default state.
     *
     * <p>This can be potentially destructive if users have any non-legacy authoritative stores.
     */
    public static final int MANAGER_VERSION = 1;

    private final String mCurrentAuthoritativeStoreKey;
    private final String mShadowWrittenStoreKey;
    private @StoreType int mShadowStoreType = StoreType.INVALID;

    /**
     * @param windowTag The tag of the window to manage state for.
     */
    public PersistentStoreMigrationManagerImpl(String windowTag) {
        mCurrentAuthoritativeStoreKey =
                TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE.createKey(windowTag);
        mShadowWrittenStoreKey = TAB_PERSISTENCE_SHADOW_WRITTEN_STORE.createKey(windowTag);

        clearPrefsIfVersionChanged();
    }

    @Override
    public @StoreType int getAuthoritativeStoreType() {
        if (!isTabStorageEnabled()) return StoreType.LEGACY;
        @StoreType int shadowWrittenStore = getShadowWrittenStore();
        if (shadowWrittenStore == StoreType.LEGACY && !isStorageAuthoritative()) {
            return StoreType.LEGACY;
        }
        @StoreType
        int currentAuthoritativeStore =
                getPrefs().readInt(mCurrentAuthoritativeStoreKey, StoreType.INVALID);
        if (currentAuthoritativeStore == StoreType.INVALID) {
            return StoreType.LEGACY;
        } else if (currentAuthoritativeStore != StoreType.UNKNOWN) {
            return currentAuthoritativeStore;
        } else if (isStorageAuthoritative()) {
            return StoreType.TAB_STATE_STORE;
        }
        return StoreType.LEGACY;
    }

    @Override
    public @StoreType int getShadowStoreType() {
        @StoreType int currentAuthoritativeStoreType = getAuthoritativeStoreType();
        if (isTabStorageEnabled()
                && !fullRollback()
                && currentAuthoritativeStoreType == StoreType.LEGACY) {
            return StoreType.TAB_STATE_STORE;
        } else if (!allowFullMigration()
                && currentAuthoritativeStoreType == StoreType.TAB_STATE_STORE) {
            return StoreType.LEGACY;
        }
        // Raze the shadow store since we will no longer be in a 'caught up' state.
        onShadowStoreRazed();
        return StoreType.INVALID;
    }

    @Override
    public void onShadowStoreCreated(@StoreType int storeType) {
        assert storeType != StoreType.INVALID;
        mShadowStoreType = storeType;
    }

    @Override
    public void onShadowStoreCaughtUp() {
        assert mShadowStoreType != StoreType.INVALID;
        @StoreType int shadowWrittenStore = getShadowWrittenStore();
        @StoreType int currentAuthoritativeStoreType = getAuthoritativeStoreType();
        if (shadowWrittenStore == StoreType.INVALID || shadowWrittenStore == StoreType.UNKNOWN) {
            shadowWrittenStore = mShadowStoreType;
            if (!maybePerformMigrationSwap(currentAuthoritativeStoreType, shadowWrittenStore)) {
                if (shadowWrittenStore == StoreType.TAB_STATE_STORE) {
                    RecordHistogram.recordBooleanHistogram(
                            "Tabs.TabStateStore.ShadowStoreCaughtUp", true);
                }
                // This handles the case where we do not want to perform a migration swap and only
                // want to catch up.
                setShadowWrittenStore(shadowWrittenStore);
                return;
            }
        }
        maybePerformMigrationSwap(currentAuthoritativeStoreType, shadowWrittenStore);
    }

    @Override
    public void onAuthoritativeStoreInitialized(@StoreType int type) {
        setCurrentAuthoritativeStore(type);
    }

    @Override
    public boolean isShadowStoreCaughtUp() {
        @StoreType int shadowWrittenStore = getShadowWrittenStore();
        return shadowWrittenStore != StoreType.INVALID && shadowWrittenStore != StoreType.UNKNOWN;
    }

    @Override
    public boolean shouldRazeStoreForWindow(boolean isAuthoritative) {
        String key = isAuthoritative ? mCurrentAuthoritativeStoreKey : mShadowWrittenStoreKey;
        return getPrefs().readInt(key, StoreType.INVALID) == StoreType.UNKNOWN;
    }

    @Override
    public void onShadowStoreRazed() {
        getPrefs().writeInt(mShadowWrittenStoreKey, StoreType.UNKNOWN);
    }

    @Override
    public void onAllStoresRazed() {
        markAllKeysUnknownForPrefix(TAB_PERSISTENCE_SHADOW_WRITTEN_STORE);
        markAllKeysUnknownForPrefix(TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE);
    }

    @Override
    public void onWindowCleared() {
        getPrefs().writeInt(mCurrentAuthoritativeStoreKey, StoreType.UNKNOWN);
        getPrefs().writeInt(mShadowWrittenStoreKey, StoreType.UNKNOWN);
    }

    private boolean maybePerformMigrationSwap(
            @StoreType int currentAuthoritativeStoreType, @StoreType int shadowWrittenStoreType) {
        boolean storageAuthoritative = isStorageAuthoritative();
        boolean isCorrectForAuthoritative =
                storageAuthoritative && currentAuthoritativeStoreType == StoreType.TAB_STATE_STORE;
        boolean isCorrectForNonAuthoritative =
                !storageAuthoritative && currentAuthoritativeStoreType == StoreType.LEGACY;

        if (isCorrectForAuthoritative || isCorrectForNonAuthoritative) {
            return false;
        }

        setCurrentAuthoritativeStore(shadowWrittenStoreType);
        setShadowWrittenStore(currentAuthoritativeStoreType);
        return true;
    }

    private void setCurrentAuthoritativeStore(@StoreType int storeType) {
        getPrefs().writeInt(mCurrentAuthoritativeStoreKey, storeType);
    }

    private @StoreType int getShadowWrittenStore() {
        return getPrefs().readInt(mShadowWrittenStoreKey, StoreType.UNKNOWN);
    }

    private void setShadowWrittenStore(@StoreType int storeType) {
        getPrefs().writeInt(mShadowWrittenStoreKey, storeType);
    }

    private static SharedPreferencesManager getPrefs() {
        return ChromeSharedPreferences.getInstance();
    }

    private void markAllKeysUnknownForPrefix(KeyPrefix prefix) {
        SharedPreferencesManager prefs = getPrefs();
        Map<String, Integer> authoritativeStores = prefs.readIntsWithPrefix(prefix);
        for (Map.Entry<String, Integer> entry : authoritativeStores.entrySet()) {
            prefs.writeInt(entry.getKey(), StoreType.UNKNOWN);
        }
    }

    private void clearPrefsIfVersionChanged() {
        SharedPreferencesManager prefs = getPrefs();
        if (MANAGER_VERSION != prefs.readInt(TAB_PERSISTENCE_STORE_MANAGER_VERSION, 0)) {
            prefs.removeKeysWithPrefix(TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE);
            prefs.removeKeysWithPrefix(TAB_PERSISTENCE_SHADOW_WRITTEN_STORE);
            prefs.writeInt(TAB_PERSISTENCE_STORE_MANAGER_VERSION, MANAGER_VERSION);
        }
    }
}
