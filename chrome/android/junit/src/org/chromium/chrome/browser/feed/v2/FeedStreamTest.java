// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.feed.shared.stream.Stream.POSITION_NOT_KNOWN;

import android.app.Activity;
import android.util.TypedValue;
import android.view.View;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link FeedStream}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedStreamTest {
    private static final int LOAD_MORE_TRIGGER_LOOKAHEAD = 5;
    private static final int LOAD_MORE_TRIGGER_SCROLL_DISTANCE_DP = 100;
    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private FakeLinearLayoutManager mLayoutManager;
    private FeedStream mFeedStream;

    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private NativePageNavigationDelegate mPageNavigationDelegate;
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private FeedStreamSurface.Natives mFeedStreamSurfaceJniMock;
    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private Supplier<ShareDelegate> mShareDelegateSupplier;

    @Rule
    public JniMocker mocker = new JniMocker();

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mocker.mock(FeedStreamSurfaceJni.TEST_HOOKS, mFeedStreamSurfaceJniMock);
        mocker.mock(FeedServiceBridge.getTestHooksForTesting(), mFeedServiceBridgeJniMock);

        when(mFeedServiceBridgeJniMock.getLoadMoreTriggerLookahead())
                .thenReturn(LOAD_MORE_TRIGGER_LOOKAHEAD);
        when(mFeedServiceBridgeJniMock.getLoadMoreTriggerScrollDistanceDp())
                .thenReturn(LOAD_MORE_TRIGGER_SCROLL_DISTANCE_DP);
        // Surfaces won't open until after startup.
        FeedStreamSurface.startup();
        mFeedStream = new FeedStream(mActivity, false, mSnackbarManager, mPageNavigationDelegate,
                mBottomSheetController, /* isPlaceholderShown= */ false, mWindowAndroid,
                mShareDelegateSupplier);
        mFeedStream.onCreate(null);
        mRecyclerView = (RecyclerView) mFeedStream.getView();
        mLayoutManager = new FakeLinearLayoutManager(mActivity);
        mRecyclerView.setLayoutManager(mLayoutManager);

        // Print logs to stdout.
        ShadowLog.stream = System.out;
    }

    @Test
    public void testIsChildAtPositionVisible() {
        mLayoutManager.setFirstVisiblePosition(0);
        mLayoutManager.setLastVisiblePosition(1);
        assertThat(mFeedStream.isChildAtPositionVisible(-2)).isFalse();
        assertThat(mFeedStream.isChildAtPositionVisible(-1)).isFalse();
        assertThat(mFeedStream.isChildAtPositionVisible(0)).isTrue();
        assertThat(mFeedStream.isChildAtPositionVisible(1)).isTrue();
        assertThat(mFeedStream.isChildAtPositionVisible(2)).isFalse();
    }

    @Test
    public void testIsChildAtPositionVisible_nothingVisible() {
        assertThat(mFeedStream.isChildAtPositionVisible(0)).isFalse();
    }

    @Test
    public void testIsChildAtPositionVisible_validTop() {
        mLayoutManager.setFirstVisiblePosition(0);
        assertThat(mFeedStream.isChildAtPositionVisible(0)).isFalse();
    }

    @Test
    public void testIsChildAtPositionVisible_validBottom() {
        mLayoutManager.setLastVisiblePosition(1);
        assertThat(mFeedStream.isChildAtPositionVisible(0)).isFalse();
    }

    @Test
    public void testGetChildTopAt_noVisibleChild() {
        assertThat(mFeedStream.getChildTopAt(0)).isEqualTo(POSITION_NOT_KNOWN);
    }

    @Test
    public void testGetChildTopAt_noChild() {
        mLayoutManager.setFirstVisiblePosition(0);
        mLayoutManager.setLastVisiblePosition(1);
        assertThat(mFeedStream.getChildTopAt(0)).isEqualTo(POSITION_NOT_KNOWN);
    }

    @Test
    public void testGetChildTopAt() {
        mLayoutManager.setFirstVisiblePosition(0);
        mLayoutManager.setLastVisiblePosition(1);
        View view = new FrameLayout(mActivity);
        mLayoutManager.addChildToPosition(0, view);

        assertThat(mFeedStream.getChildTopAt(0)).isEqualTo(view.getTop());
    }

    @Test
    public void testSurfaceNotOpenedInitially() {
        Assert.assertFalse(mFeedStream.mFeedStreamSurface.isOpened());
    }

    @Test
    public void testSurfaceOpenedAfterSetStreamContentVisibility() {
        mFeedStream.onShow();
        mFeedStream.setStreamContentVisibility(true);

        Assert.assertTrue(mFeedStream.mFeedStreamSurface.isOpened());
    }

    @Test
    public void testSurfaceNotOpenedAfterSetStreamContentVisibilityIfNotShow() {
        mFeedStream.setStreamContentVisibility(true);

        Assert.assertFalse(mFeedStream.mFeedStreamSurface.isOpened());
    }

    @Test
    public void testSurfaceOpenedOnShow() {
        mFeedStream.setStreamContentVisibility(true);
        mFeedStream.onShow();

        Assert.assertTrue(mFeedStream.mFeedStreamSurface.isOpened());
    }

    @Test
    public void testSurfaceNotOnShowIfStreamContentNotVisible() {
        mFeedStream.onShow();

        Assert.assertFalse(mFeedStream.mFeedStreamSurface.isOpened());
    }

    @Test
    public void testSurfaceClosedOnHide() {
        mFeedStream.setStreamContentVisibility(true);
        mFeedStream.onShow();

        mFeedStream.onHide();

        Assert.assertFalse(mFeedStream.mFeedStreamSurface.isOpened());
    }

    @Test
    public void testSurfaceClosedOnContentNotVisible() {
        mFeedStream.setStreamContentVisibility(true);
        mFeedStream.onShow();

        mFeedStream.setStreamContentVisibility(false);

        Assert.assertFalse(mFeedStream.mFeedStreamSurface.isOpened());
    }

    @Test
    public void testCheckScrollingForLoadMore_StreamContentHidden() {
        // By default, stream content is not visible.
        final int triggerDistance = getLoadMoreTriggerScrollDistance();
        mFeedStream.checkScrollingForLoadMore(triggerDistance);
        verify(mFeedStreamSurfaceJniMock, never())
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));
    }

    @Test
    public void testCheckScrollingForLoadMore_StreamContentVisible() {
        mFeedStream.onShow();
        mFeedStream.setStreamContentVisibility(true);
        final int triggerDistance = getLoadMoreTriggerScrollDistance();
        final int itemCount = 10;

        // loadMore not triggered due to not enough accumulated scrolling distance.
        mFeedStream.checkScrollingForLoadMore(triggerDistance / 2);
        verify(mFeedStreamSurfaceJniMock, never())
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));

        // loadMore not triggered due to last visible item not falling into lookahead range.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD - 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStream.checkScrollingForLoadMore(triggerDistance / 2);
        verify(mFeedStreamSurfaceJniMock, never())
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStream.checkScrollingForLoadMore(triggerDistance / 2);
        verify(mFeedStreamSurfaceJniMock)
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));
    }

    @Test
    public void testCheckScrollingForLoadMore_LoadMoreAfterHide() {
        mFeedStream.onShow();
        mFeedStream.setStreamContentVisibility(true);
        final int triggerDistance = getLoadMoreTriggerScrollDistance();
        final int itemCount = 10;

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStream.checkScrollingForLoadMore(triggerDistance);
        verify(mFeedStreamSurfaceJniMock)
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));

        // loadMore triggered again after hide&show.
        mFeedStream.checkScrollingForLoadMore(-triggerDistance);
        mFeedStream.onHide();
        mFeedStream.onShow();

        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStream.checkScrollingForLoadMore(triggerDistance);
        verify(mFeedStreamSurfaceJniMock)
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));
    }

    @Test
    public void testSerializeScrollState() {
        FeedStream.ScrollState state = new FeedStream.ScrollState();
        state.position = 2;
        state.lastPosition = 4;
        state.offset = 50;

        FeedStream.ScrollState deserializedState = FeedStream.ScrollState.fromJson(state.toJson());

        Assert.assertEquals(2, deserializedState.position);
        Assert.assertEquals(4, deserializedState.lastPosition);
        Assert.assertEquals(50, deserializedState.offset);
        Assert.assertEquals(state.toJson(), deserializedState.toJson());
    }

    @Test
    public void testGetSavedInstanceStateString() {
        mFeedStream.onShow();
        mFeedStream.setStreamContentVisibility(true);

        View view1 = new FrameLayout(mActivity);
        mLayoutManager.addChildToPosition(0, new FrameLayout(mActivity));
        mLayoutManager.addChildToPosition(1, view1);
        mLayoutManager.addChildToPosition(2, new FrameLayout(mActivity));
        mLayoutManager.addChildToPosition(3, new FrameLayout(mActivity));

        mLayoutManager.setFirstVisiblePosition(1);
        mLayoutManager.setLastVisiblePosition(3);

        String json = mFeedStream.getSavedInstanceStateString();
        Assert.assertNotEquals("", json);

        FeedStream.ScrollState state = FeedStream.ScrollState.fromJson(json);
        Assert.assertEquals(1, state.position);
        Assert.assertEquals(3, state.lastPosition);
    }

    @Test
    public void testScrollStateFromInvalidJson() {
        Assert.assertEquals(null, FeedStream.ScrollState.fromJson("{{=xcg"));
    }

    private int getLoadMoreTriggerScrollDistance() {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                LOAD_MORE_TRIGGER_SCROLL_DISTANCE_DP,
                mRecyclerView.getResources().getDisplayMetrics());
    }
}
