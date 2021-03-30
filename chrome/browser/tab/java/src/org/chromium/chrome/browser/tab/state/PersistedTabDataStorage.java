// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;

/**
 * Storage for {@link PersistedTabData}
 */
public interface PersistedTabDataStorage {
    /**
     * @param tabId identifier for the {@link Tab}
     * @param tabDataId unique identifier representing the type {@link PersistedTabData}
     * @param dataSupplier {@link Supplier} for serialized {@link PersistedTabData}
     */
    void save(int tabId, String tabDataId, Supplier<byte[]> dataSupplier);

    /**
     * @param tabId identifier for the {@link Tab}
     * @param tabDataId unique identifier representing the type of {@link PersistedTabData}
     * @param callback to pass back the seraizliaed {@link PersistedTabData} in
     */
    void restore(int tabId, String tabDataId, Callback<byte[]> callback);

    /**
     * @param tabId identifier for the {@link Tab}
     * @param tabDataId unique identifier representing the type of {@link PersistedTabData}
     * @return serialized {@link PersitsedTabData}
     */
    byte[] restore(int tabId, String tabDataId);

    /**
     * @param tabId identifier for the {@link Tab}
     * @param tabDataId unique identifier representing the type of {@link PersistedTabData}
     */
    void delete(int tabId, String tabDataId);

    /**
     * @return unique tag appended to the end of metrics for Uma
     */
    String getUmaTag();
}
