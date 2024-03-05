// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

import java.util.ArrayList;

/** Utility functions used in download dialogs. */
public class DownloadDialogUtils {
    // The threshold to determine if a location suggestion is triggered.
    private static final double LOCATION_SUGGESTION_THRESHOLD = 0.05;

    /**
     * Returns a long value from property model, or a default value.
     *
     * @param model The model that contains the data.
     * @param key The key of the data.
     * @param defaultValue The default value returned when the given property doesn't exist.
     */
    public static long getLong(
            PropertyModel model, ReadableObjectPropertyKey<Long> key, long defaultValue) {
        Long value = model.get(key);
        return (value != null) ? value : defaultValue;
    }

    /**
     * Returns whether the download location suggestion dialog should be prompted.
     *
     * @param dirs The available directories.
     * @param defaultLocation The default download location.
     * @param totalBytes The download size.
     */
    public static boolean shouldSuggestDownloadLocation(
            ArrayList<DirectoryOption> dirs, String defaultLocation, long totalBytes) {
        // Return false if totalBytes is unknown.
        if (totalBytes <= 0) return false;

        boolean shouldSuggestDownloadLocation = false;
        for (DirectoryOption dir : dirs) {
            double spaceLeft = (double) (dir.availableSpace - totalBytes) / dir.totalSpace;
            // If not enough space, skip.
            if (spaceLeft < LOCATION_SUGGESTION_THRESHOLD) continue;
            if (defaultLocation.equals(dir.location)) return false;
            shouldSuggestDownloadLocation = true;
    }
        return shouldSuggestDownloadLocation;
    }

    private DownloadDialogUtils() {}
}
