// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabwindow;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.TabStateStorageFlagHelper;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;

@NullMarked
public class PersistentStoreMigrationManagerImpl implements PersistentStoreMigrationManager {
    @SuppressWarnings("unused")
    private final String mWindowTag;

    public PersistentStoreMigrationManagerImpl(String windowTag) {
        mWindowTag = windowTag;
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
}
