// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;

import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.util.ArrayMap;
import android.util.TypedValue;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatTextView;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.FollowResults;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge.UnfollowResults;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.feed.webfeed.WebFeedRecommendationFollowAcceleratorController;
import org.chromium.chrome.browser.feed.webfeed.WebFeedSubscriptionRequestStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler.OpenMode;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler.OpenUrlOptions;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler.WebFeedFollowUpdate;
import org.chromium.chrome.browser.xsurface.feed.FeedActionsHandler;
import org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScope;
import org.chromium.chrome.browser.xsurface.feed.FeedUserInteractionReliabilityLogger.ClosedReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feed.proto.FeedUiProto;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link FeedStream}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
// TODO(crbug.com/40182398): Rewrite using paused loop. See crbug for details.
@LooperMode(LooperMode.Mode.LEGACY)
public class FeedStreamTest {
    private static final int LOAD_MORE_TRIGGER_LOOKAHEAD = 5;
    private static final int LOAD_MORE_TRIGGER_SCROLL_DISTANCE_DP = 100;
    private static final String TEST_DATA = "test";
    private static final String TEST_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    private static final String HEADER_PREFIX = "header";
    private static final OpenUrlOptions DEFAULT_OPEN_URL_OPTIONS = new OpenUrlOptions() {};

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private FakeLinearLayoutManager mLayoutManager;
    private FeedStream mFeedStream;
    private FeedListContentManager mContentManager;

    @Mock private FeedSurfaceRendererBridge mFeedSurfaceRendererBridgeMock;
    @Mock private FeedSurfaceRendererBridge.Natives mFeedRendererJniMock;
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private FeedReliabilityLoggingBridge.Natives mFeedReliabilityLoggingBridgeJniMock;

    @Mock private SnackbarManager mSnackbarManager;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;
    private StubSnackbarController mSnackbarController = new StubSnackbarController();
    @Mock private Runnable mMockRunnable;
    @Mock private Callback<Boolean> mMockRefreshCallback;
    @Mock private FeedStream.ShareHelperWrapper mShareHelper;
    @Mock private Profile mProfileMock;
    @Mock private HybridListRenderer mRenderer;
    @Mock private FeedSurfaceScope mSurfaceScope;
    @Mock private RecyclerView.Adapter mAdapter;
    @Mock private FeedReliabilityLogger mReliabilityLogger;
    @Mock private FeedActionDelegate mActionDelegate;
    @Mock WebFeedBridge.Natives mWebFeedBridgeJni;

    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;
    @Captor private ArgumentCaptor<Callback<FollowResults>> mFollowResultsCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<UnfollowResults>> mUnfollowResultsCallbackCaptor;
    @Mock private WebFeedFollowUpdate.Callback mWebFeedFollowUpdateCallback;
    @Mock private FeedContentFirstLoadWatcher mFeedContentFirstLoadWatcher;
    @Mock private Stream.StreamsMediator mStreamsMediator;

    @Rule public JniMocker mocker = new JniMocker();
    // Enable the Features class, so we can call code which checks to see if features are enabled
    // without crashing.

    private FeedSurfaceRendererBridge.Renderer mBridgeRenderer;

    class FeedSurfaceRendererBridgeFactory implements FeedSurfaceRendererBridge.Factory {
        @Override
        public FeedSurfaceRendererBridge create(
                FeedSurfaceRendererBridge.Renderer renderer,
                FeedReliabilityLoggingBridge reliabilityLoggingBridge,
                @StreamKind int streamKind,
                SingleWebFeedParameters webFeedParameters) {
            mBridgeRenderer = renderer;
            return mFeedSurfaceRendererBridgeMock;
        }
    }

    private void setFeatureOverrides(boolean feedLoadingPlaceholderOn) {
        Map<String, Boolean> overrides = new ArrayMap<>();
        overrides.put(ChromeFeatureList.FEED_LOADING_PLACEHOLDER, feedLoadingPlaceholderOn);
        overrides.put(ChromeFeatureList.FEED_CONTAINMENT, false);
        FeatureList.setTestFeatures(overrides);
    }

    private static HistogramWatcher expectFeedRecordForLoadMoreTrigger(
            @StreamKind int streamKind, int itemCount, int numCardsRemaining) {
        return HistogramWatcher.newBuilder()
                .expectIntRecord(FeedUma.TOTAL_CARDS_HISTOGRAM_NAMES[streamKind - 1], itemCount)
                .expectIntRecord(
                        FeedUma.OFFSET_FROM_END_OF_STREAM_HISTOGRAM_NAMES[streamKind - 1],
                        numCardsRemaining)
                .build();
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivity = mActivityController.get();

        mocker.mock(FeedSurfaceRendererBridgeJni.TEST_HOOKS, mFeedRendererJniMock);
        mocker.mock(FeedServiceBridge.getTestHooksForTesting(), mFeedServiceBridgeJniMock);
        mocker.mock(
                FeedReliabilityLoggingBridge.getTestHooksForTesting(),
                mFeedReliabilityLoggingBridgeJniMock);
        mocker.mock(WebFeedBridgeJni.TEST_HOOKS, mWebFeedBridgeJni);
        ProfileManager.setLastUsedProfileForTesting(mProfileMock);

        when(mFeedServiceBridgeJniMock.getLoadMoreTriggerLookahead())
                .thenReturn(LOAD_MORE_TRIGGER_LOOKAHEAD);
        when(mFeedServiceBridgeJniMock.getLoadMoreTriggerScrollDistanceDp())
                .thenReturn(LOAD_MORE_TRIGGER_SCROLL_DISTANCE_DP);
        mFeedStream =
                new FeedStream(
                        mActivity,
                        mProfileMock,
                        mSnackbarManager,
                        mBottomSheetController,
                        mWindowAndroid,
                        mShareDelegateSupplier,
                        /* isInterestFeed= */ StreamKind.FOR_YOU,
                        mActionDelegate,
                        mFeedContentFirstLoadWatcher,
                        mStreamsMediator,
                        /* SingleWebFeedHelper= */ null,
                        new FeedSurfaceRendererBridgeFactory());
        mRecyclerView = new RecyclerView(mActivity);
        mRecyclerView.setAdapter(mAdapter);
        mContentManager = new FeedListContentManager();
        mLayoutManager = new FakeLinearLayoutManager(mActivity);
        mRecyclerView.setLayoutManager(mLayoutManager);
        when(mRenderer.getListLayoutHelper()).thenReturn(mLayoutManager);

        setFeatureOverrides(/* feedLoadingPlaceholderOn= */ true);

        // Print logs to stdout.
        ShadowLog.stream = System.out;
    }

