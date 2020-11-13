// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.offline_items_collection.OfflineItemFilter;

/** Unit tests for the Filters class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FiltersTest {
    @Test
    public void testFilterConversions() {
        Assert.assertEquals(Integer.valueOf(Filters.FilterType.SITES),
                Filters.fromOfflineItem(OfflineItemFilter.PAGE));
        Assert.assertEquals(Integer.valueOf(Filters.FilterType.VIDEOS),
                Filters.fromOfflineItem(OfflineItemFilter.VIDEO));
        Assert.assertEquals(Integer.valueOf(Filters.FilterType.MUSIC),
                Filters.fromOfflineItem(OfflineItemFilter.AUDIO));
        Assert.assertEquals(Integer.valueOf(Filters.FilterType.IMAGES),
                Filters.fromOfflineItem(OfflineItemFilter.IMAGE));
        Assert.assertEquals(Integer.valueOf(Filters.FilterType.OTHER),
                Filters.fromOfflineItem(OfflineItemFilter.OTHER));
        Assert.assertEquals(Integer.valueOf(Filters.FilterType.OTHER),
                Filters.fromOfflineItem(OfflineItemFilter.DOCUMENT));
    }
}