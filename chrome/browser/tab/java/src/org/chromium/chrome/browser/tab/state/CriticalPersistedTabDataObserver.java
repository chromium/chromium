// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Observe fields in CriticalPersistedTabData
 */
public interface CriticalPersistedTabDataObserver {
    /**
     * Broadcast that root identifier on a {@link Tab} has changed
     * @param tab {@link Tab} root identifier has changed on
     * @param newRootId new value of new root id
     */
    default void onRootIdChanged(Tab tab, int newRootId) {}

    /**
     * Broadcast that the timestamp on a {@link Tab} has changed
     * @param tab {@link Tab} timestamp has changed on
     * @param timestampMillis new value of the timestamp
     */
    default void onTimestampChanged(Tab tab, long timestampMillis) {}
}