    @Test
    public void testBindUnbind_keepsHeaderViews() {
        // Have header content.
        createHeaderContent(3);
        bindToView();

        // Add feed content.
        FeedUiProto.StreamUpdate update =
                FeedUiProto.StreamUpdate.newBuilder()
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                        .build();
        mBridgeRenderer.onStreamUpdated(update.toByteArray());

        mFeedStream.unbind(false, false);
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(HEADER_PREFIX + "0", mContentManager.getContent(0).getKey());
        assertEquals(HEADER_PREFIX + "1", mContentManager.getContent(1).getKey());
        assertEquals(HEADER_PREFIX + "2", mContentManager.getContent(2).getKey());
    }

    @Test
    public void testBindUnbind_HeaderViewCountChangeAfterBind() {
        // Have header content.
        createHeaderContent(3);
        bindToView();

        // Add feed content.
        FeedUiProto.StreamUpdate update =
                FeedUiProto.StreamUpdate.newBuilder()
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                        .build();
        mBridgeRenderer.onStreamUpdated(update.toByteArray());

        // Add header content.
        createHeaderContent(2);
        mFeedStream.notifyNewHeaderCount(5);

        mFeedStream.unbind(false, false);

        assertEquals(5, mContentManager.getItemCount());
        assertEquals(HEADER_PREFIX + "0", mContentManager.getContent(0).getKey());
        assertEquals(HEADER_PREFIX + "1", mContentManager.getContent(1).getKey());
        assertEquals(HEADER_PREFIX + "0", mContentManager.getContent(2).getKey());
        assertEquals(HEADER_PREFIX + "1", mContentManager.getContent(3).getKey());
        assertEquals(HEADER_PREFIX + "2", mContentManager.getContent(4).getKey());
    }

    @Test
    @SmallTest
    public void testBindUnbind_shouldPlaceSpacerTrue() {
        createHeaderContent(1);
        bindToView();

        // Add feed content.
        FeedUiProto.StreamUpdate update =
                FeedUiProto.StreamUpdate.newBuilder()
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                        .build();
        mBridgeRenderer.onStreamUpdated(update.toByteArray());

        mFeedStream.unbind(true, false);

        assertEquals(2, mContentManager.getItemCount());
        assertEquals(HEADER_PREFIX + "0", mContentManager.getContent(0).getKey());
        assertEquals(1, mContentManager.findContentPositionByKey("Spacer"));
        verify(mFeedContentFirstLoadWatcher).nonNativeContentLoaded(anyInt());
    }

    @Test
    @SmallTest
    public void testUnbindBind_shouldPlaceSpacerTrue() {
        createHeaderContent(2);
        bindToView();

        // Add feed content.
        FeedUiProto.StreamUpdate update =
                FeedUiProto.StreamUpdate.newBuilder()
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                        .build();
        mBridgeRenderer.onStreamUpdated(update.toByteArray());
        mFeedStream.unbind(true, false);

        // Bind again with correct headercount.
        mFeedStream.bind(
                mRecyclerView,
                mContentManager,
                null,
                mSurfaceScope,
                mRenderer,
                mReliabilityLogger,
                2);

        // Add different feed content.
        update =
                FeedUiProto.StreamUpdate.newBuilder()
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                        .build();
        mBridgeRenderer.onStreamUpdated(update.toByteArray());

        assertEquals(3, mContentManager.getItemCount());
        assertEquals(HEADER_PREFIX + "0", mContentManager.getContent(0).getKey());
        assertEquals(HEADER_PREFIX + "1", mContentManager.getContent(1).getKey());
        assertEquals(2, mContentManager.findContentPositionByKey("b"));
        // 'a' should've been replaced.
        assertEquals(-1, mContentManager.findContentPositionByKey("a"));
    }

    @Test
    public void testBind() {
        bindToView();
        // Called surfaceOpened.
        verify(mFeedSurfaceRendererBridgeMock).surfaceOpened();
        // Set handlers in contentmanager.
        assertEquals(2, mContentManager.getContextValues(0).size());
        verify(mReliabilityLogger).onBindStream(anyInt(), anyInt());
    }

    @Test
    public void testUnbind() {
        bindToView();
        mFeedStream.unbind(false, /* switchingStream= */ false);
        verify(mFeedSurfaceRendererBridgeMock).surfaceClosed();
        // Unset handlers in contentmanager.
        assertEquals(0, mContentManager.getContextValues(0).size());
        verify(mReliabilityLogger).onUnbindStream(eq(ClosedReason.LEAVE_FEED));
    }

