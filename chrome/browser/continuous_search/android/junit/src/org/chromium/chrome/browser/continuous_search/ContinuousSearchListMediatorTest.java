// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.content.res.Resources;

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
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

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
        }, Mockito.mock(ThemeColorProvider.class), Mockito.mock(Resources.class));
        ContinuousNavigationUserDataImpl continuousNavigationUserData =
                Mockito.mock(ContinuousNavigationUserDataImpl.class);
        ContinuousNavigationUserDataImpl.setInstanceForTesting(continuousNavigationUserData);
    }

    @After
    public void tearDown() {
        ContinuousNavigationUserDataImpl.setInstanceForTesting(null);
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
        mMediator.onResult(Mockito.mock(Tab.class));
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
        PageItem pageItem = new PageItem(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), "result 1");
        PageGroup pageGroup = new PageGroup("result", false, Arrays.asList(pageItem));
        ContinuousNavigationMetadata continuousNavigationMetadata =
                new ContinuousNavigationMetadata(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL),
                        "query", 1, Arrays.asList(pageGroup));
        mMediator.onUpdate(continuousNavigationMetadata);
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), false);
        Assert.assertEquals("mLayoutVisibilityTrue should have been called.", 1,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should not have been called.", 2,
                mLayoutVisibilityFalse.getCallCount());

        // Back to SRP. UI should be hidden.
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL), true);
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 1,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 3,
                mLayoutVisibilityFalse.getCallCount());

        // Back to result. UI should be shown.
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), false);
        Assert.assertEquals("mLayoutVisibilityTrue should have been called.", 2,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should not have been called.", 3,
                mLayoutVisibilityFalse.getCallCount());

        // 0 results available. UI should be hidden.
        continuousNavigationMetadata = new ContinuousNavigationMetadata(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL), "query", 1, Arrays.asList());
        mMediator.onUpdate(continuousNavigationMetadata);
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL), true);
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 2,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 4,
                mLayoutVisibilityFalse.getCallCount());
    }

    /**
     * Tests that the ModelList is correctly populated on updates from {@link
     * ContinuousNavigationUserDataImpl}.
     */
    @Test
    public void testModelList() {
        // Mock 3 SearchResultGroups, with 1, 2 and 3 SearchResults. The first group is an ad group.
        PageItem pageItem11 = new PageItem(Mockito.mock(GURL.class), "result 11");
        PageItem pageItem21 = new PageItem(Mockito.mock(GURL.class), "result 21");
        PageItem pageItem22 = new PageItem(Mockito.mock(GURL.class), "result 22");
        PageItem pageItem31 = new PageItem(Mockito.mock(GURL.class), "result 31");
        PageItem pageItem32 = new PageItem(Mockito.mock(GURL.class), "result 32");
        PageItem pageItem33 = new PageItem(Mockito.mock(GURL.class), "result 33");

        PageGroup pageGroup1 = new PageGroup("group1", true, Arrays.asList(pageItem11));
        PageGroup pageGroup2 =
                new PageGroup("group2", false, Arrays.asList(pageItem21, pageItem22));
        PageGroup pageGroup3 =
                new PageGroup("group3", false, Arrays.asList(pageItem31, pageItem32, pageItem33));

        ContinuousNavigationMetadata continuousNavigationMetadata =
                new ContinuousNavigationMetadata(Mockito.mock(GURL.class), "query", 1,
                        Arrays.asList(pageGroup1, pageGroup2, pageGroup3));
        mMediator.onUpdate(continuousNavigationMetadata);

        // Each non-ad SearchResultGroup will add a group label as an item. So in total we should
        // have 8 items in the model list.
        Assert.assertEquals("ModelList length is incorrect.", 8, mModelList.size());

        // Assert the list items for group labels are correctly populated.
        Assert.assertEquals("List item type should be GROUP_LABEL.", ListItemType.GROUP_LABEL,
                mModelList.get(1).type);
        Assert.assertEquals("List item type should be GROUP_LABEL.", ListItemType.GROUP_LABEL,
                mModelList.get(4).type);
        Assert.assertEquals("List item label doesn't match SearchResultGroup.",
                pageGroup2.getLabel(),
                mModelList.get(1).model.get(ContinuousSearchListProperties.LABEL));
        Assert.assertEquals("List item label doesn't match SearchResultGroup.",
                pageGroup3.getLabel(),
                mModelList.get(4).model.get(ContinuousSearchListProperties.LABEL));

        // Assert the list items for search results are correctly populated.
        assertListItemEqualsSearchResult(mModelList.get(0), pageItem11, true);
        assertListItemEqualsSearchResult(mModelList.get(2), pageItem21, false);
        assertListItemEqualsSearchResult(mModelList.get(3), pageItem22, false);
        assertListItemEqualsSearchResult(mModelList.get(5), pageItem31, false);
        assertListItemEqualsSearchResult(mModelList.get(6), pageItem32, false);
        assertListItemEqualsSearchResult(mModelList.get(7), pageItem33, false);
    }

    private void assertListItemEqualsSearchResult(
            MVCListAdapter.ListItem listItem, PageItem pageItem, boolean isAdGroup) {
        Assert.assertEquals("List item type doesn't match SearchResult.",
                isAdGroup ? ListItemType.AD : ListItemType.SEARCH_RESULT, listItem.type);
        Assert.assertEquals("List item title doesn't match SearchResult.", pageItem.getTitle(),
                listItem.model.get(ContinuousSearchListProperties.LABEL));
        Assert.assertEquals("List item URL doesn't match SearchResult.", pageItem.getUrl(),
                listItem.model.get(ContinuousSearchListProperties.URL));
    }
}
