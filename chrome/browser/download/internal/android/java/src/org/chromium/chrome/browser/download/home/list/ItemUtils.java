// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.text.TextUtils;

import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/** Helper methods to make performing actions on {@link OfflineItem}s easier. */
class ItemUtils {
    private ItemUtils() {}

    /**
     * Finds all {@link OfflineItem}s in {@code allItems} that have the same file path as an
     * {@link OfflineItem} in {@code items}.  Note that {@link OfflineItem}s in {@code items} with
     * empty or {@code null} file paths are ignored in the search, but that {@link OfflineItem} is
     * still included in the returned {@link Collection}.
     *
     * @param items    A {@link Collection} of {@link OfflineItem}s to use as the source for file
     *                 paths.
     * @param allItems A {@link Collection} of {@link OfflineItem}s to search for matching file
     *                 paths in.
     * @return         All {@link OfflineItem}s in {@code allItems} that have file paths that match
     *                 an {@link OfflineItem} in {@code items}.  The values in {@code items} are
     *                 automatically included.
     */
    public static Collection<OfflineItem> findItemsWithSameFilePath(
            Collection<OfflineItem> items, Collection<OfflineItem> allItems) {
        Set<String> uniqueFilePaths = new HashSet<>();
        for (OfflineItem item : items) {
            if (!TextUtils.isEmpty(item.filePath)) uniqueFilePaths.add(item.filePath);
        }

        Set<OfflineItem> matchedItems = new HashSet<>(items);
        for (OfflineItem item : allItems) {
            if (uniqueFilePaths.contains(item.filePath)) matchedItems.add(item);
        }

        return matchedItems;
    }
}
