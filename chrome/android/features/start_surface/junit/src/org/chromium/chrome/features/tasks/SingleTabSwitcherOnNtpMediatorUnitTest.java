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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.features.tasks.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.LATERAL_MARGIN;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TAB_THUMBNAIL;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TITLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.URL;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.util.Size;

import org.junit.Before;
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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider;
import org.chromium.components.browser_ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link SingleTabSwitcherOnNtpMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SingleTabSwitcherOnNtpMediatorUnitTest {
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
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private UiConfig mUiConfig;
    @Captor private ArgumentCaptor<DisplayStyleObserver> mDisplayStyleObserverCaptor;
    @Captor private ArgumentCaptor<ConfigurationChangedObserver> mConfigurationChangedObserver;

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
    public void testSingleTabSwitcherOnNtp_Tablet() {
        testSingleTabSwitcherOnNtpImpl(true);
    }

    @Test
    public void testSingleTabSwitcherOnNtp_Phone() {
        testSingleTabSwitcherOnNtpImpl(false);
    }

    private void testSingleTabSwitcherOnNtpImpl(boolean isTablet) {
        SingleTabSwitcherOnNtpMediator mediator =
                new SingleTabSwitcherOnNtpMediator(
                        ContextUtils.getApplicationContext(),
                        mPropertyModel,
                        null,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        mTab,
                        false,
                        null,
                        null,
                        null, isTablet);
        assertNull(mPropertyModel.get(FAVICON));
        assertNull(mPropertyModel.get(TITLE));
        assertNotNull(mPropertyModel.get(CLICK_LISTENER));
        assertFalse(mPropertyModel.get(IS_VISIBLE));

        mediator.setVisibility(true);

        verify(mTabListFaviconProvider)
                .getFaviconDrawableForUrlAsync(
                        eq(mUrl), eq(false), mFaviconCallbackCaptor.capture());
        assertEquals(mTitle, mPropertyModel.get(TITLE));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mPropertyModel.get(CLICK_LISTENER).onClick(null);

        verify(mNormalTabModel).setIndex(0, TabSelectionType.FROM_USER, false);

        mediator.setVisibility(false);

        assertFalse(mPropertyModel.get(IS_VISIBLE));
        assertNull(mPropertyModel.get(TITLE));
        assertNull(mPropertyModel.get(FAVICON));
    }

    @Test
    public void testSingleTabSwitcherOnNtp_SurfacePolish_Tablet() {
        testSingleTabSwitcherOnNtpImpl_SurfacePolish(true);
    }

    @Test
    public void testSingleTabSwitcherOnNtp_SurfacePolish_Phone() {
        testSingleTabSwitcherOnNtpImpl_SurfacePolish(false);
    }

    private void testSingleTabSwitcherOnNtpImpl_SurfacePolish(boolean isTablet) {
        SingleTabSwitcherOnNtpMediator mediator =
                new SingleTabSwitcherOnNtpMediator(
                        ContextUtils.getApplicationContext(),
                        mPropertyModel,
                        null,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        mTab,
                        false,
                        null,
                        mTabContentManager,
                        null, isTablet);
        doNothing()
                .when(mTabContentManager)
                .getTabThumbnailWithCallback(anyInt(), any(), any(), anyBoolean(), anyBoolean());
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
                        .getDimensionPixelSize(
                                org.chromium.chrome.R.dimen.single_tab_module_tab_thumbnail_size);
        int height = width;
        Size thumbnailSize = new Size(width, height);

        verify(mTabListFaviconProvider)
                .getFaviconDrawableForUrlAsync(
                        eq(mUrl), eq(false), mFaviconCallbackCaptor.capture());
        verify(mTabContentManager)
                .getTabThumbnailWithCallback(
                        eq(mTabId), eq(thumbnailSize), any(), anyBoolean(), anyBoolean());
        assertEquals(mPropertyModel.get(TITLE), mTitle);
        assertEquals(mUrlHost, mPropertyModel.get(URL));
        assertTrue(mPropertyModel.get(IS_VISIBLE));

        mPropertyModel.get(CLICK_LISTENER).onClick(null);
        Bitmap bitmap = Bitmap.createBitmap(300, 400, Bitmap.Config.ALPHA_8);
        mPropertyModel.set(TAB_THUMBNAIL, bitmap);
        assertNotNull(mPropertyModel.get(TAB_THUMBNAIL));
        verify(mNormalTabModel).setIndex(0, TabSelectionType.FROM_USER, false);

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
                        null,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        null,
                        false,
                        null,
                        null,
                        null, true);
        assertNotNull(mPropertyModel.get(CLICK_LISTENER));

        mediator.setVisibility(true);

        assertNull(mPropertyModel.get(TITLE));
        verify(mTabListFaviconProvider, never())
                .getFaviconDrawableForUrlAsync(any(), anyBoolean(), any());
        assertFalse(mPropertyModel.get(IS_VISIBLE));
    }

    @Test
    public void testUpdateMostRecentTabInfo_Tablet() {
        testUpdateMostRecentTabInfoImpl(true);
    }

    @Test
    public void testUpdateMostRecentTabInfo_Phone() {
        testUpdateMostRecentTabInfoImpl(false);
    }

    private void testUpdateMostRecentTabInfoImpl(boolean isTablet) {
        SingleTabSwitcherOnNtpMediator mediator =
            new SingleTabSwitcherOnNtpMediator(
                ContextUtils.getApplicationContext(),
                mPropertyModel,
                null,
                mTabModelSelector,
                mTabListFaviconProvider,
                mTab,
                false,
                null,
                null,
                null, isTablet);
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
        testUpdateSingleTabSwitcherTitleImpl(true);
    }

    @Test
    public void testUpdateSingleTabSwitcherTitle_Phone() {
        testUpdateSingleTabSwitcherTitleImpl(false);
    }

    private void testUpdateSingleTabSwitcherTitleImpl(boolean isTablet) {
        doReturn(true).when(mTab3).isLoading();
        doReturn("").when(mTab3).getTitle();
        doReturn(mUrl).when(mTab3).getUrl();
        SingleTabSwitcherOnNtpMediator mediator =
            new SingleTabSwitcherOnNtpMediator(
                ContextUtils.getApplicationContext(),
                mPropertyModel,
                null,
                mTabModelSelector,
                mTabListFaviconProvider,
                mTab3,
                false,
                null,
                null,
                null, isTablet);
        mediator.updateTitle();
        verify(mTab3).addObserver(mTabObserverCaptor.capture());
        doReturn(mTitle).when(mTab3).getTitle();
        mTabObserverCaptor.getValue().onPageLoadFinished(mTab3, mUrl);
        assertEquals(mPropertyModel.get(TITLE), mTitle);
        verify(mTab3).removeObserver(mTabObserverCaptor.getValue());
    }
    @Test
    public void testStartMarginWith1RowMvTiles_Tablet() {
        SingleTabSwitcherOnNtpMediator mediator =
                new SingleTabSwitcherOnNtpMediator(
                        ContextUtils.getApplicationContext(),
                        mPropertyModel,
                        mActivityLifecycleDispatcher,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        mTab3,
                        /* isScrollableMvtEnabled= */ true,
                        null,
                        null,
                        null, true);
        verify(mActivityLifecycleDispatcher).register(mConfigurationChangedObserver.capture());

        Resources resources = ContextUtils.getApplicationContext().getResources();
        int marginLandscape =
                resources.getDimensionPixelSize(
                        R.dimen.single_tab_card_lateral_margin_landscape_tablet);
        int marginPortrait =
                resources.getDimensionPixelSize(R.dimen.tile_grid_layout_bleed) / 2
                        + resources.getDimensionPixelSize(
                                R.dimen.single_tab_card_lateral_margin_portrait_tablet);
        // Verifies the start margins are initialized.
        assertEquals(marginLandscape, mediator.getMarginDefaultForTesting());
        assertEquals(marginPortrait, mediator.getMarginSmallPortraitForTesting());

        // Verifies the LATERAL_MARGIN is set to the margin in the landscape mode.
        Configuration config = Mockito.mock(Configuration.class);
        config.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mConfigurationChangedObserver.getValue().onConfigurationChanged(config);
        assertEquals(marginLandscape, mPropertyModel.get(LATERAL_MARGIN));

        // Verifies the LATERAL_MARGIN is set to the margin in the portrait mode.
        config.orientation = Configuration.ORIENTATION_PORTRAIT;
        mConfigurationChangedObserver.getValue().onConfigurationChanged(config);
        assertEquals(marginPortrait, mPropertyModel.get(LATERAL_MARGIN));

        mediator.destroy();
        verify(mActivityLifecycleDispatcher).unregister(mConfigurationChangedObserver.getValue());
    }

        @Test
    public void testStartMarginWith1RowMvTiles_Phone() {
        // Sets up the mediator with surface polish flag disabled, i.e., TabContentManager is null.
        SingleTabSwitcherOnNtpMediator mediator =
            new SingleTabSwitcherOnNtpMediator(
                ContextUtils.getApplicationContext(),
                mPropertyModel,
                mActivityLifecycleDispatcher,
                mTabModelSelector,
                mTabListFaviconProvider,
                mTab3,
                /* isScrollableMvtEnabled= */ true,
                null,
                /* TabContentManager */ null,
                null, /* isTablet */ false);
        verify(mActivityLifecycleDispatcher, never()).register(
            mConfigurationChangedObserver.capture());

        // Verifies the LATERAL_MARGIN is set to 0.
        Configuration config = Mockito.mock(Configuration.class);
        config.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mediator.onConfigurationChanged(config);
        assertEquals(0, mPropertyModel.get(LATERAL_MARGIN));

        // Verifies the LATERAL_MARGIN is set to 0.
        config.orientation = Configuration.ORIENTATION_PORTRAIT;
        mediator.onConfigurationChanged(config);
        assertEquals(0, mPropertyModel.get(LATERAL_MARGIN));
    }


    @Test
    public void testStartMarginWith1RowMvTiles_Phone_SurfacePolish() {
        // Sets up the mediator with surface polish flag enabled, i.e., TabContentManager isn't
        // null.
        SingleTabSwitcherOnNtpMediator mediator =
            new SingleTabSwitcherOnNtpMediator(
                ContextUtils.getApplicationContext(),
                mPropertyModel,
                mActivityLifecycleDispatcher,
                mTabModelSelector,
                mTabListFaviconProvider,
                mTab3,
                /* isScrollableMvtEnabled= */ true,
                null,
                mTabContentManager,
                null, /* isTablet */ false);
        verify(mActivityLifecycleDispatcher, never()).register(
            mConfigurationChangedObserver.capture());

        Resources resources = ContextUtils.getApplicationContext().getResources();
        int marginLandscape =
            resources.getDimensionPixelSize(
                R.dimen.search_box_lateral_margin_polish);
        int marginPortrait = marginLandscape;

        // Verifies the LATERAL_MARGIN is set to the margin in the landscape mode.
        Configuration config = Mockito.mock(Configuration.class);
        config.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mediator.onConfigurationChanged(config);
        assertEquals(marginLandscape, mPropertyModel.get(LATERAL_MARGIN));

        // Verifies the LATERAL_MARGIN is set to the margin in the portrait mode.
        config.orientation = Configuration.ORIENTATION_PORTRAIT;
        mediator.onConfigurationChanged(config);
        assertEquals(marginPortrait, mPropertyModel.get(LATERAL_MARGIN));
    }

    @Test
    public void testStartMarginWith2RowMvTiles_Tablets() {
        SingleTabSwitcherOnNtpMediator mediator =
                new SingleTabSwitcherOnNtpMediator(
                        ContextUtils.getApplicationContext(),
                        mPropertyModel,
                        mActivityLifecycleDispatcher,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        mTab3,
                        /* isScrollableMvtEnabled= */ false,
                        null,
                        null,
                        null, true);
        verify(mActivityLifecycleDispatcher).register(mConfigurationChangedObserver.capture());

        int lateralMargin =
                ContextUtils.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.single_tab_card_lateral_margin_landscape_tablet);
        // Verifies the start margins are initialized.
        assertEquals(lateralMargin, mediator.getMarginDefaultForTesting());

        mediator.setVisibility(true);
        assertEquals(lateralMargin, mPropertyModel.get(LATERAL_MARGIN));

        // Verifies the LATERAL_MARGIN is set to the margin in both landscape and portrait
        // modes.
        Configuration config = Mockito.mock(Configuration.class);
        config.orientation = Configuration.ORIENTATION_LANDSCAPE;
        mConfigurationChangedObserver.getValue().onConfigurationChanged(config);
        assertEquals(lateralMargin, mPropertyModel.get(LATERAL_MARGIN));

        config.orientation = Configuration.ORIENTATION_PORTRAIT;
        mConfigurationChangedObserver.getValue().onConfigurationChanged(config);
        assertEquals(lateralMargin, mPropertyModel.get(LATERAL_MARGIN));

        mediator.destroy();
        verify(mActivityLifecycleDispatcher).unregister(mConfigurationChangedObserver.getValue());
    }

    @Test
    public void testNoLateralMargin_SurfacePolish_Tablet() {
        TabContentManager tabContentManager = Mockito.mock(TabContentManager.class);
        SingleTabSwitcherOnNtpMediator mediator =
                new SingleTabSwitcherOnNtpMediator(
                        ContextUtils.getApplicationContext(),
                        mPropertyModel,
                        mActivityLifecycleDispatcher,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        mTab3,
                        /* isScrollableMvtEnabled= */ false,
                        null,
                        tabContentManager,
                        null, true);
        verify(mActivityLifecycleDispatcher, never())
                .register(mConfigurationChangedObserver.capture());

        // Verifies the start margins are initialized.
        assertEquals(0, mediator.getMarginDefaultForTesting());
        assertEquals(0, mediator.getMarginSmallPortraitForTesting());
        assertEquals(0, mPropertyModel.get(LATERAL_MARGIN));
    }

    @Test
    public void testSingleTabCardClickCallback() {
        Runnable callback = Mockito.mock(Runnable.class);
        new SingleTabSwitcherOnNtpMediator(
                ContextUtils.getApplicationContext(),
                mPropertyModel,
                mActivityLifecycleDispatcher,
                mTabModelSelector,
                mTabListFaviconProvider,
                mTab3,
                /* isScrollableMvtEnabled= */ false,
                callback,
                null,
                null, true);
        verify(callback, never()).run();

        mPropertyModel.get(CLICK_LISTENER).onClick(null);
        verify(callback).run();
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
                        mActivityLifecycleDispatcher,
                        mTabModelSelector,
                        mTabListFaviconProvider,
                        mTab3,
                        /* isScrollableMvtEnabled= */ true,
                        null,
                        mTabContentManager,
                        mUiConfig, true);

        verify(mUiConfig).addObserver(mDisplayStyleObserverCaptor.capture());
        assertEquals(0, mPropertyModel.get(LATERAL_MARGIN));

        int expectedLateralMarginInNarrowWindow =
                ContextUtils.getApplicationContext()
                        .getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.R.dimen.search_box_lateral_margin_polish);
        UiConfig.DisplayStyle displayStyleRegular =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleRegular);
        mDisplayStyleObserverCaptor.getValue().onDisplayStyleChanged(displayStyleRegular);
        assertEquals(expectedLateralMarginInNarrowWindow, mPropertyModel.get(LATERAL_MARGIN));

        mediator.destroy();
        verify(mUiConfig).removeObserver(mDisplayStyleObserverCaptor.capture());
    }
}
