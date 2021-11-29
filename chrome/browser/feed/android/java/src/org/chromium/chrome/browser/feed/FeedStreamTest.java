// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;

import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.Matchers.hasEntry;
import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.util.ArrayMap;
import android.util.TypedValue;
import android.widget.FrameLayout;

import androidx.appcompat.widget.AppCompatTextView;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import com.google.protobuf.ByteString;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.FeedActionsHandler;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler;
import org.chromium.chrome.browser.xsurface.SurfaceScope;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feed.proto.FeedUiProto;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link FeedStream}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowPostTask.class, ShadowRecordHistogram.class, ShadowGURL.class})
// TODO(crbug.com/1210371): Rewrite using paused loop. See crbug for details.
@LooperMode(LooperMode.Mode.LEGACY)
public class FeedStreamTest {
    private static final int LOAD_MORE_TRIGGER_LOOKAHEAD = 5;
    private static final int LOAD_MORE_TRIGGER_SCROLL_DISTANCE_DP = 100;
    private static final String TEST_DATA = "test";
    private static final String TEST_URL = JUnitTestGURLs.EXAMPLE_URL;
    private static final String HEADER_PREFIX = "header";

    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private FakeLinearLayoutManager mLayoutManager;
    private FeedStream mFeedStream;
    private NtpListContentManager mContentManager;

    @Mock
    private FeedStream.Natives mFeedStreamJniMock;
    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock
    private FeedReliabilityLoggingBridge.Natives mFeedReliabilityLoggingBridgeJniMock;

    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private NativePageNavigationDelegate mPageNavigationDelegate;
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private HelpAndFeedbackLauncherImpl mHelpAndFeedbackLauncherImpl;
    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock
    private FeedActionsHandler.SnackbarController mSnackbarController;
    @Mock
    private FeedStream.ShareHelperWrapper mShareHelper;
    @Mock
    private Profile mProfileMock;
    @Mock
    private HybridListRenderer mRenderer;
    @Mock
    private SurfaceScope mSurfaceScope;
    @Mock
    private RecyclerView.Adapter mAdapter;
    @Mock
    private FeedLaunchReliabilityLogger mLaunchReliabilityLogger;
    @Mock
    private BookmarkBridge mBookmarkBridge;
    @Mock
    private FeedActionDelegate mActionDelegate;

    @Captor
    private ArgumentCaptor<Map<String, String>> mMapCaptor;

    @Rule
    public JniMocker mocker = new JniMocker();
    // Enable the Features class, so we can call code which checks to see if features are enabled
    // without crashing.
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    private void setFeatureOverrides(boolean feedLoadingPlaceholderOn) {
        Map<String, Boolean> overrides = new ArrayMap<>();
        overrides.put(ChromeFeatureList.FEED_LOADING_PLACEHOLDER, feedLoadingPlaceholderOn);
        overrides.put(ChromeFeatureList.INTEREST_FEED_SPINNER_ALWAYS_ANIMATE, false);
        FeatureList.setTestFeatures(overrides);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).get();

        mocker.mock(FeedStreamJni.TEST_HOOKS, mFeedStreamJniMock);
        mocker.mock(FeedServiceBridge.getTestHooksForTesting(), mFeedServiceBridgeJniMock);
        mocker.mock(FeedReliabilityLoggingBridge.getTestHooksForTesting(),
                mFeedReliabilityLoggingBridgeJniMock);
        Profile.setLastUsedProfileForTesting(mProfileMock);

