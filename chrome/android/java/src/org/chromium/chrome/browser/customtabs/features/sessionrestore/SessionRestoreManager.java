// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.sessionrestore;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Object used to store a tab object, for restoration.
 */
public interface SessionRestoreManager {
    /**
     * Observer that receives signal from SessionRestoreManager.
     */
    interface Observer {
        /**
         * Notified when the tab is evicted (i.e removed from cache), with the eviction reason.
         */
        void onCacheCleared(int reason);
    }

    /** Put the tab in storage. */
    boolean store(Tab tabToRestore);

    /**
     * Return the restored tab from previous session. Return null if the restoration failed.
     */
    @Nullable
    Tab restoreTab();

    /**
     * Check if there's a tab to restore. This function does not guarantee restoration will be
     * successful.
     */
    boolean canRestoreTab();

    /**
     * Set the eviction timeout for tab. If tab is not restored before eviction, it will be cleared
     * from the cache.
     * @param timeoutMs Timeout for tab to get evicted from cache.
     */
    void setEvictionTimeout(long timeoutMs);

    /**
     * Cancel the eviction timeout started during {@link #store}.
     */
    void cancelEvictionTimer();

    /** Clean the stored tab, if any. */
    void clearCache();

    void addObserver(Observer observer);

    void removeObserver(Observer observer);
}
