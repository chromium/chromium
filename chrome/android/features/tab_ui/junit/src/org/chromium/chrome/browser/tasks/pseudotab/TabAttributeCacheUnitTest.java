// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.pseudotab;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.util.browser.Features;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link TabAttributeCache}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabAttributeCacheUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;

    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabModelFilter mTabModelFilter;
    @Mock
    TabModel mTabModel;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor
    ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private TabAttributeCache mCache;

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        mTab1 = prepareTab(TAB1_ID);
        mTab2 = prepareTab(TAB2_ID);

        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);

        doReturn(tabModelList).when(mTabModelSelector).getModels();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getTabModelFilter(eq(false));

        doNothing().when(mTabModelFilter).addObserver(mTabModelObserverCaptor.capture());

        doNothing().when(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doNothing().when(mTab1).addObserver(mTabObserverCaptor.capture());
        doNothing().when(mTab2).addObserver(mTabObserverCaptor.capture());
        doReturn(0).when(mTabModel).index();
        doReturn(2).when(mTabModel).getCount();
        doReturn(mTabModel).when(mTabModel).getComprehensiveModel();

        mCache = new TabAttributeCache(mTabModelSelector);
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        RecordHistogram.setDisabledForTests(false);
        mCache.destroy();
        TabAttributeCache.clearAllForTesting();
    }

    @Test
    public void updateUrl() {
        String url = "url 1";
        doReturn(url).when(mTab1).getUrl();

        Assert.assertNotEquals(url, TabAttributeCache.getUrl(TAB1_ID));

        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);
        Assert.assertEquals(url, TabAttributeCache.getUrl(TAB1_ID));

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        Assert.assertNotEquals(url, TabAttributeCache.getUrl(TAB1_ID));
    }

    @Test
    public void updateUrl_incognito() {
        String url = "url 1";
        doReturn(url).when(mTab1).getUrl();
        doReturn(true).when(mTab1).isIncognito();

        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);
        Assert.assertNotEquals(url, TabAttributeCache.getUrl(TAB1_ID));
    }

    @Test
    public void updateTitle() {
        String title = "title 1";
        doReturn(title).when(mTab1).getTitle();

        Assert.assertNotEquals(title, TabAttributeCache.getTitle(TAB1_ID));

        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);
        Assert.assertEquals(title, TabAttributeCache.getTitle(TAB1_ID));

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        Assert.assertNotEquals(title, TabAttributeCache.getTitle(TAB1_ID));
    }

    @Test
    public void updateTitle_incognito() {
        String title = "title 1";
        doReturn(title).when(mTab1).getTitle();
        doReturn(true).when(mTab1).isIncognito();

        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);
        Assert.assertNotEquals(title, TabAttributeCache.getTitle(TAB1_ID));
    }

    @Test
    public void updateRootId() {
        int rootId = 1337;
        doReturn(rootId).when(mTab1).getRootId();

        Assert.assertNotEquals(rootId, TabAttributeCache.getRootId(TAB1_ID));

        mTabObserverCaptor.getValue().onRootIdChanged(mTab1, rootId);
        Assert.assertEquals(rootId, TabAttributeCache.getRootId(TAB1_ID));

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        Assert.assertNotEquals(rootId, TabAttributeCache.getRootId(TAB1_ID));
    }

    @Test
    public void updateRootId_incognito() {
        int rootId = 1337;
        doReturn(rootId).when(mTab1).getRootId();
        doReturn(true).when(mTab1).isIncognito();

        mTabObserverCaptor.getValue().onRootIdChanged(mTab1, rootId);
        Assert.assertNotEquals(rootId, TabAttributeCache.getRootId(TAB1_ID));
    }

    @Test
    public void onTabStateInitialized() {
        String url1 = "url 1";
        doReturn(url1).when(mTab1).getUrl();
        String title1 = "title 1";
        doReturn(title1).when(mTab1).getTitle();
        int rootId1 = 1337;
        doReturn(rootId1).when(mTab1).getRootId();

        String url2 = "url 2";
        doReturn(url2).when(mTab2).getUrl();
        String title2 = "title 2";
        doReturn(title2).when(mTab2).getTitle();
        int rootId2 = 42;
        doReturn(rootId2).when(mTab2).getRootId();

        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(2).when(mTabModelFilter).getCount();

        Assert.assertNotEquals(url1, TabAttributeCache.getUrl(TAB1_ID));
        Assert.assertNotEquals(title1, TabAttributeCache.getTitle(TAB1_ID));
        Assert.assertNotEquals(rootId1, TabAttributeCache.getRootId(TAB1_ID));

        Assert.assertNotEquals(url2, TabAttributeCache.getUrl(TAB2_ID));
        Assert.assertNotEquals(title2, TabAttributeCache.getTitle(TAB2_ID));
        Assert.assertNotEquals(rootId2, TabAttributeCache.getRootId(TAB2_ID));

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();

        Assert.assertEquals(url1, TabAttributeCache.getUrl(TAB1_ID));
        Assert.assertEquals(title1, TabAttributeCache.getTitle(TAB1_ID));
        Assert.assertEquals(rootId1, TabAttributeCache.getRootId(TAB1_ID));

        Assert.assertEquals(url2, TabAttributeCache.getUrl(TAB2_ID));
        Assert.assertEquals(title2, TabAttributeCache.getTitle(TAB2_ID));
        Assert.assertEquals(rootId2, TabAttributeCache.getRootId(TAB2_ID));
    }

    private Tab prepareTab(int id) {
        Tab tab = mock(Tab.class);
        doReturn(id).when(tab).getId();
        return tab;
    }
}
