// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;

/** Icons for download items, shared by the Downloads page and the download toolbar tray. */
@NullMarked
public final class DownloadIconUtils {
    private DownloadIconUtils() {}

    /** Returns the drawable resource for the icon representing {@code item}. */
    public static @DrawableRes int getIconForItem(OfflineItem item) {
        if (DownloadUtils.shouldDisplayDownloadAsDangerous(item.dangerType, item.state)
                || DownloadUtils.isBlockedSensitiveDownload(item)) {
            return R.drawable.dangerous_filled_24dp;
        }
        switch (item.filter) {
            case OfflineItemFilter.PAGE:
                return R.drawable.ic_globe_24dp;
            case OfflineItemFilter.VIDEO:
                return R.drawable.ic_videocam_fill_24dp;
            case OfflineItemFilter.AUDIO:
                return R.drawable.ic_music_note_24dp;
            case OfflineItemFilter.IMAGE:
                return R.drawable.ic_drive_image_24dp;
            default:
                // Documents share the generic file icon.
                return R.drawable.ic_drive_file_24dp;
        }
    }
}
