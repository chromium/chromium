// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.UpdateDelta;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Keeps track of offline-available content addition and removal from Chrome. This information is
 * used to decide whether or not users should see messages in the UI about offline content
 * availability in Chrome.
 */
public class OfflineContentAvailabilityStatusProvider implements OfflineContentProvider.Observer {
    private static OfflineContentAvailabilityStatusProvider sInstance;

    // Keeps track of suggested content.
    private Set<ContentId> mSuggestedItems = new HashSet<>();
    // Keeps track of persistent content, i.e. non-transient content, including prefetch, downloads,
    // offline pages, etc. The idea is that this set will be empty iff Download Home would be empty.
    private Set<ContentId> mPersistentItems = new HashSet<>();

    /**
     * @return An {@link OfflineContentAvailabilityStatusProvider} instance singleton.  If one
     *         is not available this will create a new one.
     */
    public static OfflineContentAvailabilityStatusProvider getInstance() {
        if (sInstance == null) {
            sInstance = new OfflineContentAvailabilityStatusProvider();
            OfflineContentProvider provider = OfflineContentAggregatorFactory.get();
            provider.addObserver(sInstance);
            provider.getAllItems(sInstance::onItemsAdded);
        }
        return sInstance;
    }

    @VisibleForTesting
    OfflineContentAvailabilityStatusProvider() {}

    /**
     * @return Whether or not there is any suggested offline content available in Chrome.
     */
    public boolean isSuggestedContentAvailable() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(
                        ChromePreferenceKeys.EXPLORE_OFFLINE_CONTENT_AVAILABILITY_STATUS, false);
    }

    /**
     * @return Whether or not there is any persistent (i.e. non-transient) offline content
     *         available in Chrome.
     */
    public boolean isPersistentContentAvailable() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(
                        ChromePreferenceKeys.PERSISTENT_OFFLINE_CONTENT_AVAILABILITY_STATUS, false);
    }

    // OfflineContentProvider.Observer overrides

    @Override
    public void onItemsAdded(List<OfflineItem> items) {
        if (items.isEmpty()) return;

        for (OfflineItem item : items) {
            if (item.isSuggested) mSuggestedItems.add(item.id);
            if (!item.isTransient) mPersistentItems.add(item.id);
        }
        updateSharedPrefs();
    }

    @Override
    public void onItemRemoved(ContentId id) {
        boolean prefetch_removed = mSuggestedItems.remove(id);
        boolean persistent_removed = mPersistentItems.remove(id);
        if (prefetch_removed || persistent_removed) updateSharedPrefs();
    }

    @Override
    public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {}

    private void updateSharedPrefs() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.EXPLORE_OFFLINE_CONTENT_AVAILABILITY_STATUS,
                        !mSuggestedItems.isEmpty());
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.PERSISTENT_OFFLINE_CONTENT_AVAILABILITY_STATUS,
                        !mPersistentItems.isEmpty());
    }
}