    @Test
    public void testUnbind_ClosedReasonForSwitchStream() {
        bindToView();
        mFeedStream.unbind(false, /* switchingStream= */ true);
        verify(mReliabilityLogger).onUnbindStream(eq(ClosedReason.SWITCH_STREAM));

        bindToView();
        mFeedStream.unbind(false, /* switchingStream= */ false);
        verify(mReliabilityLogger).onUnbindStream(eq(ClosedReason.LEAVE_FEED));
    }

    @Test
    public void testUnbind_ClosedReasonForSuspendApp() {
        bindToView();
        mActivityController.create();
        mActivityController.stop();
        mFeedStream.unbind(false, /* switchingStream= */ false);
        verify(mReliabilityLogger).onUnbindStream(eq(ClosedReason.SUSPEND_APP));

        bindToView();
        mActivityController.start();
        mFeedStream.unbind(false, /* switchingStream= */ false);
        verify(mReliabilityLogger).onUnbindStream(eq(ClosedReason.LEAVE_FEED));
    }

    @Test
    public void testUnbindDismissesSnackbars() {
        bindToView();

        FeedStream.FeedActionsHandlerImpl handler =
                (FeedStream.FeedActionsHandlerImpl)
                        mContentManager.getContextValues(0).get(FeedActionsHandler.KEY);

        handler.showSnackbar(
                "message", "Undo", FeedActionsHandler.SnackbarDuration.SHORT, mSnackbarController);
        verify(mSnackbarManager).showSnackbar(any());
        mFeedStream.unbind(false, false);
        verify(mSnackbarManager, times(1)).dismissSnackbars(any());
    }

