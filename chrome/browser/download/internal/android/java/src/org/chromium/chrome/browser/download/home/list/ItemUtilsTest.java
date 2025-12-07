// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Set;

/** Unit tests for the ItemUtils class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ItemUtilsTest {
    /**
     * Test scenarios with which we might call {@link
     * ItemUtils#findItemsWithSameFilePath(Collection, Collection)}.
     */
    @Test
    public void testFindItemsWithSameFilePath() {
        OfflineItem item1 = buildItem("1", "");
        OfflineItem item2 = buildItem("2", "");
        OfflineItem item3 = buildItem("3", null);
        OfflineItem item4 = buildItem("4", null);
        OfflineItem item5 = buildItem("5", "path1");
        OfflineItem item6 = buildItem("6", "path1");
        OfflineItem item7 = buildItem("7", "path2");
        OfflineItem item8 = buildItem("8", "path3");
        OfflineItem item9 = buildItem("9", "path4");
        OfflineItem item10 = buildItem("10", "path4");
        OfflineItem item11 = buildItem("11", "path4");
        OfflineItem item12 = buildItem("12", "path5");

        /* Empty path includes item but not other empty paths. */
        /* Null path includes item but not other null paths. */
        /* Pull in duplicate path item. */
        /* Pull in item with no duplicate paths by itself. */
        /* Pull in item with two other matching paths. */
        /* Item is included even if it does not exist in all items. */
        Set<OfflineItem> items = Set.of(item1, item3, item5, item7, item9, item12);
        Set<OfflineItem> allItems =
                Set.of(
                        item1, item2, item3, item4, item5, item6, item7, item8, item9, item10,
                        item11, item12);

        Set<OfflineItem> expected =
                Set.of(item1, item3, item5, item6, item7, item9, item10, item11, item12);
        Assert.assertEquals(expected, ItemUtils.findItemsWithSameFilePath(items, allItems));
    }

    private static OfflineItem buildItem(String id, String filePath) {
        OfflineItem item = new OfflineItem();
        item.id.id = id;
        item.filePath = filePath;
        return item;
    }
}
