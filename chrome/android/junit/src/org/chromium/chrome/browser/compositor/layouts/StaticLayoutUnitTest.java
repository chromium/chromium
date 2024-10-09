// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.util.DisplayMetrics;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.CallbackUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Collections;

/** Unit tests for {@link StaticLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.AVOID_SELECTED_TAB_FOCUS_ON_LAYOUT_DONE_SHOWING)
public class StaticLayoutUnitTest {

    private static final int TAB1_ID = 0;
    private static final int TAB2_ID = 789;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final String TAB1_URL = JUnitTestGURLs.URL_1.getSpec();
    private static final String TAB2_URL = JUnitTestGURLs.URL_2.getSpec();

    private static final int BACKGROUND_COLOR = Color.WHITE;
    private static final int TOOLBAR_BACKGROUND_COLOR = Color.BLUE;
    private static final int TEXT_BOX_BACKGROUND_COLOR = Color.BLACK;
    private static final int WIDTH = 9;
    private static final int HEIGHT = 16;

    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private DisplayMetrics mDisplayMetrics;
    @Mock private LayoutUpdateHost mUpdateHost;
    @Mock private LayoutRenderHost mRenderHost;
    @Mock private LayoutManagerHost mViewHost;
    @Mock StaticTabSceneLayer mStaticTabSceneLayer;

    private CompositorModelChangeProcessor.FrameRequestSupplier mRequestSupplier;

    @Mock private TabContentManager mTabContentManager;

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;

    @Captor
    private ArgumentCaptor<BrowserControlsStateProvider.Observer>
            mBrowserControlsStateProviderObserverCaptor;

    private UserDataHost mUserDataHost = new UserDataHost();
    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;

    @Mock private View mTabView;

    private Tab mTab1;
    private Tab mTab2;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private CompositorAnimationHandler mCompositorAnimationHandler;

    private StaticLayout mStaticLayout;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mRequestSupplier =
                new CompositorModelChangeProcessor.FrameRequestSupplier(
                        CallbackUtils.emptyRunnable());

        mCompositorAnimationHandler = new CompositorAnimationHandler(mUpdateHost::requestUpdate);
        CompositorAnimationHandler.setTestingMode(true);

        mTab1 = prepareTab(TAB1_ID, new GURL(TAB1_URL));
        mTab2 = prepareTab(TAB2_ID, new GURL(TAB2_URL));

        doReturn(mResources).when(mContext).getResources();
        doReturn(mDisplayMetrics).when(mResources).getDisplayMetrics();
        mDisplayMetrics.density = 1;

        doReturn(mTabModel).when(mTabModel).getComprehensiveModel();
        doReturn(2).when(mTabModel).getCount();
        doReturn(mTab1).when(mTabModel).getTabAt(0);
        doReturn(mTab2).when(mTabModel).getTabAt(1);
        doNothing().when(mTab1).addObserver(mTabObserverCaptor.capture());
        doNothing().when(mTab2).addObserver(mTabObserverCaptor.capture());
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doReturn(POSITION1).when(mTabModel).index();

        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn(Arrays.asList(mTabModel)).when(mTabModelSelector).getModels();
        doNothing().when(mTabModel).addObserver(mTabModelObserverCaptor.capture());

        doNothing()
                .when(mBrowserControlsStateProvider)
                .addObserver(mBrowserControlsStateProviderObserverCaptor.capture());
        doNothing().when(mTabContentManager).updateVisibleIds(any(), anyInt());
        doReturn(WIDTH).when(mViewHost).getWidth();
        doReturn(HEIGHT).when(mViewHost).getHeight();
        doReturn(mCompositorAnimationHandler).when(mUpdateHost).getAnimationHandler();

        mStaticLayout =
                new StaticLayout(
                        mContext,
                        mUpdateHost,
                        mRenderHost,
                        mViewHost,
                        mRequestSupplier,
                        mTabModelSelector,
                        mTabContentManager,
                        mBrowserControlsStateProvider,
                        () -> mTopUiThemeColorProvider,
                        mStaticTabSceneLayer);
        mModel = mStaticLayout.getModelForTesting();
        doReturn(true).when(mUpdateHost).isActiveLayout(mStaticLayout);

        doReturn(BACKGROUND_COLOR).when(mTopUiThemeColorProvider).getBackgroundColor(any());
        doReturn(TOOLBAR_BACKGROUND_COLOR)
                .when(mTopUiThemeColorProvider)
                .getSceneLayerBackground(any());
        mStaticLayout.setTextBoxBackgroundColorForTesting(TEXT_BOX_BACKGROUND_COLOR);

        initAndAssertAllDependencies();

        mStaticLayout.show(System.currentTimeMillis(), false);
        initAndAssertAllProperties();
        // Reset calls to the mock as it will have been called during init.
        reset(mTabContentManager);
    }

    @After
    public void tearDown() {
        CompositorAnimationHandler.setTestingMode(false);
        mStaticLayout.setTextBoxBackgroundColorForTesting(null);
        mStaticLayout.destroy();
    }

    private void initAndAssertAllDependencies() {
        assertEquals(mTabModelSelector, mStaticLayout.getTabModelSelectorForTesting());
        assertEquals(mTabContentManager, mStaticLayout.getTabContentManagerForTesting());

        assertEquals(
                mBrowserControlsStateProvider,
                mStaticLayout.getBrowserControlsStateProviderForTesting());
    }

    private void initAndAssertAllProperties() {
        assertEquals(mTab1, mTabModelSelector.getCurrentTab());
        assertEquals(TAB1_ID, mModel.get(LayoutTab.TAB_ID));
        assertEquals(WIDTH, mModel.get(LayoutTab.ORIGINAL_CONTENT_WIDTH_IN_DP), 0);
        assertEquals(HEIGHT, mModel.get(LayoutTab.ORIGINAL_CONTENT_HEIGHT_IN_DP), 0);
        assertEquals(WIDTH, mModel.get(LayoutTab.MAX_CONTENT_WIDTH), 0);
        assertEquals(HEIGHT, mModel.get(LayoutTab.MAX_CONTENT_HEIGHT), 0);
        assertEquals(BACKGROUND_COLOR, mModel.get(LayoutTab.BACKGROUND_COLOR));
        assertEquals(TOOLBAR_BACKGROUND_COLOR, mModel.get(LayoutTab.TOOLBAR_BACKGROUND_COLOR));
        assertEquals(TEXT_BOX_BACKGROUND_COLOR, mModel.get(LayoutTab.TEXT_BOX_BACKGROUND_COLOR));

        assertFalse(mModel.get(LayoutTab.IS_INCOGNITO));
        assertFalse(mModel.get(LayoutTab.SHOULD_STALL));
        assertTrue(mModel.get(LayoutTab.CAN_USE_LIVE_TEXTURE));
    }

    private Tab prepareTab(int id, GURL url) {
        Tab tab = mock(Tab.class);
        doReturn(id).when(tab).getId();
        doReturn(false).when(tab).isIncognito();
        doReturn(url).when(tab).getUrl();
        doReturn(false).when(tab).isNativePage();
        doReturn(mock(WebContents.class)).when(tab).getWebContents();
        doReturn(true).when(tab).isInitialized();
        doReturn(TOOLBAR_BACKGROUND_COLOR).when(tab).getThemeColor();
        when(tab.getUserDataHost()).thenReturn(mUserDataHost);
        return tab;
    }

    private TabModelObserver getTabModelSelectorTabModelObserverFromCaptor() {
        // Index 1 captured the TabModelSelectorTabObserver.
        return mTabModelObserverCaptor.getAllValues().get(0);
    }

    @Test
    public void testBrowserControlsContentOffsetChanged() {
        final int offset = 10;
        doReturn(offset).when(mBrowserControlsStateProvider).getContentOffset();
        mBrowserControlsStateProviderObserverCaptor
                .getValue()
                .onControlsOffsetChanged(offset, offset, 0, 0, true, false);
        assertEquals(offset, (int) mModel.get(LayoutTab.CONTENT_OFFSET));
    }

    @Test
    public void testTabSelection() {
        assertNotEquals(mTab2.getId(), mModel.get(LayoutTab.TAB_ID));

        getTabModelSelectorTabModelObserverFromCaptor()
                .didSelectTab(mTab2, TabSelectionType.FROM_USER, TAB1_ID);

        assertEquals(mTab2.getId(), mModel.get(LayoutTab.TAB_ID));
        assertFalse(mModel.get(LayoutTab.SHOULD_STALL));
        assertEquals(0.0f, mModel.get(LayoutTab.STATIC_TO_VIEW_BLEND), 0);
        assertEquals(1.0f, mModel.get(LayoutTab.SATURATION), 0);
        assertTrue(mModel.get(LayoutTab.CAN_USE_LIVE_TEXTURE));
        verify(mTabContentManager).updateVisibleIds(eq(Collections.emptyList()), eq(TAB2_ID));
    }

    @Test
    public void testTabSelectionInactive() {
        doReturn(false).when(mUpdateHost).isActiveLayout(mStaticLayout);
        assertNotEquals(mTab2.getId(), mModel.get(LayoutTab.TAB_ID));

        getTabModelSelectorTabModelObserverFromCaptor()
                .didSelectTab(mTab2, TabSelectionType.FROM_USER, TAB1_ID);

        assertEquals(mTab2.getId(), mModel.get(LayoutTab.TAB_ID));
        assertFalse(mModel.get(LayoutTab.SHOULD_STALL));
        assertEquals(0.0f, mModel.get(LayoutTab.STATIC_TO_VIEW_BLEND), 0);
        assertEquals(1.0f, mModel.get(LayoutTab.SATURATION), 0);
        assertTrue(mModel.get(LayoutTab.CAN_USE_LIVE_TEXTURE));
        verify(mTabContentManager, never()).updateVisibleIds(any(), anyInt());
    }

    @Test
    public void testTabSelectionNativeTab() {
        assertNotEquals(mTab2.getId(), mModel.get(LayoutTab.TAB_ID));
        doReturn(true).when(mTab2).isNativePage();

        getTabModelSelectorTabModelObserverFromCaptor()
                .didSelectTab(mTab2, TabSelectionType.FROM_USER, TAB1_ID);

        assertEquals(mTab2.getId(), mModel.get(LayoutTab.TAB_ID));
        assertFalse(mModel.get(LayoutTab.SHOULD_STALL));
        assertEquals(0.0f, mModel.get(LayoutTab.STATIC_TO_VIEW_BLEND), 0);
        assertEquals(1.0f, mModel.get(LayoutTab.SATURATION), 0);
        assertFalse(mModel.get(LayoutTab.CAN_USE_LIVE_TEXTURE));
        verify(mTabContentManager)
                .updateVisibleIds(eq(Collections.singletonList(TAB2_ID)), eq(TAB2_ID));
    }

    @Test
    public void testTabSelection_Stall() {
        doReturn(true).when(mTab2).isFrozen();

        getTabModelSelectorTabModelObserverFromCaptor()
                .didSelectTab(mTab2, TabSelectionType.FROM_USER, TAB1_ID);

        assertTrue(mModel.get(LayoutTab.SHOULD_STALL));
        assertEquals(1.0f, mModel.get(LayoutTab.STATIC_TO_VIEW_BLEND), 0);
        assertEquals(0.0f, mModel.get(LayoutTab.SATURATION), 0);
        verify(mTabContentManager)
                .updateVisibleIds(eq(Collections.singletonList(TAB2_ID)), eq(TAB2_ID));
    }

    @Test
    public void testTabSelection_SameTab() {
        getTabModelSelectorTabModelObserverFromCaptor()
                .didSelectTab(mTab1, TabSelectionType.FROM_USER, TAB1_ID);

        assertFalse(mModel.get(LayoutTab.SHOULD_STALL));
        assertEquals(0.0f, mModel.get(LayoutTab.STATIC_TO_VIEW_BLEND), 0);
        assertEquals(1.0f, mModel.get(LayoutTab.SATURATION), 0);
        verify(mTabContentManager).updateVisibleIds(eq(Collections.emptyList()), eq(TAB1_ID));
    }

    @Test
    public void testOnPageLoadFinished() {
        doReturn(true).when(mTab2).isFrozen();
        getTabModelSelectorTabModelObserverFromCaptor()
                .didSelectTab(mTab2, TabSelectionType.FROM_USER, TAB1_ID);
        assertTrue(mModel.get(LayoutTab.SHOULD_STALL));
        assertEquals(1.0f, mModel.get(LayoutTab.STATIC_TO_VIEW_BLEND), 0);
        assertEquals(0.0f, mModel.get(LayoutTab.SATURATION), 0);
        verify(mTabContentManager)
                .updateVisibleIds(eq(Collections.singletonList(TAB2_ID)), eq(TAB2_ID));

        // Index 1 is the TabObserver for mTab2.
        mTabObserverCaptor.getAllValues().get(1).onPageLoadFinished(mTab2, new GURL(TAB2_URL));

        assertFalse(mModel.get(LayoutTab.SHOULD_STALL));
        assertEquals(0.0f, mModel.get(LayoutTab.STATIC_TO_VIEW_BLEND), 0);
        assertEquals(1.0f, mModel.get(LayoutTab.SATURATION), 0);
        verify(mTabContentManager).updateVisibleIds(eq(Collections.emptyList()), eq(TAB2_ID));
    }

    @Test
    public void testTabOnShown() {
        assertNotEquals(TAB2_ID, mModel.get(LayoutTab.TAB_ID));

        // Index 1 is the TabObserver for mTab2.
        mTabObserverCaptor.getAllValues().get(1).onShown(mTab2, TabSelectionType.FROM_USER);
        assertEquals(TAB2_ID, mModel.get(LayoutTab.TAB_ID));
        assertTrue(mModel.get(LayoutTab.CAN_USE_LIVE_TEXTURE));
        assertFalse(mModel.get(LayoutTab.SHOULD_STALL));
        assertEquals(0.0f, mModel.get(LayoutTab.STATIC_TO_VIEW_BLEND), 0);

        verify(mTabContentManager).updateVisibleIds(eq(Collections.emptyList()), eq(TAB2_ID));
    }

    @Test
    public void testTabOnContentChanged() {
        // Index 0 is the TabObserver for mTab1.
        mTabObserverCaptor.getAllValues().get(0).onContentChanged(mTab1);
        assertTrue(mModel.get(LayoutTab.CAN_USE_LIVE_TEXTURE));
        assertEquals(0.0f, mModel.get(LayoutTab.STATIC_TO_VIEW_BLEND), 0);

        verify(mTabContentManager).updateVisibleIds(eq(Collections.emptyList()), eq(TAB1_ID));
    }

    @Test
    public void testTabOnBackgroundColorChanged() {
        mModel.set(LayoutTab.BACKGROUND_COLOR, Color.WHITE);

        // Index 0 is the TabObserver for mTab1.
        doReturn(Color.RED).when(mTopUiThemeColorProvider).getBackgroundColor(mTab1);
        mTabObserverCaptor.getAllValues().get(0).onBackgroundColorChanged(mTab1, Color.RED);

        assertEquals(Color.RED, mModel.get(LayoutTab.BACKGROUND_COLOR));
    }

    @Test
    public void testUpdateLayout() {
        mModel.set(LayoutTab.RENDER_X, 0.0f);
        mModel.set(LayoutTab.RENDER_Y, 0.0f);
        mModel.set(LayoutTab.X, 1.0f);
        mModel.set(LayoutTab.Y, 1.0f);

        mStaticLayout.updateLayout(System.currentTimeMillis(), 1000);

        assertEquals(1.0f, mModel.get(LayoutTab.RENDER_X), 0);
        assertEquals(1.0f, mModel.get(LayoutTab.RENDER_Y), 0);

        mModel.set(LayoutTab.X, 0.3f);
        mModel.set(LayoutTab.Y, 0.3f);

        mStaticLayout.updateLayout(System.currentTimeMillis(), 1000);

        assertEquals(0.0f, mModel.get(LayoutTab.RENDER_X), 0);
        assertEquals(0.0f, mModel.get(LayoutTab.RENDER_Y), 0);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testTabGainsFocusOnPhoneOnLayoutDoneShowing() {
        doReturn(mTabView).when(mTab1).getView();
        doReturn(true).when(mTabView).requestFocus();

        mStaticLayout.doneShowing();
        verify(mTabView).requestFocus();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testTabDoesNotGainFocusOnTabletOnLayoutDoneShowing() {
        mStaticLayout.doneShowing();
        verify(mTabView, never()).requestFocus();
    }
}
