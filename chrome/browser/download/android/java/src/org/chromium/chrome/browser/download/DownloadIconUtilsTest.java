// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.junit.Assert.assertEquals;

import android.graphics.Bitmap;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.download.DownloadDangerType;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;

/** Unit tests for {@link DownloadIconUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DownloadIconUtilsTest {
    private static OfflineItem createItem(@OfflineItemFilter int filter) {
        OfflineItem item = new OfflineItem();
        item.state = OfflineItemState.COMPLETE;
        item.filter = filter;
        return item;
    }

    @Test
    public void testGetIconForItem_dangerousOrBlockedOverridesFilter() {
        OfflineItem safe = createItem(OfflineItemFilter.IMAGE);
        assertEquals(R.drawable.ic_drive_image_24dp, DownloadIconUtils.getIconForItem(safe));

        OfflineItem dangerous = createItem(OfflineItemFilter.IMAGE);
        dangerous.dangerType = DownloadDangerType.DANGEROUS_CONTENT;
        assertEquals(R.drawable.dangerous_filled_24dp, DownloadIconUtils.getIconForItem(dangerous));

        OfflineItem blocked = createItem(OfflineItemFilter.IMAGE);
        blocked.state = OfflineItemState.FAILED;
        blocked.failState = FailState.FILE_BLOCKED;
        blocked.dangerType = DownloadDangerType.SENSITIVE_CONTENT_BLOCK;
        assertEquals(R.drawable.dangerous_filled_24dp, DownloadIconUtils.getIconForItem(blocked));
    }

    @Test
    public void testGetFileTypeIcon_hasRequestedSize() {
        Bitmap bitmap = DownloadIconUtils.getFileTypeIcon(OfflineItemFilter.IMAGE, 32);
        assertEquals(32, bitmap.getWidth());
        assertEquals(32, bitmap.getHeight());
        assertEquals(Bitmap.Config.ARGB_8888, bitmap.getConfig());

        bitmap = DownloadIconUtils.getFileTypeIcon(OfflineItemFilter.OTHER, 48);
        assertEquals(48, bitmap.getWidth());
        assertEquals(48, bitmap.getHeight());
    }

    @Test
    public void testGetFileTypeIcon_everyFilterHasIcon() {
        // The page and video icons are tinted with a theme attribute, unlike the others.
        for (@OfflineItemFilter int filter = 0; filter < OfflineItemFilter.NUM_ENTRIES; filter++) {
            Bitmap bitmap = DownloadIconUtils.getFileTypeIcon(filter, 32);
            assertEquals("Filter " + filter, 32, bitmap.getWidth());
            assertEquals("Filter " + filter, 32, bitmap.getHeight());
        }
    }
}
