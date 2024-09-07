// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.single_tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.LATERAL_MARGIN;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.TAB_THUMBNAIL;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.TITLE;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.URL;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Size;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link SingleTabSwitcherOnNtpMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SingleTabSwitcherOnNtpMediatorUnitTest {
    @Rule public JniMocker mocker = new JniMocker();
    @Mock UrlUtilities.Natives mUrlUtilitiesJniMock;

    private final int mTabId = 1;
    private final String mTitle = "test";
    private final GURL mUrl = JUnitTestGURLs.URL_1;
    private final String mUrlHost = mUrl.getHost();
    private final String mTitle2 = "test2";
    private PropertyModel mPropertyModel;

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mNormalTabModel;
    @Mock private Tab mTab;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;
    @Mock private TabListFaviconProvider mTabListFaviconProvider;
    @Mock private TabContentManager mTabContentManager;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Captor private ArgumentCaptor<Callback<Drawable>> mFaviconCallbackCaptor;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Mock private UiConfig mUiConfig;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private Callback<Integer> mSingleTabClickedCallback;
    @Mock private Runnable mSeeMoreLinkClickedCallback;
    @Captor private ArgumentCaptor<DisplayStyleObserver> mDisplayStyleObserverCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);

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
    public void testSingleTabSwitcherOnNtp_Tablet() {
        testSingleTabSwitcherOnNtpImpl(/* isTablet= */ true, /* moduleDelegate= */ null);
    }

    @Test
    public void testSingleTabSwitcherOnNtp_Phone() {
        testSingleTabSwitcherOnNtpImpl(/* isTablet= */ false, /* moduleDelegate= */ null);
    }

    @Test
    public void testSingleTabSwitcherOnNtp_Phone_MagicStack() {
        testSingleTabSwitcherOnNtpImpl(/* isTablet= */ false, mModuleDelegate);
    }

    private void testSingleTabSwitcherOnNtpImpl(boolean isTablet, ModuleDelegate moduleDelegate) {
        SingleTabSwitcherOnNtpMediator mediator =
                new SingleTabSwitcherOnNtpMediator(
                        ContextUtils.getApplicationContext(),
                        mPropertyModel,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        mTab,
                        mSingleTabClickedCallback,
                        mSeeMoreLinkClickedCallback,
                        mTabContentManager,
                        /* uiConfig= */ null,
                        isTablet,
                        moduleDelegate);
        doNothing().when(mTabContentManager).getTabThumbnailWithCallback(anyInt(), any(), any());
        assertNull(mPropertyModel.get(FAVICON));
        assertNull(mPropertyModel.get(TAB_THUMBNAIL));
        assertNull(mPropertyModel.get(TITLE));
        assertNull(mPropertyModel.get(URL));
        assertNotNull(mPropertyModel.get(CLICK_LISTENER));
        assertFalse(mPropertyModel.get(IS_VISIBLE));

        mediator.setVisibility(true);
        int width =
                ContextUtils.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.single_tab_module_tab_thumbnail_size_big);
        int height = width;
        Size thumbnailSize = new Size(width, height);

        verify(mTabListFaviconProvider)
                .getFaviconDrawableForUrlAsync(
                        eq(mUrl), eq(false), mFaviconCallbackCaptor.capture());
        verify(mTabContentManager)
                .getTabThumbnailWithCallback(eq(mTabId), eq(thumbnailSize), any());
        assertEquals(mPropertyModel.get(TITLE), mTitle);
        assertEquals(mUrlHost, mPropertyModel.get(URL));
        assertTrue(mPropertyModel.get(IS_VISIBLE));
        if (moduleDelegate != null) {
            verify(moduleDelegate).onDataReady(eq(ModuleType.SINGLE_TAB), eq(mPropertyModel));
        }

        mPropertyModel.get(CLICK_LISTENER).onClick(null);
        Bitmap bitmap = Bitmap.createBitmap(300, 400, Bitmap.Config.ALPHA_8);
        mPropertyModel.set(TAB_THUMBNAIL, new BitmapDrawable(bitmap));
        assertNotNull(mPropertyModel.get(TAB_THUMBNAIL));
        verify(mSingleTabClickedCallback).onResult(eq(mTabId));

        mediator.setVisibility(false);

        assertFalse(mPropertyModel.get(IS_VISIBLE));
        assertNull(mPropertyModel.get(TITLE));
        assertNull(mPropertyModel.get(FAVICON));
        assertNull(mPropertyModel.get(URL));
        assertNull(mPropertyModel.get(TAB_THUMBNAIL));
    }

    @Test
    public void testWhenMostRecentTabIsNull() {
        SingleTabSwitcherOnNtpMediator mediator =
                new SingleTabSwitcherOnNtpMediator(
                        ContextUtils.getApplicationContext(),
                        mPropertyModel,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        /* mostRecentTab= */ null,
                        /* singleTabCardClickedCallback= */ null,
                        /* seeMoreLinkClickedCallback= */ null,
                        mTabContentManager,
                        /* uiConfig= */ null,
                        /* isTablet= */ true,
                        /* moduleDelegate= */ null);
        assertNotNull(mPropertyModel.get(CLICK_LISTENER));

        mediator.setVisibility(true);

        assertNull(mPropertyModel.get(TITLE));
        verify(mTabListFaviconProvider, never())
                .getFaviconDrawableForUrlAsync(any(), anyBoolean(), any());
        assertFalse(mPropertyModel.get(IS_VISIBLE));
    }

    @Test
    public void testUpdateMostRecentTabInfo_Tablet() {
        testUpdateMostRecentTabInfoImpl(/* isTablet= */ true);
    }

    @Test
    public void testUpdateMostRecentTabInfo_Phone() {
        testUpdateMostRecentTabInfoImpl(/* isTablet= */ false);
    }

    private void testUpdateMostRecentTabInfoImpl(boolean isTablet) {
        SingleTabSwitcherOnNtpMediator mediator =
                new SingleTabSwitcherOnNtpMediator(
                        ContextUtils.getApplicationContext(),
                        mPropertyModel,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        mTab,
                        /* singleTabCardClickedCallback= */ null,
                        /* seeMoreLinkClickedCallback= */ null,
                        mTabContentManager,
                        /* uiConfig= */ null,
                        isTablet,
                        /* moduleDelegate= */ null);
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
    public void testUpdateSingleTabSwitcherTitle_Tablet() {
        testUpdateSingleTabSwitcherTitleImpl(/* isTablet= */ true);
    }

    @Test
    public void testUpdateSingleTabSwitcherTitle_Phone() {
        testUpdateSingleTabSwitcherTitleImpl(/* isTablet= */ false);
    }

    private void testUpdateSingleTabSwitcherTitleImpl(boolean isTablet) {
        doReturn(true).when(mTab3).isLoading();
        doReturn("").when(mTab3).getTitle();
        doReturn(mUrl).when(mTab3).getUrl();
        SingleTabSwitcherOnNtpMediator mediator =
                new SingleTabSwitcherOnNtpMediator(
                        ContextUtils.getApplicationContext(),
                        mPropertyModel,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        mTab3,
                        /* singleTabCardClickedCallback= */ null,
                        /* seeMoreLinkClickedCallback= */ null,
                        /* tabContentManager= */ null,
                        /* uiConfig= */ null,
                        isTablet,
                        /* moduleDelegate= */ null);
        mediator.updateTitle();
        verify(mTab3).addObserver(mTabObserverCaptor.capture());
        doReturn(mTitle).when(mTab3).getTitle();
        mTabObserverCaptor.getValue().onPageLoadFinished(mTab3, mUrl);
        assertEquals(mPropertyModel.get(TITLE), mTitle);
        verify(mTab3).removeObserver(mTabObserverCaptor.getValue());
    }

    @Test
    public void testNoLateralMargin_Tablet() {
        SingleTabSwitcherOnNtpMediator mediator =
                new SingleTabSwitcherOnNtpMediator(
                        ContextUtils.getApplicationContext(),
                        mPropertyModel,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        mTab3,
                        /* singleTabCardClickedCallback= */ null,
                        /* seeMoreLinkClickedCallback= */ null,
                        mTabContentManager,
                        /* uiConfig= */ null,
                        /* isTablet= */ true,
                        /* moduleDelegate= */ null);
        // Verifies the start margins are initialized.
        assertEquals(0, mediator.getDefaultLateralMargin());

        mediator.updateMargins(null);
        // Verifies the LATERAL_MARGIN is set to the margin.
        assertEquals(0, mPropertyModel.get(LATERAL_MARGIN));
        mediator.destroy();
    }

    @Test
    public void testLateralMargin_Phone() {
        SingleTabSwitcherOnNtpMediator mediator =
                new SingleTabSwitcherOnNtpMediator(
                        ContextUtils.getApplicationContext(),
                        mPropertyModel,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        mTab3,
                        /* singleTabCardClickedCallback= */ null,
                        /* seeMoreLinkClickedCallback= */ null,
                        mTabContentManager,
                        /* uiConfig= */ null,
                        /* isTablet= */ false,
                        /* moduleDelegate= */ null);
        Resources resources = ContextUtils.getApplicationContext().getResources();
        int marginExpected =
                resources.getDimensionPixelSize(
                        R.dimen.ntp_search_box_lateral_margin_narrow_window_tablet);

        // Verifies the start margins are initialized.
        assertEquals(marginExpected, mediator.getDefaultLateralMargin());

        mediator.updateMargins(null);
        // Verifies the LATERAL_MARGIN is set to the margin.
        assertEquals(marginExpected, mPropertyModel.get(LATERAL_MARGIN));
    }

    @Test
    public void testSingleTabCardClickCallback() {
        Callback<Integer> callback = Mockito.mock(Callback.class);
        int tabId = 3;
        when(mTab3.getId()).thenReturn(tabId);
        new SingleTabSwitcherOnNtpMediator(
                ContextUtils.getApplicationContext(),
                mPropertyModel,
                mTabModelSelector,
                mTabListFaviconProvider,
                mTab3,
                callback,
                /* seeMoreLinkClickedCallback= */ null,
                mTabContentManager,
                /* uiConfig= */ null,
                /* isTablet= */ true,
                /* moduleDelegate= */ null);
        verify(callback, never()).onResult(anyInt());

        mPropertyModel.get(CLICK_LISTENER).onClick(null);
        verify(callback).onResult(eq(tabId));
    }

    @Test
    public void testUpdateWithUiConfigChanges() {
        UiConfig.DisplayStyle displayStyleWide =
                new DisplayStyle(HorizontalDisplayStyle.WIDE, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleWide);

        SingleTabSwitcherOnNtpMediator mediator =
                new SingleTabSwitcherOnNtpMediator(
                        ContextUtils.getApplicationContext(),
                        mPropertyModel,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        mTab3,
                        /* singleTabCardClickedCallback= */ null,
                        /* seeMoreLinkClickedCallback= */ null,
                        mTabContentManager,
                        mUiConfig,
                        /* isTablet= */ true,
                        /* moduleDelegate= */ null);

        verify(mUiConfig).addObserver(mDisplayStyleObserverCaptor.capture());
        assertEquals(0, mPropertyModel.get(LATERAL_MARGIN));

        int expectedLateralMarginInNarrowWindow =
                ContextUtils.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.R.dimen
                                        .ntp_search_box_lateral_margin_narrow_window_tablet);
        UiConfig.DisplayStyle displayStyleRegular =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleRegular);
        mDisplayStyleObserverCaptor.getValue().onDisplayStyleChanged(displayStyleRegular);
        assertEquals(expectedLateralMarginInNarrowWindow, mPropertyModel.get(LATERAL_MARGIN));

        mediator.destroy();
        verify(mUiConfig).removeObserver(mDisplayStyleObserverCaptor.capture());
    }
}
