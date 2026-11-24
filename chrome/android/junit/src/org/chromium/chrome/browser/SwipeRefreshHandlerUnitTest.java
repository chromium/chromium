// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProviderSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcherImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.side_ui.SideUiObserver;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;
import org.chromium.ui.OverscrollAction;
import org.chromium.ui.OverscrollActivationStatus;
import org.chromium.ui.base.BackGestureEventSwipeEdge;
import org.chromium.ui.base.WindowAndroid;

import java.util.Map;

/** Unit tests for {@link SwipeRefreshHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SwipeRefreshHandlerUnitTest {
    private static final int TOP_CONTROLS_HEIGHT = 56;
    private static final int HIDDEN_TOP_CONTROLS_OFFSET = -TOP_CONTROLS_HEIGHT;
    private static final int PARTIAL_REVEAL_TOP_CONTROLS_OFFSET = -14;
    private static final int FULL_REVEAL_TOP_CONTROLS_OFFSET = 0;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TabDelegateFactory mTabDelegateFactory;
    @Mock private SideUiStateProvider mSideUiStateProvider;

    @Captor private ArgumentCaptor<SideUiObserver> mSideUiObserverCaptor;

    private ContentView mContentView;
    private TestSwipeRefreshLayout mSwipeRefreshLayout;
    private Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private BrowserControlsManager mBrowserControlsManager;
    private MockWebContents mWebContents;
    private MockTab mTab;
    // Held here because UnownedUserDataHost keeps only weak references to its data.
    private MonotonicObservableSupplier<BrowserControlsStateProvider> mProviderSupplier;

    /** Records the horizontal offsets, which SwipeRefreshLayout does not expose. */
    private static class TestSwipeRefreshLayout extends SwipeRefreshLayout {
        private int mLeftOffset;
        private int mRightOffset;

        TestSwipeRefreshLayout(Context context) {
            super(context);
        }

        @Override
        public void setHorizontalOffsets(int leftOffset, int rightOffset) {
            super.setHorizontalOffsets(leftOffset, rightOffset);
            mLeftOffset = leftOffset;
            mRightOffset = rightOffset;
        }
    }

    @Before
    public void setUp() {
        TabTestUtils.mockTabJni();
        TabTestUtils.mockTabBrowserControlsConstraintsHelperJni();
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContentView = ContentView.createContentView(mActivity, /* webContents= */ null);
        mSwipeRefreshLayout = new TestSwipeRefreshLayout(mActivity);
        mWebContents = new MockWebContents();

        mWindowAndroid = new WindowAndroid(mActivity, /* occlusionTrackingAllowed= */ false);
        mBrowserControlsManager =
                new BrowserControlsManager(
                        mActivity,
                        ControlsPosition.TOP,
                        new MultiWindowModeStateDispatcherImpl(mActivity));
        mBrowserControlsManager.setTopControlsHeight(
                TOP_CONTROLS_HEIGHT, /* topControlsMinHeight= */ 0);
        mProviderSupplier = ObservableSuppliers.createMonotonic(mBrowserControlsManager);
        BrowserControlsStateProviderSupplier.attach(
                mWindowAndroid.getUnownedUserDataHost(), mProviderSupplier);

        when(mTabDelegateFactory.createBrowserControlsVisibilityDelegate(any()))
                .thenReturn(mBrowserControlsManager.getBrowserVisibilityDelegate());

        mTab =
                new MockTab(1, mProfile) {
                    @Override
                    public @Nullable ContentView getContentView() {
                        return mContentView;
                    }

                    @Override
                    public boolean isHidden() {
                        return false;
                    }
                };
        mTab.setIsTabInBrowser(true);
        mTab.setWebContentsOverrideForTesting(mWebContents);
        TabBrowserControlsConstraintsHelper.createForTab(mTab);
        mTab.updateAttachment(mWindowAndroid, mTabDelegateFactory);
        mTab.setIsInitialized(true);
        TabBrowserControlsConstraintsHelper.updateVisibilityDelegate(mTab);
        setTopControlsOffset(FULL_REVEAL_TOP_CONTROLS_OFFSET);
    }

    @After
    public void tearDown() {
        mTab.destroy();
        mBrowserControlsManager.destroy();
        mWindowAndroid.destroy();
        ShadowLooper.runUiThreadTasks();
    }

    @Test
    public void testSetSideUiStateProvider_attachesObserverAndUpdatesLayout() {
        SideUiSpecs specs =
                new SideUiSpecs(
                        Map.of(
                                AnchorSide.LEFT,
                                new SideUiSize(120, HeightType.WEB_CONTENTS),
                                AnchorSide.RIGHT,
                                new SideUiSize(240, HeightType.WEB_CONTENTS)));
        when(mSideUiStateProvider.getCurrentSideUiSpecs()).thenReturn(specs);

        SwipeRefreshHandler handler =
                SwipeRefreshHandler.from(mTab, context -> mSwipeRefreshLayout);
        handler.setSideUiStateProvider(mSideUiStateProvider);

        assertSame(mSideUiStateProvider, handler.getSideUiStateProviderForTesting());
        verify(mSideUiStateProvider).addObserver(mSideUiObserverCaptor.capture());

        handler.initWebContents(mWebContents);
        handler.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT);

        assertHorizontalOffsets(120, 240);
    }

    @Test
    public void testOnSideUiSpecsChangeStarted() {
        SwipeRefreshHandler handler =
                SwipeRefreshHandler.from(mTab, context -> mSwipeRefreshLayout);
        handler.setSideUiStateProvider(mSideUiStateProvider);
        verify(mSideUiStateProvider).addObserver(mSideUiObserverCaptor.capture());

        handler.initWebContents(mWebContents);
        handler.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT);

        SideUiSpecs newSpecs =
                new SideUiSpecs(
                        Map.of(
                                AnchorSide.LEFT,
                                new SideUiSize(150, HeightType.WEB_CONTENTS),
                                AnchorSide.RIGHT,
                                new SideUiSize(250, HeightType.WEB_CONTENTS)));
        mSideUiObserverCaptor
                .getValue()
                .onSideUiSpecsChangeStarted(
                        newSpecs,
                        UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));

        assertHorizontalOffsets(150, 250);
    }

    @Test
    public void testSetSideUiStateProvider_null_unregistersObserver() {
        SideUiSpecs specs =
                new SideUiSpecs(
                        Map.of(
                                AnchorSide.LEFT,
                                new SideUiSize(100, HeightType.WEB_CONTENTS),
                                AnchorSide.RIGHT,
                                new SideUiSize(200, HeightType.WEB_CONTENTS)));
        when(mSideUiStateProvider.getCurrentSideUiSpecs()).thenReturn(specs);

        SwipeRefreshHandler handler =
                SwipeRefreshHandler.from(mTab, context -> mSwipeRefreshLayout);
        handler.setSideUiStateProvider(mSideUiStateProvider);
        verify(mSideUiStateProvider).addObserver(mSideUiObserverCaptor.capture());

        handler.initWebContents(mWebContents);
        handler.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT);
        assertHorizontalOffsets(100, 200);

        handler.setSideUiStateProvider(null);
        assertNull(handler.getSideUiStateProviderForTesting());
        verify(mSideUiStateProvider).removeObserver(mSideUiObserverCaptor.getValue());
        assertHorizontalOffsets(0, 0);
    }

    @Test
    public void testCleanupWebContents_removesObserverAndResetsWidths() {
        SideUiSpecs specs =
                new SideUiSpecs(
                        Map.of(
                                AnchorSide.LEFT,
                                new SideUiSize(100, HeightType.WEB_CONTENTS),
                                AnchorSide.RIGHT,
                                new SideUiSize(200, HeightType.WEB_CONTENTS)));
        when(mSideUiStateProvider.getCurrentSideUiSpecs()).thenReturn(specs);

        SwipeRefreshHandler handler =
                SwipeRefreshHandler.from(mTab, context -> mSwipeRefreshLayout);
        handler.setSideUiStateProvider(mSideUiStateProvider);
        verify(mSideUiStateProvider).addObserver(mSideUiObserverCaptor.capture());

        handler.initWebContents(mWebContents);
        handler.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT);
        assertHorizontalOffsets(100, 200);

        handler.cleanupWebContents(mWebContents);
        assertNull(handler.getSideUiStateProviderForTesting());
        verify(mSideUiStateProvider).removeObserver(mSideUiObserverCaptor.getValue());
        assertHorizontalOffsets(0, 0);
    }

    @Test
    public void testDestroyInternal_removesObserver() {
        SwipeRefreshHandler handler =
                SwipeRefreshHandler.from(mTab, context -> mSwipeRefreshLayout);
        handler.setSideUiStateProvider(mSideUiStateProvider);
        verify(mSideUiStateProvider).addObserver(mSideUiObserverCaptor.capture());

        handler.destroyInternal();
        assertNull(handler.getSideUiStateProviderForTesting());
        verify(mSideUiStateProvider).removeObserver(mSideUiObserverCaptor.getValue());
    }

    @Test
    @DisabledTest(message = "crbug.com/579544393")
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testPullToRefresh_SuppressedWhenControlsNotFullyVisible() {
        SwipeRefreshHandler handler =
                SwipeRefreshHandler.from(mTab, context -> mSwipeRefreshLayout);
        handler.initWebContents(mWebContents);

        // Suppressed when controls are fully hidden; pull/release before lazy layout creation must
        // be ignored because mSwipeType is reset to NONE.
        setTopControlsOffset(HIDDEN_TOP_CONTROLS_OFFSET);
        assertFalse(
                handler.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT));
        performPullAndRelease(handler);
        assertNull(mSwipeRefreshLayout.getParent());
        assertFalse(mSwipeRefreshLayout.isRefreshing());

        // Also suppressed when controls are only partially revealed mid-pull.
        setTopControlsOffset(PARTIAL_REVEAL_TOP_CONTROLS_OFFSET);
        assertFalse(
                handler.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT));
        performPullAndRelease(handler);
        assertNull(mSwipeRefreshLayout.getParent());
        assertFalse(mSwipeRefreshLayout.isRefreshing());

        // Once controls are fully visible, the full pull-to-refresh gesture triggers refreshing.
        setTopControlsOffset(FULL_REVEAL_TOP_CONTROLS_OFFSET);
        assertTrue(handler.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT));
        assertSame(mContentView, mSwipeRefreshLayout.getParent());
        performPullAndRelease(handler);
        assertTrue(mSwipeRefreshLayout.isRefreshing());

        // Reset and verify a subsequent suppressed gesture does not re-trigger refreshing.
        handler.reset();
        assertFalse(mSwipeRefreshLayout.isRefreshing());
        setTopControlsOffset(PARTIAL_REVEAL_TOP_CONTROLS_OFFSET);
        assertFalse(
                handler.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT));
        performPullAndRelease(handler);
        assertFalse(mSwipeRefreshLayout.isRefreshing());
    }

    @Test
    @DisabledTest(message = "crbug.com/579544393")
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testPullToRefresh_TopControlsAtPositiveMinHeight() {
        SwipeRefreshHandler handler =
                SwipeRefreshHandler.from(mTab, context -> mSwipeRefreshLayout);
        handler.initWebContents(mWebContents);

        // When a pinned strip sets 0 < minHeight < height and controls scroll to minHeight, the
        // Omnibox itself is scrolled off (offset < 0), so the first pull is suppressed.
        int pinnedStripMinHeight = 20;
        mBrowserControlsManager.setTopControlsHeight(TOP_CONTROLS_HEIGHT, pinnedStripMinHeight);
        setTopControlsOffset(-(TOP_CONTROLS_HEIGHT - pinnedStripMinHeight));
        assertTrue(mBrowserControlsManager.areBrowserControlsAtMinHeight());
        assertFalse(
                handler.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT));
        performPullAndRelease(handler);
        assertNull(mSwipeRefreshLayout.getParent());
        assertFalse(mSwipeRefreshLayout.isRefreshing());

        // When minHeight == height (e.g. locked top controls on large tablets), controls at
        // minHeight are already fully visible (offset == 0), so pull-to-refresh succeeds.
        mBrowserControlsManager.setTopControlsHeight(TOP_CONTROLS_HEIGHT, TOP_CONTROLS_HEIGHT);
        setTopControlsOffset(FULL_REVEAL_TOP_CONTROLS_OFFSET);
        assertTrue(mBrowserControlsManager.areBrowserControlsAtMinHeight());
        assertTrue(handler.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT));
        performPullAndRelease(handler);
        assertTrue(mSwipeRefreshLayout.isRefreshing());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION + ":suppress_pull_to_refresh_reveal/false")
    public void testPullToRefresh_AllowedWhenSuppressParamDisabled() {
        assertPullToRefreshSucceedsWithHiddenControls();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION + ":arm/minimize_omnibox")
    public void testPullToRefresh_AllowedInMiniOmniboxArm() {
        assertPullToRefreshSucceedsWithHiddenControls();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testPullToRefresh_AllowedOutsideTabbedBrowser() {
        // Custom Tabs, TWAs and WebAPKs never keep the Omnibox hidden across navigations.
        mTab.setIsTabInBrowser(false);

        assertPullToRefreshSucceedsWithHiddenControls();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testPullToRefresh_AllowedWithBottomControls() {
        mBrowserControlsManager.setControlsPosition(
                ControlsPosition.BOTTOM,
                /* newTopControlsHeight= */ 0,
                /* newTopControlsMinHeight= */ 0,
                /* newRendererTopControlsOffset= */ 0,
                /* newBottomControlsHeight= */ TOP_CONTROLS_HEIGHT,
                /* newBottomControlsMinHeight= */ 0,
                /* newRendererBottomControlsOffset= */ 0);
        // setControlsPosition() resets renderer offsets to 0 when no active tab is attached, so
        // apply the hidden bottom offset explicitly via setPositionsForTab().
        mBrowserControlsManager.setPositionsForTab(
                /* topControlsOffset= */ 0,
                /* bottomControlsOffset= */ TOP_CONTROLS_HEIGHT,
                /* topContentOffset= */ 0,
                /* topControlsMinHeightOffset= */ 0,
                /* bottomControlsMinHeightOffset= */ 0);
        assertTrue(BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsManager));

        SwipeRefreshHandler handler =
                SwipeRefreshHandler.from(mTab, context -> mSwipeRefreshLayout);
        handler.initWebContents(mWebContents);

        assertTrue(handler.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT));
        performPullAndRelease(handler);
        assertTrue(mSwipeRefreshLayout.isRefreshing());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testPullToRefresh_AllowedWhenControlsForceHidden() {
        // Fullscreen video forces BrowserControlsState.HIDDEN so the Omnibox cannot scroll into
        // view; single-pull reload must remain available.
        mBrowserControlsManager.getBrowserVisibilityDelegate().set(BrowserControlsState.HIDDEN);
        assertEquals(
                BrowserControlsState.HIDDEN,
                TabBrowserControlsConstraintsHelper.getConstraints(mTab));

        assertPullToRefreshSucceedsWithHiddenControls();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testPullToRefresh_AllowedOnAutomotive() {
        DeviceInfo.setIsAutomotiveForTesting(true);

        assertPullToRefreshSucceedsWithHiddenControls();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testPullToRefresh_AllowedOnTv() {
        DeviceInfo.setIsTVForTesting(true);

        assertPullToRefreshSucceedsWithHiddenControls();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testPullToRefresh_AllowedOnXr() {
        DeviceInfo.setIsXrForTesting(true);

        assertPullToRefreshSucceedsWithHiddenControls();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CLANK_OMNIBOX_MINIMIZATION)
    public void testPullToRefresh_AllowedWhenFlagDisabled() {
        assertPullToRefreshSucceedsWithHiddenControls();
    }

    private void setTopControlsOffset(int topControlsOffset) {
        int contentOffset = topControlsOffset + mBrowserControlsManager.getTopControlsHeight();
        int topControlsMinHeightOffset = mBrowserControlsManager.getTopControlsMinHeight();
        TabTestUtils.setBrowserControlsOffsets(mTab, topControlsOffset, contentOffset);
        mBrowserControlsManager.setPositionsForTab(
                topControlsOffset,
                /* bottomControlsOffset= */ 0,
                contentOffset,
                topControlsMinHeightOffset,
                /* bottomControlsMinHeightOffset= */ 0);
    }

    private static void performPullAndRelease(SwipeRefreshHandler handler) {
        for (int i = 0; i < 4; i++) {
            handler.pull(0, 100f);
        }
        handler.release(OverscrollActivationStatus.ALLOW_ACTIVATION);
    }

    private void assertPullToRefreshSucceedsWithHiddenControls() {
        setTopControlsOffset(HIDDEN_TOP_CONTROLS_OFFSET);
        SwipeRefreshHandler handler =
                SwipeRefreshHandler.from(mTab, context -> mSwipeRefreshLayout);
        handler.initWebContents(mWebContents);
        assertTrue(handler.start(OverscrollAction.PULL_TO_REFRESH, BackGestureEventSwipeEdge.LEFT));
        performPullAndRelease(handler);
        assertTrue(mSwipeRefreshLayout.isRefreshing());
    }

    private void assertHorizontalOffsets(int left, int right) {
        assertEquals(left, mSwipeRefreshLayout.mLeftOffset);
        assertEquals(right, mSwipeRefreshLayout.mRightOffset);
    }
}
