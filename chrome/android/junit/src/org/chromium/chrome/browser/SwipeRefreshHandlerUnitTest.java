// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;

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

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.side_ui.SideUiObserver;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.third_party.android.swiperefresh.SwipeRefreshLayout;
import org.chromium.ui.OverscrollAction;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

import java.util.Map;

/** Unit tests for {@link SwipeRefreshHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SwipeRefreshHandlerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private SideUiStateProvider mSideUiStateProvider;
    @Mock private WebContents mWebContents;

    @Captor private ArgumentCaptor<SideUiObserver> mSideUiObserverCaptor;

    private ContentView mContentView;
    private TestSwipeRefreshLayout mSwipeRefreshLayout;
    private Activity mActivity;

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
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContentView = ContentView.createContentView(mActivity, /* webContents= */ null);
        mSwipeRefreshLayout = new TestSwipeRefreshLayout(mActivity);
        when(mTab.getContext()).thenReturn(mActivity);
        when(mTab.getUserDataHost()).thenReturn(new UserDataHost());
        when(mTab.getContentView()).thenReturn(mContentView);
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
    public void testOnSideUiSpecsChanged() {
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
                .onSideUiSpecsChanged(
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

    private void assertHorizontalOffsets(int left, int right) {
        assertEquals(left, mSwipeRefreshLayout.mLeftOffset);
        assertEquals(right, mSwipeRefreshLayout.mRightOffset);
    }
}
