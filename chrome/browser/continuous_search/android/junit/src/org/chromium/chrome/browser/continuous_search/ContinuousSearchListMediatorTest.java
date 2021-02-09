// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.url.GURL;

import java.util.Arrays;

/**
 * Tests for {@link ContinuousSearchListMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ContinuousSearchListMediatorTest {
    private ContinuousSearchListMediator mMediator;
    private MVCListAdapter.ModelList mModelList;
    private CallbackHelper mLayoutVisibilityTrue;
    private CallbackHelper mLayoutVisibilityFalse;

    @Before
    public void setUp() {
        mModelList = new MVCListAdapter.ModelList();
        mLayoutVisibilityTrue = new CallbackHelper();
        mLayoutVisibilityFalse = new CallbackHelper();
        mMediator = new ContinuousSearchListMediator(mModelList, (visibility) -> {
            if (visibility) {
                mLayoutVisibilityTrue.notifyCalled();
            } else {
                mLayoutVisibilityFalse.notifyCalled();
            }
        });
        SearchResultUserData searchResultUserData = Mockito.mock(SearchResultUserData.class);
        SearchResultUserData.setInstanceForTesting(searchResultUserData);
    }

    @After
    public void tearDown() {
        SearchResultUserData.setInstanceForTesting(null);
    }

    /**
     * Tests the show/hide logic for the CNS.
     */
    @Test
    public void testVisibility() {
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 0,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should not have been called.", 0,
                mLayoutVisibilityFalse.getCallCount());

        // UI should hide on observing a new tab.
        mMediator.onObserverNewTab(Mockito.mock(Tab.class));
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 0,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 1,
                mLayoutVisibilityFalse.getCallCount());

        // UI should hide on invalidate.
        mMediator.onInvalidate();
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 0,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 2,
                mLayoutVisibilityFalse.getCallCount());

        // 1 result available. UI should be shown.
        SearchResult searchResult = new SearchResult(Mockito.mock(GURL.class), "result 1");
        SearchResultGroup searchResultGroup =
                new SearchResultGroup("result", false, Arrays.asList(searchResult));
        SearchResultMetadata searchResultMetadata = new SearchResultMetadata(
                Mockito.mock(GURL.class), "query", 1, Arrays.asList(searchResultGroup));
        mMediator.onUpdate(searchResultMetadata, Mockito.mock(GURL.class));
        Assert.assertEquals("mLayoutVisibilityTrue should have been called.", 1,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should not have been called.", 2,
                mLayoutVisibilityFalse.getCallCount());

        // 0 results available. UI should be hidden.
        searchResultMetadata =
                new SearchResultMetadata(Mockito.mock(GURL.class), "query", 1, Arrays.asList());
        mMediator.onUpdate(searchResultMetadata, Mockito.mock(GURL.class));
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 1,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 3,
                mLayoutVisibilityFalse.getCallCount());
    }

    /**
     * Tests that the ModelList is correctly populated on updates from {@link SearchResultUserData}.
     */
    @Test
    public void testModelList() {
        // Mock 3 SearchResultGroups, with 1, 2 and 3 SearchResults. The first group is an ad group.
        SearchResult searchResult11 = new SearchResult(Mockito.mock(GURL.class), "result 11");
        SearchResult searchResult21 = new SearchResult(Mockito.mock(GURL.class), "result 21");
        SearchResult searchResult22 = new SearchResult(Mockito.mock(GURL.class), "result 22");
        SearchResult searchResult31 = new SearchResult(Mockito.mock(GURL.class), "result 31");
        SearchResult searchResult32 = new SearchResult(Mockito.mock(GURL.class), "result 32");
        SearchResult searchResult33 = new SearchResult(Mockito.mock(GURL.class), "result 33");

        SearchResultGroup searchResultGroup1 =
                new SearchResultGroup("group1", true, Arrays.asList(searchResult11));
        SearchResultGroup searchResultGroup2 = new SearchResultGroup(
                "group2", false, Arrays.asList(searchResult21, searchResult22));
        SearchResultGroup searchResultGroup3 = new SearchResultGroup(
                "group3", false, Arrays.asList(searchResult31, searchResult32, searchResult33));

        SearchResultMetadata searchResultMetadata =
                new SearchResultMetadata(Mockito.mock(GURL.class), "query", 1,
                        Arrays.asList(searchResultGroup1, searchResultGroup2, searchResultGroup3));
        mMediator.onUpdate(searchResultMetadata, Mockito.mock(GURL.class));

        // Each non-ad SearchResultGroup will add a group label as an item. So in total we should
        // have 8 items in the model list.
        Assert.assertEquals("ModelList length is incorrect.", 8, mModelList.size());

        // Assert the list items for group labels are correctly populated.
        Assert.assertEquals("List item type should be GROUP_LABEL.", ListItemType.GROUP_LABEL,
                mModelList.get(1).type);
        Assert.assertEquals("List item type should be GROUP_LABEL.", ListItemType.GROUP_LABEL,
                mModelList.get(4).type);
        Assert.assertEquals("List item label doesn't match SearchResultGroup.",
                searchResultGroup2.getLabel(),
                mModelList.get(1).model.get(ContinuousSearchListProperties.LABEL));
        Assert.assertEquals("List item label doesn't match SearchResultGroup.",
                searchResultGroup3.getLabel(),
                mModelList.get(4).model.get(ContinuousSearchListProperties.LABEL));

        // Assert the list items for search results are correctly populated.
        assertListItemEqualsSearchResult(mModelList.get(0), searchResult11, true);
        assertListItemEqualsSearchResult(mModelList.get(2), searchResult21, false);
        assertListItemEqualsSearchResult(mModelList.get(3), searchResult22, false);
        assertListItemEqualsSearchResult(mModelList.get(5), searchResult31, false);
        assertListItemEqualsSearchResult(mModelList.get(6), searchResult32, false);
        assertListItemEqualsSearchResult(mModelList.get(7), searchResult33, false);
    }

    private void assertListItemEqualsSearchResult(
            MVCListAdapter.ListItem listItem, SearchResult searchResult, boolean isAdGroup) {
        Assert.assertEquals("List item type doesn't match SearchResult.",
                isAdGroup ? ListItemType.AD : ListItemType.SEARCH_RESULT, listItem.type);
        Assert.assertEquals("List item title doesn't match SearchResult.", searchResult.getTitle(),
                listItem.model.get(ContinuousSearchListProperties.LABEL));
        Assert.assertEquals("List item URL doesn't match SearchResult.", searchResult.getUrl(),
                listItem.model.get(ContinuousSearchListProperties.URL));
    }
}