// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
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
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Collection;
import java.util.Collections;

/** Unit tests for the SearchOfflineItemFilter class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchOfflineItemFilterTest {
    @Mock private OfflineItemFilterSource mSource;

    @Mock private OfflineItemFilterObserver mObserver;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    public void testTitleFiltering() {
        OfflineItem item1 = buildItem("cows", GURL.emptyGURL());
        OfflineItem item2 = buildItem("cows are", GURL.emptyGURL());
        OfflineItem item3 = buildItem("cows are crazy!", GURL.emptyGURL());
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2, item3);
        when(mSource.getItems()).thenReturn(sourceItems);

        SearchOfflineItemFilter filter = buildFilter(mSource);
        filter.addObserver(mObserver);
        Assert.assertEquals(sourceItems, filter.getItems());

        // Test a query that doesn't match.
        filter.onQueryChanged("dogs");
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item1, item2, item3));
        Assert.assertEquals(Collections.emptySet(), filter.getItems());

        // Test undoing the query.
        filter.onQueryChanged("");
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item1, item2, item3));
        Assert.assertEquals(sourceItems, filter.getItems());

        // Test null queries.
        filter.onQueryChanged(null);
        Assert.assertEquals(sourceItems, filter.getItems());

        // Test progressive queries.
        filter.onQueryChanged("cows");
        Assert.assertEquals(CollectionUtil.newHashSet(item1, item2, item3), filter.getItems());

        filter.onQueryChanged("cows ar");
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item1));
        Assert.assertEquals(CollectionUtil.newHashSet(item2, item3), filter.getItems());

        filter.onQueryChanged("cows are c");
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item2));
        Assert.assertEquals(CollectionUtil.newHashSet(item3), filter.getItems());

        filter.onQueryChanged("cows are crazy!");
        Assert.assertEquals(CollectionUtil.newHashSet(item3), filter.getItems());

        filter.onQueryChanged("cows are");
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item2));
        Assert.assertEquals(CollectionUtil.newHashSet(item2, item3), filter.getItems());

        // Test upper case.
        filter.onQueryChanged("CoWs ArE");
        Assert.assertEquals(CollectionUtil.newHashSet(item2, item3), filter.getItems());
    }

    @Test
    public void testUrlFiltering() {
        OfflineItem item1 = buildItem("", JUnitTestGURLs.GOOGLE_URL);
        OfflineItem item2 = buildItem("", JUnitTestGURLs.GOOGLE_URL_DOGS);
        OfflineItem item3 = buildItem("", new GURL("http://www.google.com/dogs-are-fun"));
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2, item3);
        when(mSource.getItems()).thenReturn(sourceItems);

        SearchOfflineItemFilter filter = buildFilter(mSource);
        filter.addObserver(mObserver);
        Assert.assertEquals(sourceItems, filter.getItems());

        // Test a query that doesn't match.
        filter.onQueryChanged("cows");
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item1, item2, item3));
        Assert.assertEquals(Collections.emptySet(), filter.getItems());

        // Test undoing the query.
        filter.onQueryChanged("");
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item1, item2, item3));
        Assert.assertEquals(sourceItems, filter.getItems());

        // Test null queries.
        filter.onQueryChanged(null);
        Assert.assertEquals(sourceItems, filter.getItems());

        // Test progressive queries.
        filter.onQueryChanged("google");
        Assert.assertEquals(CollectionUtil.newHashSet(item1, item2, item3), filter.getItems());

        filter.onQueryChanged("dogs");
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item1));
        Assert.assertEquals(CollectionUtil.newHashSet(item2, item3), filter.getItems());

        filter.onQueryChanged("dogs-are");
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item2));
        Assert.assertEquals(CollectionUtil.newHashSet(item3), filter.getItems());

        filter.onQueryChanged("dogs-are-fun");
        Assert.assertEquals(CollectionUtil.newHashSet(item3), filter.getItems());

        filter.onQueryChanged("dogs");
        verify(mObserver, times(1)).onItemsAdded(CollectionUtil.newHashSet(item2));
        Assert.assertEquals(CollectionUtil.newHashSet(item2, item3), filter.getItems());

        // Test upper case.
        filter.onQueryChanged("DoGs");
        Assert.assertEquals(CollectionUtil.newHashSet(item2, item3), filter.getItems());
    }

    @Test
    public void testUrlOrTitleFiltering() {
        OfflineItem item1 = buildItem("cat", JUnitTestGURLs.GOOGLE_URL_DOG);
        OfflineItem item2 = buildItem("dog", JUnitTestGURLs.GOOGLE_URL_CAT);
        OfflineItem item3 = buildItem("cow", new GURL("http://www.google.com/pig"));
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2, item3);
        when(mSource.getItems()).thenReturn(sourceItems);

        SearchOfflineItemFilter filter = buildFilter(mSource);
        filter.addObserver(mObserver);
        Assert.assertEquals(sourceItems, filter.getItems());

        filter.onQueryChanged("cat");
        verify(mObserver, times(1)).onItemsRemoved(CollectionUtil.newHashSet(item3));
        Assert.assertEquals(CollectionUtil.newHashSet(item1, item2), filter.getItems());
    }

    private static SearchOfflineItemFilter buildFilter(OfflineItemFilterSource source) {
        return new SearchOfflineItemFilter(source) {
            /** Override this method to avoid calls into native. */
            @Override
            protected String formatUrl(String url) {
                return url;
            }
        };
    }

    private static OfflineItem buildItem(String title, GURL url) {
        OfflineItem item = new OfflineItem();
        item.title = title;
        item.originalUrl = url;
        return item;
    }
}
