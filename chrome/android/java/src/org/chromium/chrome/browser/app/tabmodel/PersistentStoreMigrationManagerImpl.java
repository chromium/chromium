// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_SHADOW_WRITTEN_STORE;
import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.allowFullMigration;
import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.isStorageAuthoritative;
import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.isTabStorageEnabled;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;

@NullMarked
public class PersistentStoreMigrationManagerImpl implements PersistentStoreMigrationManager {
    private final String mCurrentAuthoritativeStoreKey;
    private final String mShadowWrittenStoreKey;
    private @StoreType int mShadowStoreType = StoreType.INVALID;

    public PersistentStoreMigrationManagerImpl(String windowTag) {
        mCurrentAuthoritativeStoreKey =
                TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE.createKey(windowTag);
        mShadowWrittenStoreKey = TAB_PERSISTENCE_SHADOW_WRITTEN_STORE.createKey(windowTag);
    }

    @Override
    public @StoreType int getAuthoritativeStoreType() {
        if (!isTabStorageEnabled()) return StoreType.LEGACY;
        if (getShadowWrittenStore() == StoreType.LEGACY && !isStorageAuthoritative()) {
            return StoreType.LEGACY;
        }
        @StoreType
        int currentAuthoritativeStore =
                getPrefs().readInt(mCurrentAuthoritativeStoreKey, StoreType.INVALID);
        if (currentAuthoritativeStore != StoreType.INVALID) return currentAuthoritativeStore;
        if (isStorageAuthoritative()) return StoreType.TAB_STATE_STORE;
        return StoreType.LEGACY;
    }

    @Override
    public @StoreType int getShadowStoreType() {
        @StoreType int currentAuthoritativeStoreType = getAuthoritativeStoreType();
        if (isTabStorageEnabled() && currentAuthoritativeStoreType == StoreType.LEGACY) {
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
        if (shadowWrittenStore == StoreType.INVALID) {
            shadowWrittenStore = mShadowStoreType;
            if (!maybePerformMigrationSwap(currentAuthoritativeStoreType, shadowWrittenStore)) {
                if (shadowWrittenStore == StoreType.TAB_STATE_STORE) {
                    RecordHistogram.recordBooleanHistogram(
                            "Tabs.TabStateStore.ShadowStoreCaughtUp", true);
                }
                setShadowWrittenStore(shadowWrittenStore);
                return;
            }
        }
        maybePerformMigrationSwap(currentAuthoritativeStoreType, shadowWrittenStore);
    }

    @Override
    public boolean isShadowStoreCaughtUp() {
        return getShadowWrittenStore() != StoreType.INVALID;
    }

    @Override
    public void onShadowStoreRazed() {
        getPrefs().removeKey(mShadowWrittenStoreKey);
    }

    @Override
    public void onAllStoresRazed() {
        getPrefs().removeKeysWithPrefix(TAB_PERSISTENCE_SHADOW_WRITTEN_STORE);
        getPrefs().removeKeysWithPrefix(TAB_PERSISTENCE_CURRENT_AUTHORITATIVE_STORE);
    }

    @Override
    public void onWindowCleared() {
        getPrefs().removeKey(mCurrentAuthoritativeStoreKey);
        getPrefs().removeKey(mShadowWrittenStoreKey);
    }

    private boolean maybePerformMigrationSwap(
            @StoreType int currentAuthoritativeStoreType, @StoreType int shadowWrittenStoreType) {
        boolean storageAuthoritative = isStorageAuthoritative();
        boolean isAlreadyLegacy =
                !storageAuthoritative || currentAuthoritativeStoreType != StoreType.LEGACY;
        boolean isAlreadyTabStateStore =
                storageAuthoritative || currentAuthoritativeStoreType != StoreType.TAB_STATE_STORE;
        if (isAlreadyLegacy && isAlreadyTabStateStore) {
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
        return getPrefs().readInt(mShadowWrittenStoreKey, StoreType.INVALID);
    }

    private void setShadowWrittenStore(@StoreType int storeType) {
        getPrefs().writeInt(mShadowWrittenStoreKey, storeType);
    }

    private static SharedPreferencesManager getPrefs() {
        return ChromeSharedPreferences.getInstance();
    }
}
