// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Manages the migration of tab persistence logic between {@link TabPersistentStore}s. */
@NullMarked
public interface PersistentStoreMigrationManager {
    @IntDef({
        StoreType.INVALID,
        StoreType.LEGACY,
        StoreType.TAB_STATE_STORE,
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface StoreType {
        // Represents an invalid TabPersistentStore type.
        int INVALID = 0;
        // A metadata file-backed implementation of TabPersistentStore.
        int LEGACY = 1;
        // An SQLITE-backed implementation of TabPersistentStore.
        int TAB_STATE_STORE = 2;
    }

    /** Returns the {@link StoreType} that is considered the authoritative source of truth. */
    @StoreType
    int getAuthoritativeStoreType();

    /**
     * Returns the {@link StoreType} that serves as a shadow store. This store is used for
     * dual-writes during migration or verification and is not the primary source of truth.
     *
     * <p>Returns {@link StoreType#INVALID} if no shadow store is needed.
     */
    @StoreType
    int getShadowStoreType();
}
