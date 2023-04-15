// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.features.tasks.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TITLE;

import android.graphics.drawable.Drawable;

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
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link SingleTabSwitcherOnTabletMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SingleTabSwitcherOnTabletMediatorUnitTest {
    private final int mTabId = 1;
    private final String mTitle = "test";
    private final GURL mUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
    private final String mTitle2 = "test2";
    private PropertyModel mPropertyModel;

    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mNormalTabModel;
    @Mock
    private Tab mTab;
    @Mock
    private Tab mTab2;
    @Mock
    private Tab mTab3;
    @Mock
    private TabListFaviconProvider mTabListFaviconProvider;
    @Captor
    private ArgumentCaptor<Callback<Drawable>> mFaviconCallbackCaptor;
    @Captor
    private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(true).when(mTabListFaviconProvider).isInitialized();
        doReturn(mNormalTabModel).when(mTabModelSelector).getModel(false);
        doReturn(mTab).when(mNormalTabModel).getTabAt(0);
        doReturn(1).when(mNormalTabModel).getCount();

        doReturn(mUrl).when(mTab).getUrl();
        doReturn(mTabId).when(mTab).getId();
        doReturn(mTitle).when(mTab).getTitle();
        doReturn(mTitle2).when(mTab2).getTitle();

        mPropertyModel = new PropertyModel(SingleTabViewProperties.ALL_KEYS);
    }

    @Test
    public void testSingleTabSwitcherOnTablet() {
        SingleTabSwitcherOnTabletMediator mediator = new SingleTabSwitcherOnTabletMediator(
                mPropertyModel, mTabModelSelector, mTabListFaviconProvider, mTab);
        assertNull(mPropertyModel.get(FAVICON));
        assertNull(mPropertyModel.get(TITLE));
        assertNotNull(mPropertyModel.get(CLICK_LISTENER));
        assertFalse(mPropertyModel.get(IS_VISIBLE));

        mediator.setVisibility(true);

        verify(mTabListFaviconProvider)
                .getFaviconDrawableForUrlAsync(
                        eq(mUrl), eq(false), mFaviconCallbackCaptor.capture());
        assertEquals(mPropertyModel.get(TITLE), mTitle);
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mPropertyModel.get(CLICK_LISTENER).onClick(null);

        verify(mNormalTabModel).setIndex(0, TabSelectionType.FROM_USER, false);

        mediator.setVisibility(false);

        assertFalse(mPropertyModel.get(IS_VISIBLE));
    }

    @Test
    public void testWhenMostRecentTabIsNull() {
        SingleTabSwitcherOnTabletMediator mediator = new SingleTabSwitcherOnTabletMediator(
                mPropertyModel, mTabModelSelector, mTabListFaviconProvider, null);
        assertNotNull(mPropertyModel.get(CLICK_LISTENER));

        mediator.setVisibility(true);

        assertNull(mPropertyModel.get(TITLE));
        verify(mTabListFaviconProvider, never())
                .getFaviconDrawableForUrlAsync(any(), anyBoolean(), any());
        assertFalse(mPropertyModel.get(IS_VISIBLE));
    }

    @Test
    public void testUpdateMostRecentTabInfo() {
        SingleTabSwitcherOnTabletMediator mediator = new SingleTabSwitcherOnTabletMediator(
                mPropertyModel, mTabModelSelector, mTabListFaviconProvider, mTab);
        assertFalse(mediator.getInitialized());

        mediator.setVisibility(true);

        verify(mTabListFaviconProvider)
                .getFaviconDrawableForUrlAsync(
                        eq(mUrl), eq(false), mFaviconCallbackCaptor.capture());
        assertEquals(mPropertyModel.get(TITLE), mTitle);
        assertTrue(mediator.getInitialized());

        mediator.setMostRecentTab(mTab2);
        mediator.setVisibility(true);

        verify(mTabListFaviconProvider, times(1))
                .getFaviconDrawableForUrlAsync(any(), anyBoolean(), any());
        assertEquals(mPropertyModel.get(TITLE), mTitle);
        assertNotEquals(mPropertyModel.get(TITLE), mTitle2);
    }

    @Test
    public void testUpdateSingleTabSwitcherTitleOnTablet() {
        doReturn(true).when(mTab3).isLoading();
        doReturn("").when(mTab3).getTitle();
        doReturn(mUrl).when(mTab3).getUrl();
        SingleTabSwitcherOnTabletMediator mediator = new SingleTabSwitcherOnTabletMediator(
                mPropertyModel, mTabModelSelector, mTabListFaviconProvider, mTab3);
        mediator.updateTitle();
        verify(mTab3).addObserver(mTabObserverCaptor.capture());
        doReturn(mTitle).when(mTab3).getTitle();
        mTabObserverCaptor.getValue().onPageLoadFinished(mTab3, mUrl);
        assertEquals(mPropertyModel.get(TITLE), mTitle);
        verify(mTab3).removeObserver(mTabObserverCaptor.getValue());
    }
}
