// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

/**
 * Creates a {@link PersistedTabDataStorage}
 * @param <T> {@link PersistedTabDataStorage} being created
 */
public interface PersistedTabDataStorageFactory<T extends PersistedTabDataStorage> {
    /**
     * @return {@link PersistedTabDataStorage}
     */
    T create();
}