    @Test
    @SmallTest
    public void testAddSlicesOnStreamUpdated() {
        bindToView();
        // Add 3 new slices at first.
        FeedUiProto.StreamUpdate update =
                FeedUiProto.StreamUpdate.newBuilder()
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                        .build();
        mBridgeRenderer.onStreamUpdated(update.toByteArray());
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));

        // Add 2 more slices.
        update =
                FeedUiProto.StreamUpdate.newBuilder()
                        .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                        .addUpdatedSlices(createSliceUpdateForExistingSlice("b"))
                        .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("d"))
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("e"))
                        .build();
        mBridgeRenderer.onStreamUpdated(update.toByteArray());
        assertEquals(5, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));
        assertEquals(3, mContentManager.findContentPositionByKey("d"));
        assertEquals(4, mContentManager.findContentPositionByKey("e"));
    }

    @Test
    @SmallTest
    public void testAddNewSlicesWithSameIds() {
        bindToView();
        // Add 2 new slices at first.
        FeedUiProto.StreamUpdate update =
                FeedUiProto.StreamUpdate.newBuilder()
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                        .build();
        mBridgeRenderer.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));

        // Add 2 new slice with same ids as before.
        update =
                FeedUiProto.StreamUpdate.newBuilder()
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                        .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                        .build();
        mBridgeRenderer.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("b"));
        assertEquals(1, mContentManager.findContentPositionByKey("a"));
    }

    @Test
    public void testLoadMore_unbound() {
        // By default, stream content is not visible.
        final int triggerDistance = getLoadMoreTriggerScrollDistance();
        mFeedStream.checkScrollingForLoadMore(triggerDistance);
        verify(mFeedSurfaceRendererBridgeMock, never()).loadMore(any());
    }

    @Test
    public void testLoadMore_bound() {
        bindToView();
        final int triggerDistance = getLoadMoreTriggerScrollDistance();
        final int itemCount = 10;
        final int lookAheadRange = itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD;
        HistogramWatcher histogramWatcher =
                expectFeedRecordForLoadMoreTrigger(
                        mFeedStream.getStreamKind(), itemCount, lookAheadRange - 1);

        // loadMore not triggered due to not enough accumulated scrolling distance.
        mFeedStream.checkScrollingForLoadMore(triggerDistance / 2);
        verify(mFeedSurfaceRendererBridgeMock, never()).loadMore(any(Callback.class));

        // loadMore not triggered due to last visible item not falling into lookahead range.
        mLayoutManager.setLastVisiblePosition(lookAheadRange - 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStream.checkScrollingForLoadMore(triggerDistance / 2);
        verify(mFeedSurfaceRendererBridgeMock, never()).loadMore(any(Callback.class));

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(lookAheadRange + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStream.checkScrollingForLoadMore(triggerDistance / 2);
        verify(mFeedSurfaceRendererBridgeMock).loadMore(any(Callback.class));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testContentAfterUnbind() {
        bindToView();
        final int triggerDistance = getLoadMoreTriggerScrollDistance();
        final int itemCount = 10;
        final int lookAheadRange = itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD;
        HistogramWatcher histogramWatcher =
                expectFeedRecordForLoadMoreTrigger(
                        mFeedStream.getStreamKind(), itemCount, lookAheadRange - 1);

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(lookAheadRange + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStream.checkScrollingForLoadMore(triggerDistance);
        verify(mFeedSurfaceRendererBridgeMock).loadMore(any(Callback.class));

        // loadMore triggered again after hide&show.
        mFeedStream.checkScrollingForLoadMore(-triggerDistance);
        mFeedStream.unbind(false, false);
        bindToView();

        mLayoutManager.setLastVisiblePosition(lookAheadRange + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStream.checkScrollingForLoadMore(triggerDistance);
        verify(mFeedSurfaceRendererBridgeMock).loadMore(any(Callback.class));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testOpenUrlSameTab() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);
        handler.openUrl(OpenMode.SAME_TAB, TEST_URL, DEFAULT_OPEN_URL_OPTIONS);
        verify(mActionDelegate)
                .openSuggestionUrl(
                        eq(org.chromium.ui.mojom.WindowOpenDisposition.CURRENT_TAB),
                        any(),
                        eq(false),
                        anyInt(),
                        eq(handler),
                        any());
    }

    @Test
    @SmallTest
    public void testOpenUrlWithWebFeedRecommendation() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);
        handler.openUrl(
                OpenMode.SAME_TAB,
                TEST_URL,
                new OpenUrlOptions() {
                    @Override
                    public boolean shouldShowWebFeedAccelerator() {
                        return true;
                    }

                    @Override
                    public String webFeedName() {
                        return "someWebFeedName";
                    }
                });
        verify(mActionDelegate)
                .openSuggestionUrl(
                        eq(org.chromium.ui.mojom.WindowOpenDisposition.CURRENT_TAB),
                        mLoadUrlParamsCaptor.capture(),
                        eq(false),
                        anyInt(),
                        eq(handler),
                        any());

        assertEquals(
                "someWebFeedName",
                new String(
                        WebFeedRecommendationFollowAcceleratorController
                                .getWebFeedNameIfInLoadUrlParams(mLoadUrlParamsCaptor.getValue()),
                        StandardCharsets.UTF_8));
    }

    @Test
    @SmallTest
    public void testOpenUrlNotShouldShowWebFeedAccelerator() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);
        handler.openUrl(
                OpenMode.SAME_TAB,
                TEST_URL,
                new OpenUrlOptions() {
                    @Override
                    public boolean shouldShowWebFeedAccelerator() {
                        return false;
                    }

                    @Override
                    public String webFeedName() {
                        return "someWebFeedName";
                    }
                });
        verify(mActionDelegate)
                .openSuggestionUrl(
                        eq(org.chromium.ui.mojom.WindowOpenDisposition.CURRENT_TAB),
                        mLoadUrlParamsCaptor.capture(),
                        eq(false),
                        anyInt(),
                        eq(handler),
                        any());

        assertEquals(
                null,
                WebFeedRecommendationFollowAcceleratorController.getWebFeedNameIfInLoadUrlParams(
                        mLoadUrlParamsCaptor.getValue()));
    }

    @Test
    @SmallTest
    public void testLogLaunchFinishedOnOpenSuggestionUrl() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);
        handler.openUrl(OpenMode.SAME_TAB, TEST_URL, DEFAULT_OPEN_URL_OPTIONS);
        verify(mReliabilityLogger).onOpenCard(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testLogLaunchFinishedOnOpenSuggestionUrlNewTab() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);
        handler.openUrl(OpenMode.NEW_TAB, TEST_URL, DEFAULT_OPEN_URL_OPTIONS);

        // Don't report card opened if the card was opened in a new tab in the background.
        verify(mReliabilityLogger, never()).onOpenCard(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testLogLaunchFinishedOnOpenUrlNewTab() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);
        handler.openUrl(OpenMode.NEW_TAB, TEST_URL, DEFAULT_OPEN_URL_OPTIONS);
        // Don't report card opened if the card was opened in a new tab in the background.
        verify(mReliabilityLogger, never()).onOpenCard(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testLogLaunchFinishedOnOpenSuggestionUrlIncognito() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);
        handler.openUrl(OpenMode.INCOGNITO_TAB, TEST_URL, DEFAULT_OPEN_URL_OPTIONS);
        verify(mReliabilityLogger).onOpenCard(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testOpenUrlInNewTab() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.openUrl(OpenMode.NEW_TAB, TEST_URL, DEFAULT_OPEN_URL_OPTIONS);
        verify(mActionDelegate)
                .openSuggestionUrl(
                        eq(org.chromium.ui.mojom.WindowOpenDisposition.NEW_BACKGROUND_TAB),
                        any(),
                        eq(false),
                        anyInt(),
                        eq(handler),
                        any());
    }

    @Test
    @SmallTest
    public void testOpenUrlNewTabInGroup() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.openUrl(OpenMode.NEW_TAB_IN_GROUP, TEST_URL, DEFAULT_OPEN_URL_OPTIONS);
        verify(mActionDelegate)
                .openSuggestionUrl(
                        eq(org.chromium.ui.mojom.WindowOpenDisposition.NEW_BACKGROUND_TAB),
                        any(),
                        eq(true),
                        anyInt(),
                        eq(handler),
                        any());
    }

    @Test
    @SmallTest
    public void testOpenUrlIncognitoTab() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);
        handler.openUrl(OpenMode.INCOGNITO_TAB, TEST_URL, DEFAULT_OPEN_URL_OPTIONS);
        verify(mActionDelegate)
                .openSuggestionUrl(
                        eq(org.chromium.ui.mojom.WindowOpenDisposition.OFF_THE_RECORD),
                        any(),
                        eq(false),
                        anyInt(),
                        eq(handler),
                        any());
    }

    @Test
    @SmallTest
    public void testShowBottomSheet() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.showBottomSheet(new AppCompatTextView(mActivity), null);
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testDismissBottomSheet() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.showBottomSheet(new AppCompatTextView(mActivity), null);
        mFeedStream.dismissBottomSheet();
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testUpdateWebFeedFollowState_follow_success() throws Exception {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.updateWebFeedFollowState(
                new WebFeedFollowUpdate() {
                    @Override
                    public String webFeedName() {
                        return "webFeed1";
                    }

                    @Override
                    @Nullable
                    public WebFeedFollowUpdate.Callback callback() {
                        return mWebFeedFollowUpdateCallback;
                    }

                    @Override
                    public int webFeedChangeReason() {
                        return WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU;
                    }
                });

        verify(mWebFeedBridgeJni)
                .followWebFeedById(
                        eq("webFeed1".getBytes("UTF8")),
                        eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU),
                        mFollowResultsCallbackCaptor.capture());
        mFollowResultsCallbackCaptor
                .getValue()
                .onResult(new FollowResults(WebFeedSubscriptionRequestStatus.SUCCESS, null));
        verify(mWebFeedFollowUpdateCallback).requestComplete(eq(true));
    }

    @Test
    @SmallTest
    public void testUpdateWebFeedFollowState_follow_null_callback() throws Exception {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.updateWebFeedFollowState(
                new WebFeedFollowUpdate() {
                    @Override
                    public String webFeedName() {
                        return "webFeed1";
                    }
                });

        verify(mWebFeedBridgeJni)
                .followWebFeedById(any(), eq(false), eq(0), mFollowResultsCallbackCaptor.capture());
        // Just make sure no exception is thrown because there is no callback to call.
        mFollowResultsCallbackCaptor
                .getValue()
                .onResult(new FollowResults(WebFeedSubscriptionRequestStatus.SUCCESS, null));
    }

    @Test
    @SmallTest
    public void testUpdateWebFeedFollowState_follow_durable_failure() throws Exception {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.updateWebFeedFollowState(
                new WebFeedFollowUpdate() {
                    @Override
                    public String webFeedName() {
                        return "webFeed1";
                    }

                    @Override
                    public boolean isDurable() {
                        return true;
                    }

                    @Override
                    @Nullable
                    public WebFeedFollowUpdate.Callback callback() {
                        return mWebFeedFollowUpdateCallback;
                    }

                    @Override
                    public int webFeedChangeReason() {
                        return WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU;
                    }
                });

        verify(mWebFeedBridgeJni)
                .followWebFeedById(
                        eq("webFeed1".getBytes("UTF8")),
                        eq(true),
                        eq(WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU),
                        mFollowResultsCallbackCaptor.capture());
        mFollowResultsCallbackCaptor
                .getValue()
                .onResult(new FollowResults(WebFeedSubscriptionRequestStatus.FAILED_OFFLINE, null));
        verify(mWebFeedFollowUpdateCallback).requestComplete(eq(false));
    }

    @Test
    @SmallTest
    public void testUpdateWebFeedFollowState_unfollow_durable_success() throws Exception {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.updateWebFeedFollowState(
                new WebFeedFollowUpdate() {
                    @Override
                    public String webFeedName() {
                        return "webFeed1";
                    }

                    @Override
                    @Nullable
                    public WebFeedFollowUpdate.Callback callback() {
                        return mWebFeedFollowUpdateCallback;
                    }

                    @Override
                    public boolean isFollow() {
                        return false;
                    }

                    @Override
                    public boolean isDurable() {
                        return true;
                    }

                    @Override
                    public int webFeedChangeReason() {
                        return WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU;
                    }
                });

        verify(mWebFeedBridgeJni)
                .unfollowWebFeed(
                        eq("webFeed1".getBytes("UTF8")),
                        eq(true),
                        eq(WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU),
                        mUnfollowResultsCallbackCaptor.capture());
        mUnfollowResultsCallbackCaptor
                .getValue()
                .onResult(new UnfollowResults(WebFeedSubscriptionRequestStatus.SUCCESS));
        // Just make sure no exception is thrown because there is no callback to call.
        verify(mWebFeedFollowUpdateCallback).requestComplete(eq(true));
    }

    @Test
    @SmallTest
    public void testUpdateWebFeedFollowState_unfollow_null_callback() throws Exception {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.updateWebFeedFollowState(
                new WebFeedFollowUpdate() {
                    @Override
                    public String webFeedName() {
                        return "webFeed1";
                    }

                    @Override
                    public boolean isFollow() {
                        return false;
                    }

                    @Override
                    public int webFeedChangeReason() {
                        return WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU;
                    }
                });

        verify(mWebFeedBridgeJni)
                .unfollowWebFeed(
                        any(),
                        eq(false),
                        eq(WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU),
                        mUnfollowResultsCallbackCaptor.capture());
        mUnfollowResultsCallbackCaptor
                .getValue()
                .onResult(new UnfollowResults(WebFeedSubscriptionRequestStatus.SUCCESS));
    }

    @Test
    @SmallTest
    public void testUpdateWebFeedFollowState_unfollow_durable_failure() throws Exception {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.updateWebFeedFollowState(
                new WebFeedFollowUpdate() {
                    @Override
                    public String webFeedName() {
                        return "webFeed1";
                    }

                    @Override
                    @Nullable
                    public WebFeedFollowUpdate.Callback callback() {
                        return mWebFeedFollowUpdateCallback;
                    }

                    @Override
                    public boolean isFollow() {
                        return false;
                    }

                    @Override
                    public boolean isDurable() {
                        return true;
                    }

                    @Override
                    public int webFeedChangeReason() {
                        return WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU;
                    }
                });

        verify(mWebFeedBridgeJni)
                .unfollowWebFeed(
                        eq("webFeed1".getBytes("UTF8")),
                        eq(true),
                        eq(WebFeedBridge.CHANGE_REASON_WEB_PAGE_MENU),
                        mUnfollowResultsCallbackCaptor.capture());
        mUnfollowResultsCallbackCaptor
                .getValue()
                .onResult(new UnfollowResults(WebFeedSubscriptionRequestStatus.FAILED_OFFLINE));
        verify(mWebFeedFollowUpdateCallback).requestComplete(eq(false));
    }

    @Test
    @SmallTest
    public void testAddToReadingList() {
        bindToView();
        String title = "title";
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);
        handler.openUrl(
                OpenMode.READ_LATER,
                TEST_URL,
                new OpenUrlOptions() {
                    @Override
                    public String getTitle() {
                        return title;
                    }
                });

        verify(mFeedSurfaceRendererBridgeMock)
                .reportOtherUserAction(eq(FeedUserActionType.TAPPED_ADD_TO_READING_LIST));
        verify(mActionDelegate).addToReadingList(eq(title), eq(TEST_URL));
    }

    @Test
    @SmallTest
    public void testShowSnackbar() {
        bindToView();
        FeedStream.FeedActionsHandlerImpl handler =
                (FeedStream.FeedActionsHandlerImpl)
                        mContentManager.getContextValues(0).get(FeedActionsHandler.KEY);

        handler.showSnackbar(
                "message", "Undo", FeedActionsHandler.SnackbarDuration.SHORT, mSnackbarController);
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    @SmallTest
    public void testShowSnackbarOnAction() {
        bindToView();
        FeedStream.FeedActionsHandlerImpl handler =
                (FeedStream.FeedActionsHandlerImpl)
                        mContentManager.getContextValues(0).get(FeedActionsHandler.KEY);

        handler.showSnackbar(
                "message", "Undo", FeedActionsHandler.SnackbarDuration.SHORT, mSnackbarController);
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());

        // Tapping on the snackbar action should trigger the onAction on the stub snackbar
        // controller. postTaskAfterWorkComplete() should not execute the runnable until after the
        // stub snackbar runnable is executed.
        mSnackbarCaptor.getValue().getController().onAction("data");
        mFeedStream.getInProgressWorkTrackerForTesting().postTaskAfterWorkComplete(mMockRunnable);
        verify(mMockRunnable, times(0)).run();

        mSnackbarController.mOnActionFinished.run();
        verify(mMockRunnable, times(1)).run();
    }

    @Test
    @SmallTest
    public void testShowSnackbarOnDismissNoAction() {
        bindToView();
        FeedStream.FeedActionsHandlerImpl handler =
                (FeedStream.FeedActionsHandlerImpl)
                        mContentManager.getContextValues(0).get(FeedActionsHandler.KEY);

        handler.showSnackbar(
                "message", "Undo", FeedActionsHandler.SnackbarDuration.SHORT, mSnackbarController);
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());

        // Dismissing the snackbar should trigger onDismissNoAction() on the stub snackbar
        // controller. postTaskAfterWorkComplete() should not execute the runnable until after the
        // stub snackbar runnable is executed.
        mSnackbarCaptor.getValue().getController().onDismissNoAction("data");
        mFeedStream.getInProgressWorkTrackerForTesting().postTaskAfterWorkComplete(mMockRunnable);
        verify(mMockRunnable, times(0)).run();

        mSnackbarController.mOnDismissNoActionFinished.run();
        verify(mMockRunnable, times(1)).run();
    }

    @Test
    @SmallTest
    public void testTriggerRefreshDismissesSnackbars() {
        bindToView();
        FeedStream.FeedActionsHandlerImpl handler =
                (FeedStream.FeedActionsHandlerImpl)
                        mContentManager.getContextValues(0).get(FeedActionsHandler.KEY);

        handler.showSnackbar(
                "message", "Undo", FeedActionsHandler.SnackbarDuration.SHORT, mSnackbarController);
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());

        mFeedStream.triggerRefresh(mMockRefreshCallback);

        verify(mSnackbarManager, times(1)).dismissSnackbars(any());
        verify(mFeedSurfaceRendererBridgeMock).manualRefresh(any());
    }

    @Test
    @SmallTest
    public void testShare() {
        mFeedStream.setShareWrapperForTest(mShareHelper);

        bindToView();
        FeedStream.FeedActionsHandlerImpl handler =
                (FeedStream.FeedActionsHandlerImpl)
                        mContentManager.getContextValues(0).get(FeedActionsHandler.KEY);

        String url = "http://www.foo.com";
        String title = "fooTitle";
        handler.share(url, title);
        verify(mShareHelper).share(url, title);
    }

    @Test
    @SmallTest
    public void testLoadMoreOnDismissal() {
        bindToView();
        final int itemCount = 10;

        FeedStream.FeedActionsHandlerImpl handler =
                (FeedStream.FeedActionsHandlerImpl)
                        mContentManager.getContextValues(0).get(FeedActionsHandler.KEY);

        // loadMore not triggered due to last visible item not falling into lookahead range.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD - 1);
        mLayoutManager.setItemCount(itemCount);
        handler.commitDismissal(0);
        verify(mFeedSurfaceRendererBridgeMock, never()).loadMore(any(Callback.class));

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        handler.commitDismissal(0);
        verify(mFeedSurfaceRendererBridgeMock).loadMore(any(Callback.class));
    }

    @Test
    @SmallTest
    public void testScrollIsReportedOnUnbind() {
        bindToView();

        // RecyclerView prevents scrolling if there's no content to scroll. We hack
        // the scroll listener directly.
        mFeedStream.getScrollListenerForTest().onScrolled(mRecyclerView, 0, 100);
        mFeedStream.unbind(false, false);

        verify(mFeedSurfaceRendererBridgeMock).reportStreamScrollStart();
        verify(mFeedSurfaceRendererBridgeMock).reportStreamScrolled(eq(100));
    }

    @Test
    @SmallTest
    public void testShowPlaceholder() {
        createHeaderContent(1);
        bindToView();
        FeedUiProto.StreamUpdate update =
                FeedUiProto.StreamUpdate.newBuilder()
                        .addUpdatedSlices(createSliceUpdateForLoadingSpinnerSlice("a", true))
                        .build();
        mBridgeRenderer.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals("LoadingSpinner", mContentManager.getContent(1).getKey());
        FeedListContentManager.FeedContent content = mContentManager.getContent(1);
        assertThat(content, instanceOf(FeedListContentManager.NativeViewContent.class));
        FeedListContentManager.NativeViewContent nativeViewContent =
                (FeedListContentManager.NativeViewContent) content;

        FrameLayout layout = new FrameLayout(mActivity);

        assertThat(
                nativeViewContent.getNativeView(layout),
                hasDescendant(instanceOf(FeedPlaceholderLayout.class)));
    }

    @Test
    @SmallTest
    public void testShowSpinner_PlaceholderDisabled() {
        setFeatureOverrides(/* feedLoadingPlaceholderOn= */ false);
        createHeaderContent(1);
        bindToView();
        FeedUiProto.StreamUpdate update =
                FeedUiProto.StreamUpdate.newBuilder()
                        .addUpdatedSlices(createSliceUpdateForLoadingSpinnerSlice("a", true))
                        .build();
        mBridgeRenderer.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals("LoadingSpinner", mContentManager.getContent(1).getKey());
        FeedListContentManager.FeedContent content = mContentManager.getContent(1);
        assertThat(content, instanceOf(FeedListContentManager.NativeViewContent.class));
        FeedListContentManager.NativeViewContent nativeViewContent =
                (FeedListContentManager.NativeViewContent) content;

        FrameLayout layout = new FrameLayout(mActivity);

        assertThat(
                nativeViewContent.getNativeView(layout),
                not(hasDescendant(instanceOf(FeedPlaceholderLayout.class))));
    }

    @Test
    @SmallTest
    public void testUnreadContentObserver_nullInterestFeed() {
        FeedStream stream =
                new FeedStream(
                        mActivity,
                        mProfileMock,
                        mSnackbarManager,
                        mBottomSheetController,
                        mWindowAndroid,
                        mShareDelegateSupplier,
                        /* isInterestFeed= */ StreamKind.FOR_YOU,
                        mActionDelegate,
                        /* FeedContentFirstLoadWatcher= */ null, /*Stream.StreamsMediator*/
                        null,
                        /* SingleWebFeedHelper= */ null,
                        new FeedSurfaceRendererBridgeFactory());
        assertNull(stream.getUnreadContentObserverForTest());
    }

    @Test
    @SmallTest
    public void testUnreadContentObserver_notNullWebFeed_sortOff() {
        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.WEB_FEED_SORT, false);
        FeatureList.setTestFeatures(features);
        FeedStream stream =
                new FeedStream(
                        mActivity,
                        mProfileMock,
                        mSnackbarManager,
                        mBottomSheetController,
                        mWindowAndroid,
                        mShareDelegateSupplier,
                        /* isInterestFeed= */ StreamKind.FOLLOWING,
                        mActionDelegate,
                        /* FeedContentFirstLoadWatcher= */ null, /*Stream.StreamsMediator*/
                        null,
                        /* SingleWebFeedHelper= */ null,
                        new FeedSurfaceRendererBridgeFactory());
        assertNotNull(stream.getUnreadContentObserverForTest());
        FeatureList.setTestFeatures(null);
    }

    @Test
    @SmallTest
    public void testUnreadContentObserver_notNullWebFeed_sortOn() {
        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.WEB_FEED_SORT, true);
        FeatureList.setTestFeatures(features);
        FeedStream stream =
                new FeedStream(
                        mActivity,
                        mProfileMock,
                        mSnackbarManager,
                        mBottomSheetController,
                        mWindowAndroid,
                        mShareDelegateSupplier,
                        StreamKind.FOLLOWING,
                        mActionDelegate,
                        /* FeedContentFirstLoadWatcher= */ null, /*Stream.StreamsMediator*/
                        null,
                        /* SingleWebFeedHelper= */ null,
                        new FeedSurfaceRendererBridgeFactory());
        assertNotNull(stream.getUnreadContentObserverForTest());
        FeatureList.setTestFeatures(null);
    }

    @Test
    @SmallTest
    public void testSupportsOptions_InterestFeed_sortOff() {
        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.WEB_FEED_SORT, false);
        FeatureList.setTestFeatures(features);
        FeedStream stream =
                new FeedStream(
                        mActivity,
                        mProfileMock,
                        mSnackbarManager,
                        mBottomSheetController,
                        mWindowAndroid,
                        mShareDelegateSupplier,
                        StreamKind.FOR_YOU,
                        mActionDelegate,
                        /* FeedContentFirstLoadWatcher= */ null, /*Stream.StreamsMediator*/
                        null,
                        /* SingleWebFeedHelper= */ null,
                        new FeedSurfaceRendererBridgeFactory());
        assertFalse(stream.supportsOptions());
    }

    @Test
    @SmallTest
    public void testSupportsOptions_InterestFeed_sortOn() {
        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.WEB_FEED_SORT, true);
        FeatureList.setTestFeatures(features);
        FeedStream stream =
                new FeedStream(
                        mActivity,
                        mProfileMock,
                        mSnackbarManager,
                        mBottomSheetController,
                        mWindowAndroid,
                        mShareDelegateSupplier,
                        StreamKind.FOR_YOU,
                        mActionDelegate,
                        /* FeedContentFirstLoadWatcher= */ null, /*Stream.StreamsMediator*/
                        null,
                        /* SingleWebFeedHelper= */ null,
                        new FeedSurfaceRendererBridgeFactory());
        assertFalse(stream.supportsOptions());
    }

    @Test
    @SmallTest
    public void testSupportsOptions_WebFeed_sortOff() {
        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.WEB_FEED_SORT, false);
        FeatureList.setTestFeatures(features);
        FeedStream stream =
                new FeedStream(
                        mActivity,
                        mProfileMock,
                        mSnackbarManager,
                        mBottomSheetController,
                        mWindowAndroid,
                        mShareDelegateSupplier,
                        StreamKind.FOLLOWING,
                        mActionDelegate,
                        /* FeedContentFirstLoadWatcher= */ null, /*Stream.StreamsMediator*/
                        null,
                        /* SingleWebFeedHelper= */ null,
                        new FeedSurfaceRendererBridgeFactory());
        assertFalse(stream.supportsOptions());
    }

    @Test
    @SmallTest
    public void testSupportsOptions_WebFeed_sortOn() {
        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.WEB_FEED_SORT, true);
        FeatureList.setTestFeatures(features);
        FeedStream stream =
                new FeedStream(
                        mActivity,
                        mProfileMock,
                        mSnackbarManager,
                        mBottomSheetController,
                        mWindowAndroid,
                        mShareDelegateSupplier,
                        StreamKind.FOLLOWING,
                        mActionDelegate,
                        /* FeedContentFirstLoadWatcher= */ null, /*Stream.StreamsMediator*/
                        null,
                        /* SingleWebFeedHelper= */ null,
                        new FeedSurfaceRendererBridgeFactory());
        assertTrue(stream.supportsOptions());
    }

    @Test
    @SmallTest
    public void testTriggerManualRefresh() {
        bindToView();
        FeedStream.FeedActionsHandlerImpl handler =
                (FeedStream.FeedActionsHandlerImpl)
                        mContentManager.getContextValues(0).get(FeedActionsHandler.KEY);

        handler.triggerManualRefresh();
        verify(mStreamsMediator).refreshStream();
    }

    private int getLoadMoreTriggerScrollDistance() {
        return (int)
                TypedValue.applyDimension(
                        TypedValue.COMPLEX_UNIT_DIP,
                        LOAD_MORE_TRIGGER_SCROLL_DISTANCE_DP,
                        mRecyclerView.getResources().getDisplayMetrics());
    }

    private FeedUiProto.StreamUpdate.SliceUpdate createSliceUpdateForExistingSlice(String sliceId) {
        return FeedUiProto.StreamUpdate.SliceUpdate.newBuilder().setSliceId(sliceId).build();
    }

    private FeedUiProto.StreamUpdate.SliceUpdate createSliceUpdateForNewXSurfaceSlice(
            String sliceId) {
        return FeedUiProto.StreamUpdate.SliceUpdate.newBuilder()
                .setSlice(createXSurfaceSSlice(sliceId))
                .build();
    }

    private FeedUiProto.Slice createXSurfaceSSlice(String sliceId) {
        return FeedUiProto.Slice.newBuilder()
                .setSliceId(sliceId)
                .setXsurfaceSlice(
                        FeedUiProto.XSurfaceSlice.newBuilder()
                                .setXsurfaceFrame(ByteString.copyFromUtf8(TEST_DATA))
                                .build())
                .build();
    }

    private FeedUiProto.StreamUpdate.SliceUpdate createSliceUpdateForLoadingSpinnerSlice(
            String sliceId, boolean isAtTop) {
        return FeedUiProto.StreamUpdate.SliceUpdate.newBuilder()
                .setSlice(createLoadingSpinnerSlice(sliceId, isAtTop))
                .build();
    }

    private FeedUiProto.Slice createLoadingSpinnerSlice(String sliceId, boolean isAtTop) {
        return FeedUiProto.Slice.newBuilder()
                .setSliceId(sliceId)
                .setLoadingSpinnerSlice(
                        FeedUiProto.LoadingSpinnerSlice.newBuilder().setIsAtTop(isAtTop).build())
                .build();
    }

    private void createHeaderContent(int number) {
        List<FeedListContentManager.FeedContent> contentList = new ArrayList<>();
        for (int i = 0; i < number; i++) {
            contentList.add(
                    new FeedListContentManager.NativeViewContent(
                            0, HEADER_PREFIX + i, new AppCompatTextView(mActivity)));
        }
        mContentManager.addContents(0, contentList);
    }

    void bindToView() {
        mFeedStream.bind(
                mRecyclerView,
                mContentManager,
                null,
                mSurfaceScope,
                mRenderer,
                mReliabilityLogger,
                mContentManager.getItemCount());
    }

    static class StubSnackbarController implements FeedActionsHandler.SnackbarController {
        Runnable mOnActionFinished;
        Runnable mOnDismissNoActionFinished;

        @Override
        public void onAction(Runnable actionFinished) {
            mOnActionFinished = actionFinished;
        }

        @Override
        public void onDismissNoAction(Runnable actionFinished) {
            mOnDismissNoActionFinished = actionFinished;
        }
    }
}
