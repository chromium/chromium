// Copyright 2020 The Chromium Authors
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
    CRITICAL_PERSISTED_TAB_DATA("CPTDFB"),
    ENCRYPTED_CRITICAL_PERSISTED_TAB_DATA("ECPTDFB"),
    MOCK_PERSISTED_TAB_DATA("MPTD"),
    ENCRYPTED_MOCK_PERSISTED_TAB_DATA("EMPTD"),
    SHOPPING_PERSISTED_TAB_DATA("SPTD"),
    EMPTY_BYTE_BUFFER_TEST_CONFIG("EBBTC"),
    // TODO(crbug.com/1113828) investigate separating test from prod test implementations
    TEST_CONFIG("TC");

    private static final Map<Class<? extends PersistedTabData>, PersistedTabDataConfiguration>
            sLookup = new HashMap<>();
    private static final Map<Class<? extends PersistedTabData>, PersistedTabDataConfiguration>
            sEncryptedLookup = new HashMap<>();

    /**
     * Ensure lazy initialization of singleton storage
     */
    private static FilePersistedTabDataStorage sFilePersistedTabDataStorage;
    private static EncryptedFilePersistedTabDataStorage sEncrpytedFilePersistedTabDataStorage;
    private static MockPersistedTabDataStorage sMockPersistedTabDataStorage;
    private static EmptyByteBufferPersistedTabDataStorage sEmptyByteBufferPersistedTabDataStorage;
    private static boolean sUseEmptyByteBufferTestConfig;

    private static EmptyByteBufferPersistedTabDataStorage
    getEmptyByteBufferPersistedTabDataStorage() {
        if (sEmptyByteBufferPersistedTabDataStorage == null) {
            sEmptyByteBufferPersistedTabDataStorage = new EmptyByteBufferPersistedTabDataStorage();
        }
        return sEmptyByteBufferPersistedTabDataStorage;
    }

    static FilePersistedTabDataStorage getFilePersistedTabDataStorage() {
        if (sFilePersistedTabDataStorage == null) {
            sFilePersistedTabDataStorage = new FilePersistedTabDataStorage();
        }
        return sFilePersistedTabDataStorage;
    }

    static EncryptedFilePersistedTabDataStorage getEncryptedFilePersistedTabDataStorage() {
        if (sEncrpytedFilePersistedTabDataStorage == null) {
            sEncrpytedFilePersistedTabDataStorage = new EncryptedFilePersistedTabDataStorage();
        }
        return sEncrpytedFilePersistedTabDataStorage;
    }

    private static MockPersistedTabDataStorage getMockPersistedTabDataStorage() {
        if (sMockPersistedTabDataStorage == null) {
            sMockPersistedTabDataStorage = new MockPersistedTabDataStorage();
        }
        return sMockPersistedTabDataStorage;
    }

    private static boolean sUseTestConfig;

    static {
        // TODO(crbug.com/1060187) remove static initializer and initialization lazy
        sLookup.put(CriticalPersistedTabData.class, CRITICAL_PERSISTED_TAB_DATA);
        sEncryptedLookup.put(CriticalPersistedTabData.class, ENCRYPTED_CRITICAL_PERSISTED_TAB_DATA);
        sLookup.put(MockPersistedTabData.class, MOCK_PERSISTED_TAB_DATA);
        sEncryptedLookup.put(MockPersistedTabData.class, ENCRYPTED_MOCK_PERSISTED_TAB_DATA);
        sLookup.put(ShoppingPersistedTabData.class, SHOPPING_PERSISTED_TAB_DATA);
        sEncryptedLookup.put(ShoppingPersistedTabData.class, SHOPPING_PERSISTED_TAB_DATA);

        CRITICAL_PERSISTED_TAB_DATA.mStorageFactory = () -> {
            return getFilePersistedTabDataStorage();
        };
        ENCRYPTED_CRITICAL_PERSISTED_TAB_DATA.mStorageFactory = () -> {
            return getEncryptedFilePersistedTabDataStorage();
        };
        MOCK_PERSISTED_TAB_DATA.mStorageFactory = () -> {
            return getFilePersistedTabDataStorage();
        };
        ENCRYPTED_MOCK_PERSISTED_TAB_DATA.mStorageFactory = () -> {
            return getEncryptedFilePersistedTabDataStorage();
        };
        SHOPPING_PERSISTED_TAB_DATA.mStorageFactory = new LevelDBPersistedTabDataStorageFactory();

        TEST_CONFIG.mStorageFactory = () -> {
            return getMockPersistedTabDataStorage();
        };

        EMPTY_BYTE_BUFFER_TEST_CONFIG.mStorageFactory = () -> {
            return getEmptyByteBufferPersistedTabDataStorage();
        };
    }

    private final String mId;
    private PersistedTabDataStorageFactory mStorageFactory;

    /**
     * @param id identifier for {@link PersistedTabData}
     * @param storageFactory {@link PersistedTabDataStorageFactory} associated with {@link
     *         PersistedTabData}
     */
    PersistedTabDataConfiguration(String id) {
        mId = id;
    }

    /**
     * @return {@link PersistedTabDataStorage} for a given configuration
     */
    public PersistedTabDataStorage getStorage() {
        return mStorageFactory.create();
    }

    /**
     * @return id for a given configuration
     */
    public String getId() {
        return mId;
    }

    /**
     * Acquire {@link PersistedTabDataConfiguration} for a given {@link PersistedTabData} class
     */
    public static PersistedTabDataConfiguration get(
            Class<? extends PersistedTabData> clazz, boolean isEncrypted) {
        if (sUseEmptyByteBufferTestConfig) {
            return EMPTY_BYTE_BUFFER_TEST_CONFIG;
        }
        if (sUseTestConfig) {
            return TEST_CONFIG;
        }
        if (isEncrypted) {
            return sEncryptedLookup.get(clazz);
        }
        return sLookup.get(clazz);
    }

    // TODO(crbug.com/1290977) merge test config options into an enum so there can be just one
    // setter).
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void setUseTestConfig(boolean useTestConfig) {
        sUseTestConfig = useTestConfig;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void setUseEmptyByteBufferTestConfig(boolean useEmptyByteBufferTestConfig) {
        sUseEmptyByteBufferTestConfig = useEmptyByteBufferTestConfig;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static PersistedTabDataConfiguration getTestConfig() {
        return TEST_CONFIG;
    }
}
