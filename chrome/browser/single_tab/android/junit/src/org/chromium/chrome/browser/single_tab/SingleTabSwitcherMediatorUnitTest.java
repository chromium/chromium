// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.single_tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.TAB_THUMBNAIL;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.TITLE;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.URL;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.Size;

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
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tab_ui.TabSwitcher.TabSwitcherViewObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link SingleTabSwitcherMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SingleTabSwitcherMediatorUnitTest {
    private final int mTabId = 1;
    private final String mTitle = "test";
    private final GURL mUrl = JUnitTestGURLs.URL_1;
    private final int mTabId2 = 2;
    private final String mTitle2 = "test2";
    private final GURL mUrl2 = JUnitTestGURLs.URL_2;
    private SingleTabSwitcherMediator mMediator;
    private PropertyModel mPropertyModel;

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mNormalTabModel;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private Tab mTab;
    @Mock private Tab mTab2;
    @Mock private TabListFaviconProvider mTabListFaviconProvider;
    @Mock private TabSwitcher.OnTabSelectingListener mOnTabSelectingListener;
    @Mock private TabSwitcherViewObserver mTabSwitcherViewObserver;
    @Mock private TabContentManager mTabContentManager;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private Callback<Integer> mSingleTabCardClickedCallback;
    @Captor private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<Callback<Drawable>> mFaviconCallbackCaptor;

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
        doReturn(mUrl).when(mTab).getUrl();
        doReturn(mTabId).when(mTab).getId();
        doReturn(mTitle).when(mTab).getTitle();
        doReturn(mUrl2).when(mTab2).getUrl();
        doReturn(mTabId2).when(mTab2).getId();
        doReturn(mTitle2).when(mTab2).getTitle();
        doReturn(true).when(mIncognitoTabModel).isIncognito();

        doNothing()
                .when(mTabContentManager)
                .getTabThumbnailWithCallback(anyInt(), any(), any(), anyBoolean(), anyBoolean());

        mPropertyModel = new PropertyModel(SingleTabViewProperties.ALL_KEYS);
        mMediator =
                createMediator(
                        /* singleTabCardClickedCallback= */ null,
                        /* isSurfacePolishEnabled= */ false,
                        /* moduleDelegate= */ null);
    }

    @After
    public void tearDown() {
        mMediator = null;
    }

    @Test
    public void showAndHide() {
        assertNotNull(mPropertyModel.get(FAVICON));
        assertNotNull(mPropertyModel.get(CLICK_LISTENER));
        assertFalse(mPropertyModel.get(IS_VISIBLE));
        mMediator.setOnTabSelectingListener(mOnTabSelectingListener);
        mMediator.addTabSwitcherViewObserver(mTabSwitcherViewObserver);

        mMediator.showTabSwitcherView(true);
        verify(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabListFaviconProvider)
                .getFaviconDrawableForUrlAsync(
                        eq(mUrl), eq(false), mFaviconCallbackCaptor.capture());
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        verify(mTabSwitcherViewObserver).startedShowing();
        verify(mTabSwitcherViewObserver).finishedShowing();
        assertEquals(mPropertyModel.get(TITLE), mTitle);

        mPropertyModel.get(CLICK_LISTENER).onClick(null);
        verify(mOnTabSelectingListener).onTabSelecting(eq(mTabId));

        mMediator.hideTabSwitcherView(true);
        assertFalse(mPropertyModel.get(IS_VISIBLE));
        assertEquals(mPropertyModel.get(TITLE), "");
        verify(mTabSwitcherViewObserver).startedHiding();
        verify(mTabSwitcherViewObserver).finishedHiding();

        mMediator.removeTabSwitcherViewObserver(mTabSwitcherViewObserver);
        mMediator.setOnTabSelectingListener(null);
    }

    @Test
    public void showAndHideHomeModule() {
        when(mModuleDelegate.getHostSurfaceType()).thenReturn(HostSurface.START_SURFACE);
        mMediator =
                createMediator(
                        mSingleTabCardClickedCallback,
                        /* isSurfacePolishEnabled= */ true,
                        mModuleDelegate);
        assertNotNull(mMediator.getTabSelectingListenerForTesting());

        int activeIndex = 1;
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getModel(eq(false))).thenReturn(mNormalTabModel);
        when(mNormalTabModel.getCount()).thenReturn(3);
        when(mNormalTabModel.index()).thenReturn(activeIndex);
        when(mNormalTabModel.getTabAt(activeIndex)).thenReturn(mTab);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mTab.getId()).thenReturn(1);

        // Verifies that the single tab card won't be shown if the last active Tab is a NTP.
        mMediator.showModule();
        verify(mModuleDelegate).onDataFetchFailed(eq(ModuleType.SINGLE_TAB));

        // Verifies that the single tab card will be shown if the last active Tab isn't a NTP.
        when(mTab.getUrl()).thenReturn(mUrl);
        mMediator.showModule();
        verify(mModuleDelegate).onDataReady(eq(ModuleType.SINGLE_TAB), eq(mPropertyModel));
        verify(mTabListFaviconProvider)
                .getFaviconDrawableForUrlAsync(
                        eq(mUrl), eq(false), mFaviconCallbackCaptor.capture());
        assertEquals(mPropertyModel.get(TITLE), mTitle);

        // Verifies that data is cleaned up after hide.
        mMediator.hideTabSwitcherView(true);
        assertEquals(mPropertyModel.get(TITLE), "");
        mPropertyModel.set(TAB_THUMBNAIL, null);
        mPropertyModel.set(URL, "");

        mMediator.removeTabSwitcherViewObserver(mTabSwitcherViewObserver);
    }

    @Test
    public void showAndHide_SurfacePolish() {
        mMediator =
                createMediator(
                        /* singleTabCardClickedCallback= */ null,
                        /* isSurfacePolishEnabled= */ true,
                        null);

        assertNotNull(mPropertyModel.get(FAVICON));
        assertNotNull(mPropertyModel.get(CLICK_LISTENER));
        assertFalse(mPropertyModel.get(IS_VISIBLE));
        mMediator.setOnTabSelectingListener(mOnTabSelectingListener);
        mMediator.addTabSwitcherViewObserver(mTabSwitcherViewObserver);

        mMediator.showTabSwitcherView(true);
        verify(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabListFaviconProvider)
                .getFaviconDrawableForUrlAsync(
                        eq(mUrl), eq(false), mFaviconCallbackCaptor.capture());

        int width =
                ContextUtils.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.R.dimen.single_tab_module_tab_thumbnail_size);
        int height = width;
        Size thumbnailSize = new Size(width, height);
        verify(mTabContentManager)
                .getTabThumbnailWithCallback(
                        eq(mTabId), eq(thumbnailSize), any(), anyBoolean(), anyBoolean());

        assertTrue(mPropertyModel.get(IS_VISIBLE));
        verify(mTabSwitcherViewObserver).startedShowing();
        verify(mTabSwitcherViewObserver).finishedShowing();
        assertEquals(mPropertyModel.get(TITLE), mTitle);
        assertEquals(mPropertyModel.get(URL), SingleTabSwitcherMediator.getDomainUrl(mUrl));

        mPropertyModel.get(CLICK_LISTENER).onClick(null);
        verify(mOnTabSelectingListener).onTabSelecting(eq(mTabId));
        Bitmap bitmap = Bitmap.createBitmap(300, 400, Bitmap.Config.ALPHA_8);
        mPropertyModel.set(TAB_THUMBNAIL, bitmap);
        assertNotNull(mPropertyModel.get(TAB_THUMBNAIL));

        mMediator.hideTabSwitcherView(true);
        assertFalse(mPropertyModel.get(IS_VISIBLE));
        assertEquals(mPropertyModel.get(TITLE), "");
        assertEquals(mPropertyModel.get(URL), "");
        assertEquals(mPropertyModel.get(TAB_THUMBNAIL), null);
        verify(mTabSwitcherViewObserver).startedHiding();
        verify(mTabSwitcherViewObserver).finishedHiding();

        mMediator.removeTabSwitcherViewObserver(mTabSwitcherViewObserver);
        mMediator.setOnTabSelectingListener(null);
    }

    @Test
    public void selectTabAfterSwitchingTabModel() {
        assertFalse(mPropertyModel.get(IS_VISIBLE));
        mMediator.setOnTabSelectingListener(mOnTabSelectingListener);
        mMediator.addTabSwitcherViewObserver(mTabSwitcherViewObserver);

        mMediator.showTabSwitcherView(true);
        verify(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabListFaviconProvider)
                .getFaviconDrawableForUrlAsync(
                        eq(mUrl), eq(false), mFaviconCallbackCaptor.capture());
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        verify(mTabSwitcherViewObserver).startedShowing();
        verify(mTabSwitcherViewObserver).finishedShowing();
        assertEquals(mPropertyModel.get(TITLE), mTitle);

        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, -1);
        verify(mOnTabSelectingListener).onTabSelecting(eq(mTabId));

        mTabModelSelectorObserverCaptor
                .getValue()
                .onTabModelSelected(mIncognitoTabModel, mNormalTabModel);
        mTabModelSelectorObserverCaptor
                .getValue()
                .onTabModelSelected(mNormalTabModel, mIncognitoTabModel);

        // The next tab selecting event should be ignored.
        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, mTabId);
        verify(mOnTabSelectingListener, times(1)).onTabSelecting(eq(mTabId));

        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, mTabId);
        verify(mOnTabSelectingListener, times(2)).onTabSelecting(eq(mTabId));

        mMediator.hideTabSwitcherView(true);
        assertFalse(mPropertyModel.get(IS_VISIBLE));
        assertEquals(mPropertyModel.get(TITLE), "");
        verify(mTabSwitcherViewObserver).startedHiding();
        verify(mTabSwitcherViewObserver).finishedHiding();
    }

    @Test
    public void selectNextTabAfterClosingTheSelectedTab() {
        assertFalse(mPropertyModel.get(IS_VISIBLE));
        mMediator.setOnTabSelectingListener(mOnTabSelectingListener);
        mMediator.addTabSwitcherViewObserver(mTabSwitcherViewObserver);

        mMediator.showTabSwitcherView(true);
        verify(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabListFaviconProvider)
                .getFaviconDrawableForUrlAsync(
                        eq(mUrl), eq(false), mFaviconCallbackCaptor.capture());
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        verify(mTabSwitcherViewObserver).startedShowing();
        verify(mTabSwitcherViewObserver).finishedShowing();
        assertEquals(mPropertyModel.get(TITLE), mTitle);

        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab2, TabSelectionType.FROM_CLOSE, mTabId2);
        verify(mOnTabSelectingListener, times(0)).onTabSelecting(eq(mTabId2));
        assertEquals(mPropertyModel.get(TITLE), mTitle2);

        mMediator.hideTabSwitcherView(true);
        assertFalse(mPropertyModel.get(IS_VISIBLE));
        assertEquals(mPropertyModel.get(TITLE), "");
        verify(mTabSwitcherViewObserver).startedHiding();
        verify(mTabSwitcherViewObserver).finishedHiding();
    }

    @Test
    public void selectTabAfterSwitchingTabModelAndReshown() {
        assertFalse(mPropertyModel.get(IS_VISIBLE));
        mMediator.setOnTabSelectingListener(mOnTabSelectingListener);
        mMediator.addTabSwitcherViewObserver(mTabSwitcherViewObserver);

        mMediator.showTabSwitcherView(true);
        verify(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabListFaviconProvider)
                .getFaviconDrawableForUrlAsync(
                        eq(mUrl), eq(false), mFaviconCallbackCaptor.capture());
        assertEquals(mPropertyModel.get(TITLE), mTitle);

        mTabModelSelectorObserverCaptor
                .getValue()
                .onTabModelSelected(mIncognitoTabModel, mNormalTabModel);
        mTabModelSelectorObserverCaptor
                .getValue()
                .onTabModelSelected(mNormalTabModel, mIncognitoTabModel);
        mMediator.hideTabSwitcherView(true);

        // The next tab selecting event should not be ignored after hiding and reshowing.
        mMediator.showTabSwitcherView(true);
        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, -1);
        verify(mOnTabSelectingListener).onTabSelecting(eq(mTabId));

        mMediator.hideTabSwitcherView(true);
    }

    @Test
    public void testDomainUrl() {
        String expectedUrl = "about";
        GURL url = JUnitTestGURLs.CHROME_ABOUT;
        assertNotNull(SingleTabSwitcherMediator.getDomainUrl(url));
        assertTrue(TextUtils.equals(expectedUrl, SingleTabSwitcherMediator.getDomainUrl(url)));
    }

    private SingleTabSwitcherMediator createMediator(
            Callback<Integer> singleTabCardClickedCallback,
            boolean isSurfacePolishEnabled,
            ModuleDelegate moduleDelegate) {
        return new SingleTabSwitcherMediator(
                ContextUtils.getApplicationContext(),
                mPropertyModel,
                mTabModelSelector,
                mTabListFaviconProvider,
                mTabContentManager,
                singleTabCardClickedCallback,
                isSurfacePolishEnabled,
                moduleDelegate);
    }
}