        when(mFeedServiceBridgeJniMock.getLoadMoreTriggerLookahead())
                .thenReturn(LOAD_MORE_TRIGGER_LOOKAHEAD);
        when(mFeedServiceBridgeJniMock.getLoadMoreTriggerScrollDistanceDp())
                .thenReturn(LOAD_MORE_TRIGGER_SCROLL_DISTANCE_DP);
        mFeedStream = new FeedStream(mActivity, mSnackbarManager, mBottomSheetController,
                /* isPlaceholderShown= */ false, mWindowAndroid, mShareDelegateSupplier,
                /* isInterestFeed= */ true,
                /* FeedAutoplaySettingsDelegate= */ null, mActionDelegate,
                /*helpAndFeedbackLauncher=*/null);
        mFeedStream.mMakeGURL = url -> JUnitTestGURLs.getGURL(url);
        mRecyclerView = new RecyclerView(mActivity);
        mRecyclerView.setAdapter(mAdapter);
        mContentManager = new NtpListContentManager();
        mLayoutManager = new FakeLinearLayoutManager(mActivity);
        mRecyclerView.setLayoutManager(mLayoutManager);

        doAnswer((invocation) -> {
            ((Runnable) invocation.getArgument(0)).run();
            return null;
        })
                .when(mBookmarkBridge)
                .finishLoadingBookmarkModel(any());
        doReturn(true).when(mBookmarkBridge).isBookmarkModelLoaded();

        setFeatureOverrides(true);

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
        mFeedStream.onStreamUpdated(update.toByteArray());

        mFeedStream.unbind(false);
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
        mFeedStream.onStreamUpdated(update.toByteArray());

        // Add header content.
        createHeaderContent(2);
        mFeedStream.notifyNewHeaderCount(5);

        mFeedStream.unbind(false);

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
        mFeedStream.onStreamUpdated(update.toByteArray());

        mFeedStream.unbind(true);

        assertEquals(2, mContentManager.getItemCount());
        assertEquals(HEADER_PREFIX + "0", mContentManager.getContent(0).getKey());
        assertEquals(1, mContentManager.findContentPositionByKey("Spacer"));
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
        mFeedStream.onStreamUpdated(update.toByteArray());
        mFeedStream.unbind(true);

        // Bind again with correct headercount.
        mFeedStream.bind(mRecyclerView, mContentManager, null, mSurfaceScope, mRenderer,
                mLaunchReliabilityLogger, 2);

