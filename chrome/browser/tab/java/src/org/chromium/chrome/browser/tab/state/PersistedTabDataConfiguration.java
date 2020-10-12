// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.VisibleForTesting;

import java.util.HashMap;
import java.util.Map;

/**
 * Contains configuration values such as data storage methods and unique identifiers
 * for {@link PersistedTabData}
 */
public enum PersistedTabDataConfiguration {
    // TODO(crbug.com/1059650) investigate should this go in the app code?
    // Also investigate if the storage instance should be shared.
    CRITICAL_PERSISTED_TAB_DATA("CPTD", new FilePersistedTabDataStorage()),
    ENCRYPTED_CRITICAL_PERSISTED_TAB_DATA("ECPTD", new EncryptedFilePersistedTabDataStorage()),
    MOCK_PERSISTED_TAB_DATA("MPTD", new FilePersistedTabDataStorage()),
    ENCRYPTED_MOCK_PERSISTED_TAB_DATA("EMPTD", new EncryptedFilePersistedTabDataStorage()),
    // TODO(crbug.com/1129626) Move Shopping to Level DB based storage
    SHOPPING_PERSISTED_TAB_DATA("SPTD", new FilePersistedTabDataStorage()),
    // TODO(crbug.com/1113828) investigate separating test from prod test implementations
    TEST_CONFIG("TC", new MockPersistedTabDataStorage());

    private static final Map<Class<? extends PersistedTabData>, PersistedTabDataConfiguration>
            sLookup = new HashMap<>();
    private static final Map<Class<? extends PersistedTabData>, PersistedTabDataConfiguration>
            sEncryptedLookup = new HashMap<>();

    private static boolean sUseTestConfig;

    static {
        // TODO(crbug.com/1060187) remove static initializer and initialization lazy
        sLookup.put(CriticalPersistedTabData.class, CRITICAL_PERSISTED_TAB_DATA);
        sEncryptedLookup.put(CriticalPersistedTabData.class, ENCRYPTED_CRITICAL_PERSISTED_TAB_DATA);
        sLookup.put(MockPersistedTabData.class, MOCK_PERSISTED_TAB_DATA);
        sEncryptedLookup.put(MockPersistedTabData.class, ENCRYPTED_MOCK_PERSISTED_TAB_DATA);
        sLookup.put(ShoppingPersistedTabData.class, SHOPPING_PERSISTED_TAB_DATA);
        sEncryptedLookup.put(ShoppingPersistedTabData.class, SHOPPING_PERSISTED_TAB_DATA);
    }

    public final String id;
    public final PersistedTabDataStorage storage;

    /**
     * @param id identifier for {@link PersistedTabData}
     * @param storage {@link PersistedTabDataStorage} associated with {@link PersistedTabData}
     */
    PersistedTabDataConfiguration(String id, PersistedTabDataStorage storage) {
        this.id = id;
        this.storage = storage;
    }

    /**
     * Acquire {@link PersistedTabDataConfiguration} for a given {@link PersistedTabData} class
     */
    public static PersistedTabDataConfiguration get(
            Class<? extends PersistedTabData> clazz, boolean isEncrypted) {
        if (sUseTestConfig) {
            return TEST_CONFIG;
        }
        if (isEncrypted) {
            return sEncryptedLookup.get(clazz);
        }
        return sLookup.get(clazz);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void setUseTestConfig(boolean useTestConfig) {
        sUseTestConfig = useTestConfig;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected static PersistedTabDataConfiguration getTestConfig() {
        return TEST_CONFIG;
    }
}
