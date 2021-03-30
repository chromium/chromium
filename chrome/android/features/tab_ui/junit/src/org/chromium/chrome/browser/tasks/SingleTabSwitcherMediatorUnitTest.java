// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.browser.tasks.SingleTabViewProperties.TITLE;

import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link SingleTabSwitcherMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SingleTabSwitcherMediatorUnitTest {
    private final int mTabId = 1;
    private final String mTitle = "test";
    private final String mUrlString = "chrome://test.com";
    private final int mTabId2 = 2;
    private final String mTitle2 = "test2";
    private final String mUrlString2 = "chrome://test2.com";
    private SingleTabSwitcherMediator mMediator;
    private PropertyModel mPropertyModel;

    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mNormalTabModel;
    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    private TabModel mIncognitoTabModel;
    @Mock
    private Tab mTab;
    @Mock
    private Tab mTab2;
    @Mock
    private TabListFaviconProvider mTabListFaviconProvider;
    @Mock
    private TabSwitcher.OnTabSelectingListener mOnTabSelectingListener;
    @Mock
    private TabSwitcher.OverviewModeObserver mOverviewModeObserver;
    @Captor
    private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor
    private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    private ArgumentCaptor<Callback<Drawable>> mFaviconCallbackCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(new BitmapDrawable())
                .when(mTabListFaviconProvider)
                .getDefaultFaviconDrawable(false);
        doReturn(mNormalTabModel).when(mTabModelSelector).getModel(false);
        doReturn(mTabId).when(mTabModelSelector).getCurrentTabId();
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTab).when(mNormalTabModel).getTabAt(0);
        doReturn(0).when(mNormalTabModel).index();
        doReturn(1).when(mNormalTabModel).getCount();
        doReturn(false).when(mNormalTabModel).isIncognito();
        doReturn(mUrlString).when(mTab).getUrlString();
        doReturn(mTabId).when(mTab).getId();
        doReturn(mTitle).when(mTab).getTitle();
        doReturn(mUrlString2).when(mTab2).getUrlString();
        doReturn(mTabId2).when(mTab2).getId();
        doReturn(mTitle2).when(mTab2).getTitle();
        doReturn(true).when(mIncognitoTabModel).isIncognito();

        mPropertyModel = new PropertyModel(SingleTabViewProperties.ALL_KEYS);
        mMediator = new SingleTabSwitcherMediator(
                mPropertyModel, mTabModelSelector, mTabListFaviconProvider);
    }

    @After
    public void tearDown() {
        mMediator = null;
    }

    @Test
    public void showAndHide() {
        assertNotNull(mPropertyModel.get(FAVICON));
        assertNotNull(mPropertyModel.get(CLICK_LISTENER));
        assertFalse(mMediator.overviewVisible());
        mMediator.setOnTabSelectingListener(mOnTabSelectingListener);
        mMediator.addOverviewModeObserver(mOverviewModeObserver);

        mMediator.showOverview(true);
        verify(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabListFaviconProvider)
                .getFaviconForUrlAsync(eq(mUrlString), eq(false), mFaviconCallbackCaptor.capture());
        assertTrue(mMediator.overviewVisible());
        verify(mOverviewModeObserver).startedShowing();
        verify(mOverviewModeObserver).finishedShowing();
        assertEquals(mPropertyModel.get(TITLE), mTitle);

        mPropertyModel.get(CLICK_LISTENER).onClick(null);
        verify(mOnTabSelectingListener).onTabSelecting(anyLong(), eq(mTabId));

        mMediator.hideOverview(true);
        assertFalse(mMediator.overviewVisible());
        assertEquals(mPropertyModel.get(TITLE), "");
        verify(mOverviewModeObserver).startedHiding();
        verify(mOverviewModeObserver).finishedHiding();

        mMediator.removeOverviewModeObserver(mOverviewModeObserver);
        mMediator.setOnTabSelectingListener(null);
    }

    @Test
    public void selectTabAfterSwitchingTabModel() {
        assertFalse(mMediator.overviewVisible());
        mMediator.setOnTabSelectingListener(mOnTabSelectingListener);
        mMediator.addOverviewModeObserver(mOverviewModeObserver);

        mMediator.showOverview(true);
        verify(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabListFaviconProvider)
                .getFaviconForUrlAsync(eq(mUrlString), eq(false), mFaviconCallbackCaptor.capture());
        assertTrue(mMediator.overviewVisible());
        verify(mOverviewModeObserver).startedShowing();
        verify(mOverviewModeObserver).finishedShowing();
        assertEquals(mPropertyModel.get(TITLE), mTitle);

        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, -1);
        verify(mOnTabSelectingListener).onTabSelecting(anyLong(), eq(mTabId));

        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mNormalTabModel, mIncognitoTabModel);

        // The next tab selecting event should be ignored.
        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, mTabId);
        verify(mOnTabSelectingListener, times(1)).onTabSelecting(anyLong(), eq(mTabId));

        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, mTabId);
        verify(mOnTabSelectingListener, times(2)).onTabSelecting(anyLong(), eq(mTabId));

        mMediator.hideOverview(true);
        assertFalse(mMediator.overviewVisible());
        assertEquals(mPropertyModel.get(TITLE), "");
        verify(mOverviewModeObserver).startedHiding();
        verify(mOverviewModeObserver).finishedHiding();
    }

    @Test
    public void selectNextTabAfterClosingTheSelectedTab() {
        assertFalse(mMediator.overviewVisible());
        mMediator.setOnTabSelectingListener(mOnTabSelectingListener);
        mMediator.addOverviewModeObserver(mOverviewModeObserver);

        mMediator.showOverview(true);
        verify(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabListFaviconProvider)
                .getFaviconForUrlAsync(eq(mUrlString), eq(false), mFaviconCallbackCaptor.capture());
        assertTrue(mMediator.overviewVisible());
        verify(mOverviewModeObserver).startedShowing();
        verify(mOverviewModeObserver).finishedShowing();
        assertEquals(mPropertyModel.get(TITLE), mTitle);

        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_CLOSE, mTabId2);
        verify(mOnTabSelectingListener, times(0)).onTabSelecting(anyLong(), eq(mTabId2));
        assertEquals(mPropertyModel.get(TITLE), mTitle2);

        mMediator.hideOverview(true);
        assertFalse(mMediator.overviewVisible());
        assertEquals(mPropertyModel.get(TITLE), "");
        verify(mOverviewModeObserver).startedHiding();
        verify(mOverviewModeObserver).finishedHiding();
    }

    @Test
    public void selectTabAfterSwitchingTabModelAndReshown() {
        assertFalse(mMediator.overviewVisible());
        mMediator.setOnTabSelectingListener(mOnTabSelectingListener);
        mMediator.addOverviewModeObserver(mOverviewModeObserver);

        mMediator.showOverview(true);
        verify(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabListFaviconProvider)
                .getFaviconForUrlAsync(eq(mUrlString), eq(false), mFaviconCallbackCaptor.capture());
        assertEquals(mPropertyModel.get(TITLE), mTitle);

        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mIncognitoTabModel, mNormalTabModel);
        mTabModelSelectorObserverCaptor.getValue().onTabModelSelected(
                mNormalTabModel, mIncognitoTabModel);
        mMediator.hideOverview(true);

        // The next tab selecting event should not be ignored after hiding and reshowing.
        mMediator.showOverview(true);
        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, -1);
        verify(mOnTabSelectingListener).onTabSelecting(anyLong(), eq(mTabId));

        mMediator.hideOverview(true);
    }

    @Test
    public void onBackPressed() {
        assertFalse(mMediator.overviewVisible());
        mMediator.setOnTabSelectingListener(mOnTabSelectingListener);
        mMediator.addOverviewModeObserver(mOverviewModeObserver);

        assertFalse(mMediator.onBackPressed(false));

        mMediator.showOverview(true);
        assertTrue(mMediator.onBackPressed(false));
        verify(mOnTabSelectingListener).onTabSelecting(anyLong(), eq(mTabId));

        doReturn(TabList.INVALID_TAB_INDEX).when(mTabModelSelector).getCurrentTabId();
        assertFalse(mMediator.onBackPressed(false));

        doReturn(mTabId).when(mTabModelSelector).getCurrentTabId();
        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        assertFalse(mMediator.onBackPressed(false));
    }
}