        // Add different feed content.
        update = FeedUiProto.StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                         .build();
        mFeedStream.onStreamUpdated(update.toByteArray());

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
        verify(mFeedStreamJniMock).surfaceOpened(anyLong(), any(FeedStream.class));
        // Set handlers in contentmanager.
        assertEquals(2, mContentManager.getContextValues(0).size());
        verify(mLaunchReliabilityLogger, times(1)).logFeedReloading(anyLong());
    }

    @Test
    public void testUnbind() {
        bindToView();
        mFeedStream.unbind(false);
        verify(mFeedStreamJniMock).surfaceClosed(anyLong(), any(FeedStream.class));
        // Unset handlers in contentmanager.
        assertEquals(0, mContentManager.getContextValues(0).size());
    }

    @Test
    public void testUnbindDismissesSnackbars() {
        bindToView();

        FeedStream.FeedActionsHandlerImpl handler =
                (FeedStream.FeedActionsHandlerImpl) mContentManager.getContextValues(0).get(
                        FeedActionsHandler.KEY);

        handler.showSnackbar(
                "message", "Undo", FeedActionsHandler.SnackbarDuration.SHORT, mSnackbarController);
        verify(mSnackbarManager).showSnackbar(any());
        mFeedStream.unbind(false);
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
        mFeedStream.onStreamUpdated(update.toByteArray());
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));

        // Add 2 more slices.
        update = FeedUiProto.StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("b"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("d"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("e"))
                         .build();
        mFeedStream.onStreamUpdated(update.toByteArray());
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
        mFeedStream.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));

        // Add 2 new slice with same ids as before.
        update = FeedUiProto.StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                         .build();
        mFeedStream.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("b"));
        assertEquals(1, mContentManager.findContentPositionByKey("a"));
    }

    @Test
    public void testLoadMore_unbound() {
        // By default, stream content is not visible.
        final int triggerDistance = getLoadMoreTriggerScrollDistance();
        mFeedStream.checkScrollingForLoadMore(triggerDistance);
        verify(mFeedStreamJniMock, never())
                .loadMore(anyLong(), any(FeedStream.class), any(Callback.class));
    }

    @Test
    public void testLoadMore_bound() {
        bindToView();
        final int triggerDistance = getLoadMoreTriggerScrollDistance();
        final int itemCount = 10;

        // loadMore not triggered due to not enough accumulated scrolling distance.
        mFeedStream.checkScrollingForLoadMore(triggerDistance / 2);
        verify(mFeedStreamJniMock, never())
                .loadMore(anyLong(), any(FeedStream.class), any(Callback.class));

        // loadMore not triggered due to last visible item not falling into lookahead range.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD - 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStream.checkScrollingForLoadMore(triggerDistance / 2);
        verify(mFeedStreamJniMock, never())
                .loadMore(anyLong(), any(FeedStream.class), any(Callback.class));

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStream.checkScrollingForLoadMore(triggerDistance / 2);
        verify(mFeedStreamJniMock).loadMore(anyLong(), any(FeedStream.class), any(Callback.class));
    }

    @Test
    public void testContentAfterUnbind() {
        bindToView();
        final int triggerDistance = getLoadMoreTriggerScrollDistance();
        final int itemCount = 10;

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStream.checkScrollingForLoadMore(triggerDistance);
        verify(mFeedStreamJniMock).loadMore(anyLong(), any(FeedStream.class), any(Callback.class));

        // loadMore triggered again after hide&show.
        mFeedStream.checkScrollingForLoadMore(-triggerDistance);
        mFeedStream.unbind(false);
        bindToView();

        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStream.checkScrollingForLoadMore(triggerDistance);
        verify(mFeedStreamJniMock).loadMore(anyLong(), any(FeedStream.class), any(Callback.class));
    }

    @Test
    @SmallTest
    public void testNavigateTab() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler) mContentManager.getContextValues(0).get(
                        SurfaceActionsHandler.KEY);
        handler.navigateTab(TEST_URL, null);
        verify(mActionDelegate)
                .openSuggestionUrl(eq(org.chromium.ui.mojom.WindowOpenDisposition.CURRENT_TAB),
                        any(), any(), any());
    }

    @Test
    @SmallTest
    public void testNavigateNewTab() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler) mContentManager.getContextValues(0).get(
                        SurfaceActionsHandler.KEY);

        handler.navigateNewTab(TEST_URL, null);
        verify(mActionDelegate)
                .openSuggestionUrl(
                        eq(org.chromium.ui.mojom.WindowOpenDisposition.NEW_BACKGROUND_TAB), any(),
                        any(), any());
    }

    @Test
    @SmallTest
    public void testNavigateIncognitoTab() {
        MetricsUtils.HistogramDelta actionOpenedSnippetDelta = new MetricsUtils.HistogramDelta(
                "NewTabPage.ActionAndroid2", NewTabPageUma.ACTION_OPENED_SNIPPET);
        when(mPageNavigationDelegate.openUrl(anyInt(), any())).thenReturn(new MockTab(1, false));
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler) mContentManager.getContextValues(0).get(
                        SurfaceActionsHandler.KEY);
        handler.navigateIncognitoTab(TEST_URL);
        verify(mActionDelegate)
                .openSuggestionUrl(eq(org.chromium.ui.mojom.WindowOpenDisposition.OFF_THE_RECORD),
                        any(), any(), any());
    }

    @Test
    @SmallTest
    public void testShowBottomSheet() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler) mContentManager.getContextValues(0).get(
                        SurfaceActionsHandler.KEY);

        handler.showBottomSheet(new AppCompatTextView(mActivity), null);
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testDismissBottomSheet() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler) mContentManager.getContextValues(0).get(
                        SurfaceActionsHandler.KEY);

        handler.showBottomSheet(new AppCompatTextView(mActivity), null);
        mFeedStream.dismissBottomSheet();
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testAddToReadingList() {
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler) mContentManager.getContextValues(0).get(
                        SurfaceActionsHandler.KEY);
        handler.addToReadingList("title", TEST_URL);

        verify(mFeedStreamJniMock)
                .reportOtherUserAction(anyLong(), any(FeedStream.class),
                        eq(FeedUserActionType.TAPPED_ADD_TO_READING_LIST));
        verify(mActionDelegate).addToReadingList(eq("title"), eq(TEST_URL));
    }

    @Test
    @SmallTest
    public void testSendFeedback() {
        final String testUrl = TEST_URL;
        final String testTitle = "Chromium based browsers for the win!";
        final String xSurfaceCardTitle = "Card Title";
        final String cardTitle = "CardTitle";
        final String cardUrl = "CardUrl";
        // Arrange.
        Map<String, String> productSpecificDataMap = new HashMap<>();
        productSpecificDataMap.put(FeedStream.FeedActionsHandlerImpl.XSURFACE_CARD_URL, testUrl);
        productSpecificDataMap.put(xSurfaceCardTitle, testTitle);

        mFeedStream.setHelpAndFeedbackLauncherForTest(mHelpAndFeedbackLauncherImpl);
        bindToView();
        FeedStream.FeedActionsHandlerImpl handler =
                (FeedStream.FeedActionsHandlerImpl) mContentManager.getContextValues(0).get(
                        FeedActionsHandler.KEY);

        // Act.
        handler.sendFeedback(productSpecificDataMap);

        // Assert.
        verify(mHelpAndFeedbackLauncherImpl)
                .showFeedback(any(), any(), eq(testUrl),
                        eq(FeedStream.FeedActionsHandlerImpl.FEEDBACK_REPORT_TYPE),
                        mMapCaptor.capture());

        // Check that the map contents are as expected.
        assertThat(mMapCaptor.getValue(), hasEntry(cardUrl, testUrl));
        assertThat(mMapCaptor.getValue(), hasEntry(cardTitle, testTitle));
    }

    @Test
    @SmallTest
    public void testShowSnackbar() {
        bindToView();
        FeedStream.FeedActionsHandlerImpl handler =
                (FeedStream.FeedActionsHandlerImpl) mContentManager.getContextValues(0).get(
                        FeedActionsHandler.KEY);

        handler.showSnackbar(
                "message", "Undo", FeedActionsHandler.SnackbarDuration.SHORT, mSnackbarController);
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    @SmallTest
    public void testShare() {
        mFeedStream.setShareWrapperForTest(mShareHelper);

        bindToView();
        FeedStream.FeedActionsHandlerImpl handler =
                (FeedStream.FeedActionsHandlerImpl) mContentManager.getContextValues(0).get(
                        FeedActionsHandler.KEY);

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
                (FeedStream.FeedActionsHandlerImpl) mContentManager.getContextValues(0).get(
                        FeedActionsHandler.KEY);

        // loadMore not triggered due to last visible item not falling into lookahead range.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD - 1);
        mLayoutManager.setItemCount(itemCount);
        handler.commitDismissal(0);
        verify(mFeedStreamJniMock, never())
                .loadMore(anyLong(), any(FeedStream.class), any(Callback.class));

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        handler.commitDismissal(0);
        verify(mFeedStreamJniMock).loadMore(anyLong(), any(FeedStream.class), any(Callback.class));
    }

    @Test
    @SmallTest
    public void testScrollIsReportedOnUnbind() {
        bindToView();

        // RecyclerView prevents scrolling if there's no content to scroll. We hack
        // the scroll listener directly.
        mFeedStream.getScrollListenerForTest().onScrolled(mRecyclerView, 0, 100);
        mFeedStream.unbind(false);

        verify(mFeedStreamJniMock).reportStreamScrollStart(anyLong(), any(FeedStream.class));
        verify(mFeedStreamJniMock).reportStreamScrolled(anyLong(), any(FeedStream.class), eq(100));
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
        mFeedStream.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals("a", mContentManager.getContent(1).getKey());
        NtpListContentManager.FeedContent content = mContentManager.getContent(1);
        assertThat(mContentManager.getContent(1),
                instanceOf(NtpListContentManager.NativeViewContent.class));
        NtpListContentManager.NativeViewContent nativeViewContent =
                (NtpListContentManager.NativeViewContent) mContentManager.getContent(1);

        FrameLayout layout = new FrameLayout(mActivity);

        assertThat(nativeViewContent.getNativeView(layout),
                hasDescendant(instanceOf(FeedPlaceholderLayout.class)));
    }

    @Test
    @SmallTest
    public void testShowSpinner_PlaceholderDisabled() {
        setFeatureOverrides(false);
        createHeaderContent(1);
        bindToView();
        FeedUiProto.StreamUpdate update =
                FeedUiProto.StreamUpdate.newBuilder()
                        .addUpdatedSlices(createSliceUpdateForLoadingSpinnerSlice("a", true))
                        .build();
        mFeedStream.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals("a", mContentManager.getContent(1).getKey());
        NtpListContentManager.FeedContent content = mContentManager.getContent(1);
        assertThat(mContentManager.getContent(1),
                instanceOf(NtpListContentManager.NativeViewContent.class));
        NtpListContentManager.NativeViewContent nativeViewContent =
                (NtpListContentManager.NativeViewContent) mContentManager.getContent(1);

        FrameLayout layout = new FrameLayout(mActivity);

        assertThat(nativeViewContent.getNativeView(layout),
                not(hasDescendant(instanceOf(FeedPlaceholderLayout.class))));
    }

    @Test
    @SmallTest
    public void testUnreadContentObserver_nullInterestFeed() {
        FeedStream stream = new FeedStream(mActivity, mSnackbarManager, mBottomSheetController,
                /* isPlaceholderShown= */ false, mWindowAndroid, mShareDelegateSupplier,
                /* isInterestFeed= */ true,
                /* FeedAutoplaySettingsDelegate= */ null, mActionDelegate,
                /*helpAndFeedbackLauncher=*/null);
        assertNull(stream.getUnreadContentObserverForTest());
    }

    @Test
    @SmallTest
    public void testUnreadContentObserver_notNullWebFeed_sortOff() {
        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.WEB_FEED_SORT, false);
        FeatureList.setTestFeatures(features);
        FeedStream stream = new FeedStream(mActivity, mSnackbarManager, mBottomSheetController,
                /* isPlaceholderShown= */ false, mWindowAndroid, mShareDelegateSupplier,
                /* isInterestFeed= */ false,
                /* FeedAutoplaySettingsDelegate= */ null, mActionDelegate,
                /*helpAndFeedbackLauncher=*/null);
        assertNotNull(stream.getUnreadContentObserverForTest());
        FeatureList.setTestFeatures(null);
    }

    @Test
    @SmallTest
    public void testUnreadContentObserver_notNullWebFeed_sortOn() {
        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.WEB_FEED_SORT, true);
        FeatureList.setTestFeatures(features);
        FeedStream stream = new FeedStream(mActivity, mSnackbarManager, mBottomSheetController,
                /* isPlaceholderShown= */ false, mWindowAndroid, mShareDelegateSupplier,
                /* isInterestFeed= */ false,
                /* FeedAutoplaySettingsDelegate= */ null, mActionDelegate,
                /*helpAndFeedbackLauncher=*/null);
        assertNotNull(stream.getUnreadContentObserverForTest());
        FeatureList.setTestFeatures(null);
    }

    private int getLoadMoreTriggerScrollDistance() {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
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
                .setXsurfaceSlice(FeedUiProto.XSurfaceSlice.newBuilder()
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
        List<NtpListContentManager.FeedContent> contentList = new ArrayList<>();
        for (int i = 0; i < number; i++) {
            contentList.add(new NtpListContentManager.NativeViewContent(
                    0, HEADER_PREFIX + i, new AppCompatTextView(mActivity)));
        }
        mContentManager.addContents(0, contentList);
    }

    void bindToView() {
        mFeedStream.bind(mRecyclerView, mContentManager, null, mSurfaceScope, mRenderer,
                mLaunchReliabilityLogger, mContentManager.getItemCount());
    }
}
