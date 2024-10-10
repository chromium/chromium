// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.os.Environment;

import org.chromium.base.ContentUriUtils;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.io.File;

/**
 * A {@link OfflineItemFilter} responsible for pruning out items that do not have the right state
 *  to show in the UI or have been externally deleted.
 */
public class InvalidStateOfflineItemFilter extends OfflineItemFilter {
    /** Creates an instance of this filter and wraps {@code source}. */
    public InvalidStateOfflineItemFilter(OfflineItemFilterSource source) {
        super(source);
        onFilterChanged();
    }

    // OfflineItemFilter implementation.
    @Override
    protected boolean isFilteredOut(OfflineItem item) {
        boolean inPrimaryDirectory =
                InvalidStateOfflineItemFilter.isInPrimaryStorageDownloadDirectory(item.filePath);
        if ((item.externallyRemoved && inPrimaryDirectory) || item.isTransient) return true;

        switch (item.state) {
            case OfflineItemState.CANCELLED:
            case OfflineItemState.FAILED:
                return true;
            case OfflineItemState.INTERRUPTED:
            default:
                return false;
        }
    }

    /**
     * Returns if the path is in the download directory on primary storage.
     * @param path The directory to check.
     * @return If the path is in the download directory on primary storage.
     */
    private static boolean isInPrimaryStorageDownloadDirectory(String path) {
        // Only primary storage can have content URI as file path.
        if (ContentUriUtils.isContentUri(path)) return true;

        // Check if the file path contains the external public directory.
        File primaryDir = Environment.getExternalStorageDirectory();
        if (primaryDir == null || path == null) return false;
        String primaryPath = primaryDir.getAbsolutePath();
        return primaryPath == null ? false : path.contains(primaryPath);
    }
}
