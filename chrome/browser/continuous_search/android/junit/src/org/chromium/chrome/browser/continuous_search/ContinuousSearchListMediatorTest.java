// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
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
    private PropertyModel mRootViewModel;
    private CallbackHelper mLayoutVisibilityTrue;
    private CallbackHelper mLayoutVisibilityFalse;

    @Before
    public void setUp() {
        mModelList = new MVCListAdapter.ModelList();
        mRootViewModel = new PropertyModel(ContinuousSearchListProperties.ROOT_VIEW_KEYS);
        mLayoutVisibilityTrue = new CallbackHelper();
        mLayoutVisibilityFalse = new CallbackHelper();
        mMediator = new ContinuousSearchListMediator(mModelList, mRootViewModel,
                (visibility)
                        -> {
                    if (visibility) {
                        mLayoutVisibilityTrue.notifyCalled();
                    } else {
                        mLayoutVisibilityFalse.notifyCalled();
                    }
                },
                Mockito.mock(ThemeColorProvider.class),
                ApplicationProvider.getApplicationContext().getResources());
        ContinuousNavigationUserDataImpl continuousNavigationUserData =
                Mockito.mock(ContinuousNavigationUserDataImpl.class);
        Mockito.doAnswer((invocation) -> {
                   mMediator.onInvalidate();
                   return null;
               })
                .when(continuousNavigationUserData)
                .invalidateData();
        ContinuousNavigationUserDataImpl.setInstanceForTesting(continuousNavigationUserData);
    }

    @After
    public void tearDown() {
        ContinuousNavigationUserDataImpl.setInstanceForTesting(null);
        mMediator.destroy();
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
        mMediator.onScrolled();
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
     * Tests provider label navigates user to the start page when clicked.
     */
    @Test
    public void testProviderLabel() {
        // Prepare mock classes.
        Tab tab = Mockito.mock(Tab.class);
        WebContents webContents = Mockito.mock(WebContents.class);
        NavigationController navigationController = Mockito.mock(NavigationController.class);
        NavigationEntry navigationEntry = Mockito.mock(NavigationEntry.class);
        Mockito.when(tab.getWebContents()).thenReturn(webContents);
        Mockito.when(webContents.getNavigationController()).thenReturn(navigationController);
        Mockito.when(navigationController.getEntryAtIndex(Mockito.anyInt()))
                .thenReturn(navigationEntry);

        // Prepare mock data.
        final int startNavigationIndex = 3;
        PageItem pageItem1 =
                new PageItem(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), "result 1");
        PageItem pageItem2 =
                new PageItem(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_2), "result 2");
        PageItem pageItem3 =
                new PageItem(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_3), "result 3");
        PageGroup pageGroup =
                new PageGroup("results", false, Arrays.asList(pageItem1, pageItem2, pageItem3));
        ContinuousNavigationMetadata continuousNavigationMetadata =
                new ContinuousNavigationMetadata(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL),
                        "query", 1, Arrays.asList(pageGroup));

        // Prepare mock behavior.
        Mockito.when(navigationController.getLastCommittedEntryIndex())
                .thenReturn(startNavigationIndex);
        mMediator.onResult(tab);
        mMediator.onUpdate(continuousNavigationMetadata);
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), false);
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_2), false);

        // Click on provider label and verify there's a request for going to the start navigation
        // index.
        Mockito.verify(navigationController, Mockito.never()).goToNavigationIndex(Mockito.anyInt());
        mModelList.get(0).model.get(ContinuousSearchListProperties.CLICK_LISTENER).onClick(null);
        Mockito.verify(navigationController, Mockito.times(1))
                .goToNavigationIndex(startNavigationIndex);
    }

    /**
     * Tests that the ModelList is correctly populated on updates from {@link
     * ContinuousNavigationUserDataImpl}.
     */
    @Test
    public void testModelList() {
        Tab tab = Mockito.mock(Tab.class);
        mMediator.onResult(tab);
        // Mock 3 SearchResultGroups, with 1, 2 and 3 SearchResults. The first group is an ad group.
        GURL url11 = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);
        PageItem pageItem11 = new PageItem(url11, "result 11");
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

        // We should have 1 provider label item on top of page items. So in total we should
        // have 7 items in the model list.
        Assert.assertEquals("ModelList length is incorrect.", 7, mModelList.size());

        // Assert the list item for provider label is correctly populated.
        Assert.assertEquals("List item type should be GROUP_LABEL.", ListItemType.GROUP_LABEL,
                mModelList.get(0).type);
        Assert.assertTrue("Provider label item doesn't match its category string.",
                mModelList.get(0)
                        .model.get(ContinuousSearchListProperties.LABEL)
                        .contains(continuousNavigationMetadata.getProviderName()));

        // Assert the list items for search results are correctly populated.
        assertListItemEqualsSearchResult(mModelList.get(1), pageItem11, true);
        assertListItemEqualsSearchResult(mModelList.get(2), pageItem21, false);
        assertListItemEqualsSearchResult(mModelList.get(3), pageItem22, false);
        assertListItemEqualsSearchResult(mModelList.get(4), pageItem31, false);
        assertListItemEqualsSearchResult(mModelList.get(5), pageItem32, false);
        assertListItemEqualsSearchResult(mModelList.get(6), pageItem33, false);

        mModelList.get(1).model.get(ContinuousSearchListProperties.CLICK_LISTENER).onClick(null);
        ArgumentCaptor<LoadUrlParams> params = ArgumentCaptor.forClass(LoadUrlParams.class);
        Mockito.verify(tab, Mockito.times(1)).loadUrl(params.capture());
        Assert.assertEquals(url11.getSpec(), params.getValue().getUrl());
    }

    /**
     * Tests that the data is invalidated on user dismissal.
     */
    @Test
    public void testUserInvalidate() {
        mMediator.onResult(Mockito.mock(Tab.class));
        PageItem pageItem = new PageItem(Mockito.mock(GURL.class), "result");
        PageGroup pageGroup = new PageGroup("group", true, Arrays.asList(pageItem));
        ContinuousNavigationMetadata continuousNavigationMetadata =
                new ContinuousNavigationMetadata(
                        Mockito.mock(GURL.class), "query", 1, Arrays.asList(pageGroup));
        mMediator.onUpdate(continuousNavigationMetadata);
        Assert.assertEquals("ModelList length is incorrect.", 2, mModelList.size());

        mRootViewModel.get(ContinuousSearchListProperties.DISMISS_CLICK_CALLBACK).onClick(null);
        Assert.assertEquals("ModelList length is incorrect.", 0, mModelList.size());
    }

    /**
     * Tests that the data is invalidated if a new tab is observed.
     */
    @Test
    public void testObserveNewTab() {
        PageItem pageItem = new PageItem(Mockito.mock(GURL.class), "result");
        PageGroup pageGroup = new PageGroup("group", true, Arrays.asList(pageItem));
        ContinuousNavigationMetadata continuousNavigationMetadata =
                new ContinuousNavigationMetadata(
                        Mockito.mock(GURL.class), "query", 1, Arrays.asList(pageGroup));
        mMediator.onUpdate(continuousNavigationMetadata);
        Assert.assertEquals("ModelList length is incorrect.", 2, mModelList.size());

        mMediator.onResult(null);
        Assert.assertEquals("ModelList length is incorrect.", 0, mModelList.size());
    }

    /**
     * Tests that theme color is updated correctly.
     */
    @Test
    public void testThemeColorChanged() {
        PageItem pageItem = new PageItem(Mockito.mock(GURL.class), "result");
        PageGroup pageGroup = new PageGroup("group", true, Arrays.asList(pageItem));
        ContinuousNavigationMetadata continuousNavigationMetadata =
                new ContinuousNavigationMetadata(
                        Mockito.mock(GURL.class), "query", 1, Arrays.asList(pageGroup));
        mMediator.onUpdate(continuousNavigationMetadata);
        Assert.assertEquals("ModelList length is incorrect.", 2, mModelList.size());

        // Use a dark color.
        int color = 0xFFFFFF;
        mMediator.onThemeColorChanged(color, false);
        Assert.assertEquals("Background color incorrect.", color,
                mRootViewModel.get(ContinuousSearchListProperties.BACKGROUND_COLOR));

        // Use a light color.
        color = 0x000000;
        mMediator.onThemeColorChanged(color, false);
        Assert.assertEquals("Background color incorrect.", color,
                mRootViewModel.get(ContinuousSearchListProperties.BACKGROUND_COLOR));
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
