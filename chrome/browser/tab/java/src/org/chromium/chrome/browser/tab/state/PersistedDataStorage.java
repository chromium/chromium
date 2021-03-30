// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.base.Callback;

/**
 * Provides key -> byte[] array mapping storage with namespace support for PersistedData
 */
public interface PersistedDataStorage {
    /**
     * Save a byte array corresponding to a key
     * @param key identifier in the database
     * @param data to store
     */
    void save(String key, byte[] data);

    /**
     * Asynchronous restore byte array corresponding to a key
     * @param key identifier in the database
     * @param callback to pass the data back in
     */
    void load(String key, Callback<byte[]> callback);

    /**
     * Delete data corresponding to a key
     * @param key identifier in the database
     */
    void delete(String key);
}
