// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;

/** Unit tests for the TypeOfflineItemFilter class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OffTheRecordOfflineItemFilterTest {
    @Mock
    private OfflineItemFilterSource mSource;

    @Mock
    private OfflineItemFilterObserver mObserver;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    public void testPassthrough() {
        OfflineItem item1 = buildItem(true);
        OfflineItem item2 = buildItem(false);
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2);
        when(mSource.getItems()).thenReturn(sourceItems);

        OffTheRecordOfflineItemFilter filter = new OffTheRecordOfflineItemFilter(true, mSource);
        Assert.assertEquals(CollectionUtil.newHashSet(item1, item2), filter.getItems());
    }

    @Test
    public void testFiltersOutItems() {
        OfflineItem item1 = buildItem(true);
        OfflineItem item2 = buildItem(false);
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2);
        when(mSource.getItems()).thenReturn(sourceItems);

        OffTheRecordOfflineItemFilter filter = new OffTheRecordOfflineItemFilter(false, mSource);
        Assert.assertEquals(CollectionUtil.newHashSet(item2), filter.getItems());
    }

    private static OfflineItem buildItem(boolean isOffTheRecord) {
        OfflineItem item = new OfflineItem();
        item.isOffTheRecord = isOffTheRecord;
        return item;
    }
}