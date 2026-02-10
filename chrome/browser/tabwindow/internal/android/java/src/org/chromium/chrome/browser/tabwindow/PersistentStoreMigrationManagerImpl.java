// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabwindow;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.TAB_PERSISTENCE_SHADOW_WRITTEN_STORE;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.TabStateStorageFlagHelper;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;

@NullMarked
public class PersistentStoreMigrationManagerImpl implements PersistentStoreMigrationManager {
    private final String mShadowWrittenStoreKey;
    private @StoreType int mShadowStoreType = StoreType.INVALID;

    public PersistentStoreMigrationManagerImpl(String windowTag) {
        mShadowWrittenStoreKey = TAB_PERSISTENCE_SHADOW_WRITTEN_STORE.createKey(windowTag);
    }

    @Override
    public @StoreType int getAuthoritativeStoreType() {
        return StoreType.LEGACY;
    }

    @Override
    public @StoreType int getShadowStoreType() {
        return TabStateStorageFlagHelper.isTabStorageEnabled()
                ? StoreType.TAB_STATE_STORE
                : StoreType.INVALID;
    }

    @Override
    public void onShadowStoreCreated(@StoreType int storeType) {
        assert storeType != StoreType.INVALID;
        mShadowStoreType = storeType;
    }

    @Override
    public void onShadowStoreCaughtUp() {
        assert mShadowStoreType != StoreType.INVALID;
        setShadowWrittenStore(mShadowStoreType);
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
    public void onAllShadowStoresRazed() {
        getPrefs().removeKeysWithPrefix(TAB_PERSISTENCE_SHADOW_WRITTEN_STORE);
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
