// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.isStorageAuthoritative;
import static org.chromium.chrome.browser.tab.TabStateStorageFlagHelper.isTabStorageEnabled;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;

/** A migration manager which always assumes no migration is required. */
@NullMarked
public class DefaultPersistentStoreMigrationManager implements PersistentStoreMigrationManager {
    private final String mWindowTag;

    public DefaultPersistentStoreMigrationManager(String windowTag) {
        mWindowTag = windowTag;
    }

    @Override
    public @StoreType int getAuthoritativeStoreType() {
        return (isTabStorageEnabled() && isStorageAuthoritative())
                ? StoreType.TAB_STATE_STORE
                : StoreType.LEGACY;
    }

    @Override
    public @StoreType int getShadowStoreType() {
        return (isTabStorageEnabled() && !isStorageAuthoritative())
                ? StoreType.TAB_STATE_STORE
                : StoreType.INVALID;
    }

    @Override
    public void onShadowStoreCreated(@StoreType int storeType) {}

    @Override
    public void onShadowStoreCaughtUp() {}

    @Override
    public void onAuthoritativeStoreInitialized(@StoreType int type) {
        getManager().onAuthoritativeStoreInitialized(type);
    }

    @Override
    public boolean isShadowStoreCaughtUp() {
        return true;
    }

    @Override
    public boolean shouldRazeStoreForWindow(boolean isAuthoritative) {
        if (!isAuthoritative) return true;
        return getManager().shouldRazeStoreForWindow(/* isAuthoritative= */ true);
    }

    @Override
    public void onShadowStoreRazed() {}

    @Override
    public void onAllStoresRazed() {}

    @Override
    public void onWindowCleared() {
        getManager().onWindowCleared();
    }

    /**
     * Instantiates a {@link PersistentStoreMigrationManagerImpl} to handle non-shadow store-related
     * state. Since this state is persisted to the disk and essentially static, there is no major
     * benefit to holding a long-lived object in memory instead of instantiation on demand.
     */
    private PersistentStoreMigrationManagerImpl getManager() {
        return new PersistentStoreMigrationManagerImpl(mWindowTag);
    }
}
