// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemProperties;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
    private static final String TRIGGER_MODE_ALWAYS = "0";
    private static final String TRIGGER_MODE_AFTER_SECOND_SRP = "1";
    private static final String TRIGGER_MODE_ON_REVERSE_SCROLL = "2";

    private ContinuousSearchListMediator mMediator;
    private MVCListAdapter.ModelList mModelList;
    private PropertyModel mRootViewModel;
    private CallbackHelper mLayoutVisibilityTrue;
    private CallbackHelper mLayoutVisibilityFalse;
    private int mTriggerMode;

    @Mock
    private BrowserControlsStateProvider mBrowserControlsStateProviderMock;

    @Before
    public void setUp() {
        initMocks(this);
        mModelList = new MVCListAdapter.ModelList();
        mRootViewModel = new PropertyModel(ContinuousSearchListProperties.ALL_KEYS);
        mLayoutVisibilityTrue = new CallbackHelper();
        mLayoutVisibilityFalse = new CallbackHelper();
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
     * Tests the show/hide logic for the always show mode.
     */
    @Test
    public void testVisibilityAlwaysShow() {
        initMediatorWithTriggerMode(TRIGGER_MODE_ALWAYS);

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

        // 2 results available. UI should be shown.
        PageItem pageItem1 =
                new PageItem(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), "result 1");
        PageItem pageItem2 =
                new PageItem(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_2), "result 2");
        PageGroup pageGroup = new PageGroup("result", false, Arrays.asList(pageItem1, pageItem2));
        ContinuousNavigationMetadata continuousNavigationMetadata =
                new ContinuousNavigationMetadata(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL),
                        "query", getProvider(null), Arrays.asList(pageGroup));
        mMediator.onUpdate(continuousNavigationMetadata);
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_2), false);
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
        continuousNavigationMetadata =
                new ContinuousNavigationMetadata(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL),
                        "query", getProvider(null), Arrays.asList());
        mMediator.onUpdate(continuousNavigationMetadata);
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL), true);
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 2,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 4,
                mLayoutVisibilityFalse.getCallCount());
    }

    /**
     * Tests the show/hide logic for show after second SRP mode.
     */
    @Test
    public void testVisibilityAfterSecondSrp() {
        initMediatorWithTriggerMode(TRIGGER_MODE_AFTER_SECOND_SRP);

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

        // 1 result available. UI will not show on first page.
        PageItem pageItem = new PageItem(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), "result 1");
        PageGroup pageGroup = new PageGroup("result", false, Arrays.asList(pageItem));
        ContinuousNavigationMetadata continuousNavigationMetadata =
                new ContinuousNavigationMetadata(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL),
                        "query", getProvider(null), Arrays.asList(pageGroup));
        mMediator.onUpdate(continuousNavigationMetadata);
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL), true);
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), false);
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 0,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 3,
                mLayoutVisibilityFalse.getCallCount());

        // Back to SRP. UI should be hidden.
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL), true);
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 0,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 4,
                mLayoutVisibilityFalse.getCallCount());

        // Back to result. UI should be shown.
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), false);
        Assert.assertEquals("mLayoutVisibilityTrue should have been called.", 1,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should not have been called.", 4,
                mLayoutVisibilityFalse.getCallCount());

        // Back to SRP. UI should be hidden.
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL), true);
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 1,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 5,
                mLayoutVisibilityFalse.getCallCount());

        // Back to result. UI should be shown.
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), false);
        Assert.assertEquals("mLayoutVisibilityTrue should have been called.", 2,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should not have been called.", 5,
                mLayoutVisibilityFalse.getCallCount());
    }

    /**
     * Tests the show/hide logic for the reverse scroll mode.
     */
    @Test
    public void testVisibilityReverseScroll() {
        initMediatorWithTriggerMode(TRIGGER_MODE_ON_REVERSE_SCROLL);
        ArgumentCaptor<BrowserControlsStateProvider.Observer> browserControlsObserverCaptor =
                ArgumentCaptor.forClass(BrowserControlsStateProvider.Observer.class);

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

        verify(mBrowserControlsStateProviderMock)
                .addObserver(browserControlsObserverCaptor.capture());
        BrowserControlsStateProvider.Observer observer = browserControlsObserverCaptor.getValue();
        Assert.assertNotNull(observer);

        // 1 result available. UI will not show immediately.
        PageItem pageItem = new PageItem(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), "result 1");
        PageGroup pageGroup = new PageGroup("result", false, Arrays.asList(pageItem));
        ContinuousNavigationMetadata continuousNavigationMetadata =
                new ContinuousNavigationMetadata(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL),
                        "query", getProvider(null), Arrays.asList(pageGroup));
        mMediator.onUpdate(continuousNavigationMetadata);
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), false);
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 0,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 2,
                mLayoutVisibilityFalse.getCallCount());

        // Controls visible so no change.
        when(mBrowserControlsStateProviderMock.getBrowserControlHiddenRatio())
                .thenReturn(0.0f)
                .thenReturn(1.0f);
        observer.onControlsOffsetChanged(0, 0, 0, 0, false);
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 0,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 2,
                mLayoutVisibilityFalse.getCallCount());

        // Controls hidden so set to showing.
        observer.onControlsOffsetChanged(0, 0, 0, 0, false);
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

        // Controls hidden, but on SRP so don't set showing.
        observer.onControlsOffsetChanged(0, 0, 0, 0, false);
        Assert.assertEquals("mLayoutVisibilityTrue should have been called.", 1,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should not have been called.", 3,
                mLayoutVisibilityFalse.getCallCount());
    }

    /**
     * Tests provider label navigates user to the start page when clicked.
     */
    @Test
    public void testProviderLabel() {
        initMediatorWithTriggerMode(TRIGGER_MODE_ALWAYS);

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
                        "query", getProvider("Test"), Arrays.asList(pageGroup));

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
        mRootViewModel.get(ContinuousSearchListProperties.PROVIDER_CLICK_LISTENER).onClick(null);
        Mockito.verify(navigationController, Mockito.times(1))
                .goToNavigationIndex(startNavigationIndex);
    }

    /**
     * Tests that the ModelList is correctly populated on updates from {@link
     * ContinuousNavigationUserDataImpl}.
     */
    @Test
    public void testModelList() {
        initMediatorWithTriggerMode(TRIGGER_MODE_ALWAYS);
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
                new ContinuousNavigationMetadata(Mockito.mock(GURL.class), "query",
                        getProvider("Test"), Arrays.asList(pageGroup1, pageGroup2, pageGroup3));
        mMediator.onUpdate(continuousNavigationMetadata);

        Assert.assertEquals("ModelList length is incorrect.", 6, mModelList.size());

        // Assert the list items for search results are correctly populated.
        assertListItemEqualsSearchResult(mModelList.get(0), pageItem11, true);
        assertListItemEqualsSearchResult(mModelList.get(1), pageItem21, false);
        assertListItemEqualsSearchResult(mModelList.get(2), pageItem22, false);
        assertListItemEqualsSearchResult(mModelList.get(3), pageItem31, false);
        assertListItemEqualsSearchResult(mModelList.get(4), pageItem32, false);
        assertListItemEqualsSearchResult(mModelList.get(5), pageItem33, false);

        mModelList.get(0).model.get(ListItemProperties.CLICK_LISTENER).onClick(null);
        ArgumentCaptor<LoadUrlParams> params = ArgumentCaptor.forClass(LoadUrlParams.class);
        Mockito.verify(tab, Mockito.times(1)).loadUrl(params.capture());
        Assert.assertEquals(url11.getSpec(), params.getValue().getUrl());
    }

    /**
     * Tests that the UI vanishes on dismissal.
     */
    @Test
    public void testUserDismissal() {
        initMediatorWithTriggerMode(TRIGGER_MODE_ALWAYS);

        // UI should hide on observing a new tab.
        mMediator.onResult(Mockito.mock(Tab.class));
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 0,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 1,
                mLayoutVisibilityFalse.getCallCount());

        PageItem pageItem = new PageItem(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), "result 1");
        PageGroup pageGroup = new PageGroup("result", false, Arrays.asList(pageItem));
        ContinuousNavigationMetadata continuousNavigationMetadata =
                new ContinuousNavigationMetadata(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL),
                        "query", getProvider(null), Arrays.asList(pageGroup));
        mMediator.onUpdate(continuousNavigationMetadata);

        // Open URL.
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), false);
        Assert.assertEquals("mLayoutVisibilityTrue should have been called.", 1,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should not have been called.", 1,
                mLayoutVisibilityFalse.getCallCount());

        // Dismiss.
        mRootViewModel.get(ContinuousSearchListProperties.DISMISS_CLICK_CALLBACK).onClick(null);
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 1,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 2,
                mLayoutVisibilityFalse.getCallCount());

        // Return to SRP.
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.SEARCH_URL), true);
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 1,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 3,
                mLayoutVisibilityFalse.getCallCount());

        // Return to URL - remains not visible.
        mMediator.onUrlChanged(JUnitTestGURLs.getGURL(JUnitTestGURLs.BLUE_1), false);
        Assert.assertEquals("mLayoutVisibilityTrue should not have been called.", 1,
                mLayoutVisibilityTrue.getCallCount());
        Assert.assertEquals("mLayoutVisibilityFalse should have been called.", 4,
                mLayoutVisibilityFalse.getCallCount());
    }

    /**
     * Tests that the data is invalidated if a new tab is observed.
     */
    @Test
    public void testObserveNewTab() {
        initMediatorWithTriggerMode(TRIGGER_MODE_ALWAYS);
        PageItem pageItem = new PageItem(Mockito.mock(GURL.class), "result");
        PageGroup pageGroup = new PageGroup("group", true, Arrays.asList(pageItem));
        ContinuousNavigationMetadata continuousNavigationMetadata =
                new ContinuousNavigationMetadata(Mockito.mock(GURL.class), "query",
                        getProvider(null), Arrays.asList(pageGroup));
        mMediator.onUpdate(continuousNavigationMetadata);
        Assert.assertEquals("ModelList length is incorrect.", 1, mModelList.size());

        mMediator.onResult(null);
        Assert.assertEquals("ModelList length is incorrect.", 0, mModelList.size());
    }

    /**
     * Tests that theme color is updated correctly.
     */
    @Test
    public void testThemeColorChanged() {
        initMediatorWithTriggerMode(TRIGGER_MODE_ALWAYS);
        PageItem pageItem = new PageItem(Mockito.mock(GURL.class), "result");
        PageGroup pageGroup = new PageGroup("group", true, Arrays.asList(pageItem));
        ContinuousNavigationMetadata continuousNavigationMetadata =
                new ContinuousNavigationMetadata(Mockito.mock(GURL.class), "query",
                        getProvider(null), Arrays.asList(pageGroup));
        mMediator.onUpdate(continuousNavigationMetadata);
        Assert.assertEquals("ModelList length is incorrect.", 1, mModelList.size());

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

    private void initMediatorWithTriggerMode(String triggerMode) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CONTINUOUS_SEARCH, true);
        testValues.addFieldTrialParamOverride(ChromeFeatureList.CONTINUOUS_SEARCH,
                ContinuousSearchListMediator.TRIGGER_MODE_PARAM, triggerMode);
        FeatureList.setTestValues(testValues);

        mMediator = new ContinuousSearchListMediator(mBrowserControlsStateProviderMock, mModelList,
                mRootViewModel,
                (visibilitySettings)
                        -> {
                    if (visibilitySettings.isVisible()) {
                        mLayoutVisibilityTrue.notifyCalled();
                    } else {
                        mLayoutVisibilityFalse.notifyCalled();
                        Runnable runnable = visibilitySettings.getOnFinishRunnable();
                        if (runnable != null) {
                            runnable.run();
                        }
                    }
                },
                Mockito.mock(ThemeColorProvider.class),
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(), R.style.ColorOverlay));
    }

    private void assertListItemEqualsSearchResult(
            MVCListAdapter.ListItem listItem, PageItem pageItem, boolean isAdGroup) {
        Assert.assertEquals("List item type doesn't match SearchResult.",
                isAdGroup ? ListItemType.AD : ListItemType.SEARCH_RESULT, listItem.type);
        Assert.assertEquals("List item title doesn't match SearchResult.", pageItem.getTitle(),
                listItem.model.get(ListItemProperties.LABEL));
        Assert.assertEquals("List item URL doesn't match SearchResult.", pageItem.getUrl(),
                listItem.model.get(ListItemProperties.URL));
    }

    private ContinuousNavigationMetadata.Provider getProvider(String name) {
        return new ContinuousNavigationMetadata.Provider(1, name, 0);
    }
}
