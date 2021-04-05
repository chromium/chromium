// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import com.google.protobuf.ByteString;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Callback;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.shared.stream.Stream.ContentChangedListener;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.FeedActionsHandler;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feed.proto.FeedUiProto.Slice;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate;
import org.chromium.components.feed.proto.FeedUiProto.StreamUpdate.SliceUpdate;
import org.chromium.components.feed.proto.FeedUiProto.XSurfaceSlice;
import org.chromium.ui.mojom.WindowOpenDisposition;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link FeedStreamSeSurface}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowPostTask.class, ShadowRecordHistogram.class})
public class FeedStreamSurfaceTest {
    class ContentChangeWatcher implements ContentChangedListener {
        @Override
        public void onContentChanged() {
            mContentChanged = true;
        }

        @Override
        public void onAddFinished() {}
    }

    private static final String TEST_DATA = "test";
    private static final String TEST_URL = "https://www.chromium.org";
    private static final int LOAD_MORE_TRIGGER_LOOKAHEAD = 5;
    private FeedStreamSurface mFeedStreamSurface;
    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private LinearLayout mParent;
    private FakeLinearLayoutManager mLayoutManager;
    private FeedListContentManager mContentManager;
    private boolean mContentChanged;

    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private FeedActionsHandler.SnackbarController mSnackbarController;
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private NativePageNavigationDelegate mPageNavigationDelegate;
    @Mock
    private HelpAndFeedbackLauncherImpl mHelpAndFeedbackLauncherImpl;
    @Mock
    Profile mProfileMock;
    @Mock
    private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock
    private FeedStreamSurface.ShareHelperWrapper mShareHelper;

    @Captor
    private ArgumentCaptor<Map<String, String>> mMapCaptor;

    @Rule
    public JniMocker mocker = new JniMocker();
    // Enable the Features class, so we can call code which checks to see if features are enabled
    // without crashing.
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock
    private FeedStreamSurface.Natives mFeedStreamSurfaceJniMock;

    @Mock
    private FeedServiceBridgeDelegateImpl mFeedServiceBridgeDelegate;
    @Mock
    private ProcessScope mProcessScope;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        FeedStreamSurface.sRequestContentWithoutRendererForTesting = true;
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mParent = new LinearLayout(mActivity);
        mocker.mock(FeedStreamSurfaceJni.TEST_HOOKS, mFeedStreamSurfaceJniMock);
        mocker.mock(FeedServiceBridge.getTestHooksForTesting(), mFeedServiceBridgeJniMock);

        when(mFeedServiceBridgeJniMock.getLoadMoreTriggerLookahead())
                .thenReturn(LOAD_MORE_TRIGGER_LOOKAHEAD);
        when(mFeedServiceBridgeDelegate.getProcessScope()).thenReturn(mProcessScope);
        FeedServiceBridge.setDelegate(mFeedServiceBridgeDelegate);

        Profile.setLastUsedProfileForTesting(mProfileMock);
        mFeedStreamSurface = Mockito.spy(new FeedStreamSurface(mActivity, false, mSnackbarManager,
                mPageNavigationDelegate, mBottomSheetController, mHelpAndFeedbackLauncherImpl,
                /* isPlaceholderShown= */ false, mShareHelper, null));
        mContentManager = mFeedStreamSurface.getFeedListContentManagerForTesting();
        mFeedStreamSurface.mRootView = Mockito.spy(mFeedStreamSurface.mRootView);
        mRecyclerView = mFeedStreamSurface.mRootView;
        mLayoutManager = new FakeLinearLayoutManager(mActivity);
        mRecyclerView.setLayoutManager(mLayoutManager);
        mFeedStreamSurface.addContentChangedListener(new ContentChangeWatcher());

        // Since we use a mockito spy, we need to replace the entry in sSurfaces.
        FeedStreamSurface.sSurfaces.clear();
        FeedStreamSurface.sSurfaces.add(mFeedStreamSurface);

        // Print logs to stdout.
        ShadowLog.stream = System.out;
    }

    @After
    public void tearDown() {
        mFeedStreamSurface.destroy();
        FeedStreamSurface.shutdownForTesting();
    }

    @Test
    @SmallTest
    public void testContentChangedOnStreamUpdated() {
        startupAndSetVisible();

        // Add 1 slice.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .build();
        mContentChanged = false;
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertTrue(mContentChanged);
        assertEquals(1, mContentManager.getItemCount());

        // Remove 1 slice.
        update = StreamUpdate.newBuilder().build();
        mContentChanged = false;
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertTrue(mContentChanged);
        assertEquals(0, mContentManager.getItemCount());
    }

    @Test
    @SmallTest
    public void testContentChangedOnSetHeaderViews() {
        startupAndSetVisible();

        mContentChanged = false;
        mFeedStreamSurface.setHeaderViews(Arrays.asList(new View(mActivity)));
        assertTrue(mContentChanged);
        assertEquals(1, mContentManager.getItemCount());
    }

    @Test
    @SmallTest
    public void testAddSlicesOnStreamUpdated() {
        startupAndSetVisible();
        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));

        // Add 2 more slices.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("b"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("d"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("e"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
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
        startupAndSetVisible();
        // Add 2 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));

        // Add 2 new slice with same ids as before.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("b"));
        assertEquals(1, mContentManager.findContentPositionByKey("a"));
    }

    @Test
    @SmallTest
    public void testRemoveSlicesOnStreamUpdated() {
        startupAndSetVisible();
        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));

        // Remove 1 slice.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("c"));

        // Remove 2 slices.
        update = StreamUpdate.newBuilder().build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(0, mContentManager.getItemCount());
    }

    @Test
    @SmallTest
    public void testReorderSlicesOnStreamUpdated() {
        startupAndSetVisible();
        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));

        // Reorder 1 slice.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("b"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("c"));
        assertEquals(1, mContentManager.findContentPositionByKey("a"));
        assertEquals(2, mContentManager.findContentPositionByKey("b"));

        // Reorder 2 slices.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("b"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));
    }

    @Test
    @SmallTest
    public void testComplexOperationsOnStreamUpdated() {
        startupAndSetVisible();
        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("d"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("e"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(5, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("a"));
        assertEquals(1, mContentManager.findContentPositionByKey("b"));
        assertEquals(2, mContentManager.findContentPositionByKey("c"));
        assertEquals(3, mContentManager.findContentPositionByKey("d"));
        assertEquals(4, mContentManager.findContentPositionByKey("e"));

        // Combo of add, remove and reorder operations.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("f"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("g"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("h"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("e"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("i"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(7, mContentManager.getItemCount());
        assertEquals(0, mContentManager.findContentPositionByKey("f"));
        assertEquals(1, mContentManager.findContentPositionByKey("g"));
        assertEquals(2, mContentManager.findContentPositionByKey("a"));
        assertEquals(3, mContentManager.findContentPositionByKey("h"));
        assertEquals(4, mContentManager.findContentPositionByKey("c"));
        assertEquals(5, mContentManager.findContentPositionByKey("e"));
        assertEquals(6, mContentManager.findContentPositionByKey("i"));
    }

    @Test
    @SmallTest
    public void testAddHeaderViews() {
        startupAndSetVisible();
        View v0 = new View(mActivity);
        View v1 = new View(mActivity);

        mFeedStreamSurface.setHeaderViews(Arrays.asList(v0, v1));
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(v0, getNativeView(0));
        assertEquals(v1, getNativeView(1));
    }

    @Test
    @SmallTest
    public void testUpdateHeaderViews() {
        startupAndSetVisible();
        View v0 = new View(mActivity);
        View v1 = new View(mActivity);

        mFeedStreamSurface.setHeaderViews(Arrays.asList(v0, v1));
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(v0, getNativeView(0));
        assertEquals(v1, getNativeView(1));

        View v2 = new View(mActivity);
        View v3 = new View(mActivity);

        mFeedStreamSurface.setHeaderViews(Arrays.asList(v2, v0, v3));
        assertEquals(3, mContentManager.getItemCount());
        assertEquals(v2, getNativeView(0));
        assertEquals(v0, getNativeView(1));
        assertEquals(v3, getNativeView(2));
    }

    @Test
    @SmallTest
    public void testComplexOperationsOnStreamUpdatedAfterSetHeaderViews() {
        startupAndSetVisible();
        // Set 2 header views first. These should always be there throughout stream update.
        View v0 = new View(mActivity);
        View v1 = new View(mActivity);
        mFeedStreamSurface.setHeaderViews(Arrays.asList(v0, v1));
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(v0, getNativeView(0));
        assertEquals(v1, getNativeView(1));
        final int headers = 2;

        // Add 3 new slices at first.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("d"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("e"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(headers + 5, mContentManager.getItemCount());
        assertEquals(headers + 0, mContentManager.findContentPositionByKey("a"));
        assertEquals(headers + 1, mContentManager.findContentPositionByKey("b"));
        assertEquals(headers + 2, mContentManager.findContentPositionByKey("c"));
        assertEquals(headers + 3, mContentManager.findContentPositionByKey("d"));
        assertEquals(headers + 4, mContentManager.findContentPositionByKey("e"));

        // Combo of add, remove and reorder operations.
        update = StreamUpdate.newBuilder()
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("f"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("g"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("a"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("h"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("c"))
                         .addUpdatedSlices(createSliceUpdateForExistingSlice("e"))
                         .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("i"))
                         .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(headers + 7, mContentManager.getItemCount());
        assertEquals(headers + 0, mContentManager.findContentPositionByKey("f"));
        assertEquals(headers + 1, mContentManager.findContentPositionByKey("g"));
        assertEquals(headers + 2, mContentManager.findContentPositionByKey("a"));
        assertEquals(headers + 3, mContentManager.findContentPositionByKey("h"));
        assertEquals(headers + 4, mContentManager.findContentPositionByKey("c"));
        assertEquals(headers + 5, mContentManager.findContentPositionByKey("e"));
        assertEquals(headers + 6, mContentManager.findContentPositionByKey("i"));
    }

    @Test
    @SmallTest
    public void testNavigateTab() {
        HistogramDelta actionOpenedSnippetDelta = new HistogramDelta(
                "NewTabPage.ActionAndroid2", NewTabPageUma.ACTION_OPENED_SNIPPET);
        when(mPageNavigationDelegate.openUrl(anyInt(), any())).thenReturn(new MockTab(1, false));
        mFeedStreamSurface.navigateTab(TEST_URL, null);
        verify(mPageNavigationDelegate)
                .openUrl(ArgumentMatchers.eq(WindowOpenDisposition.CURRENT_TAB), any());

        assertEquals(1, actionOpenedSnippetDelta.getDelta());
    }

    @Test
    @SmallTest
    public void testNavigateNewTab() {
        HistogramDelta actionOpenedSnippetDelta = new HistogramDelta(
                "NewTabPage.ActionAndroid2", NewTabPageUma.ACTION_OPENED_SNIPPET);
        when(mPageNavigationDelegate.openUrl(anyInt(), any())).thenReturn(new MockTab(1, false));
        mFeedStreamSurface.navigateNewTab(TEST_URL);
        verify(mPageNavigationDelegate)
                .openUrl(ArgumentMatchers.eq(WindowOpenDisposition.NEW_BACKGROUND_TAB), any());
        assertEquals(1, actionOpenedSnippetDelta.getDelta());
    }

    @Test
    @SmallTest
    public void testNavigateIncognitoTab() {
        HistogramDelta actionOpenedSnippetDelta = new HistogramDelta(
                "NewTabPage.ActionAndroid2", NewTabPageUma.ACTION_OPENED_SNIPPET);
        when(mPageNavigationDelegate.openUrl(anyInt(), any())).thenReturn(new MockTab(1, false));
        mFeedStreamSurface.navigateIncognitoTab(TEST_URL);
        verify(mPageNavigationDelegate)
                .openUrl(ArgumentMatchers.eq(WindowOpenDisposition.OFF_THE_RECORD), any());
        assertEquals(1, actionOpenedSnippetDelta.getDelta());
    }

    @Test
    @SmallTest
    public void testSendFeedback() {
        final String testUrl = "https://www.chromium.org";
        final String testTitle = "Chromium based browsers for the win!";
        final String xSurfaceCardTitle = "Card Title";
        final String cardTitle = "CardTitle";
        final String cardUrl = "CardUrl";
        // Arrange.
        Map<String, String> productSpecificDataMap = new HashMap<>();
        productSpecificDataMap.put(FeedStreamSurface.XSURFACE_CARD_URL, testUrl);
        productSpecificDataMap.put(xSurfaceCardTitle, testTitle);

        // Act.
        mFeedStreamSurface.sendFeedback(productSpecificDataMap);

        // Assert.
        verify(mHelpAndFeedbackLauncherImpl)
                .showFeedback(any(), any(), eq(testUrl), eq(FeedStreamSurface.FEEDBACK_REPORT_TYPE),
                        mMapCaptor.capture());

        // Check that the map contents are as expected.
        assertThat(mMapCaptor.getValue()).containsEntry(cardUrl, testUrl);
        assertThat(mMapCaptor.getValue()).containsEntry(cardTitle, testTitle);
    }

    @Test
    @SmallTest
    public void testShowSnackbar() {
        mFeedStreamSurface.showSnackbar(
                "message", "Undo", FeedActionsHandler.SnackbarDuration.SHORT, mSnackbarController);
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    @SmallTest
    public void testShowBottomSheet() {
        mFeedStreamSurface.showBottomSheet(new TextView(mActivity));
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testDismissBottomSheet() {
        mFeedStreamSurface.showBottomSheet(new TextView(mActivity));
        mFeedStreamSurface.dismissBottomSheet();
        verify(mBottomSheetController).hideContent(any(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testShare() {
        String url = "http://www.foo.com";
        String title = "fooTitle";
        mFeedStreamSurface.share(url, title);
        verify(mShareHelper).share(url, title);
    }

    @Test
    @SmallTest
    public void testRemoveContentsOnSurfaceClosed() {
        startupAndSetVisible();

        // Set 2 header views first.
        View v0 = new View(mActivity);
        View v1 = new View(mActivity);
        mFeedStreamSurface.setHeaderViews(Arrays.asList(v0, v1));
        assertEquals(2, mContentManager.getItemCount());
        assertEquals(v0, getNativeView(0));
        assertEquals(v1, getNativeView(1));
        final int headers = 2;

        // Add 3 new slices.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("c"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());
        assertEquals(headers + 3, mContentManager.getItemCount());

        // Closing the surface should remove all non-header contents.
        mContentChanged = false;
        mFeedStreamSurface.setStreamVisibility(false);
        assertTrue(mContentChanged);
        assertEquals(headers, mContentManager.getItemCount());
        assertEquals(v0, getNativeView(0));
        assertEquals(v1, getNativeView(1));
    }

    @Test
    @SmallTest
    public void testLoadMoreOnDismissal() {
        startupAndSetVisible();
        final int itemCount = 10;

        // loadMore not triggered due to last visible item not falling into lookahead range.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD - 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStreamSurface.commitDismissal(0);
        verify(mFeedStreamSurfaceJniMock, never())
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStreamSurface.commitDismissal(0);
        verify(mFeedStreamSurfaceJniMock)
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));
    }

    @Test
    @SmallTest
    public void testLoadMoreOnNavigateNewTab() {
        startupAndSetVisible();
        final int itemCount = 10;

        // loadMore not triggered due to last visible item not falling into lookahead range.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD - 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStreamSurface.navigateNewTab("");
        verify(mFeedStreamSurfaceJniMock, never())
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStreamSurface.navigateNewTab("");
        verify(mFeedStreamSurfaceJniMock)
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));
    }

    @Test
    @SmallTest
    public void testLoadMoreOnNavigateIncognitoTab() {
        startupAndSetVisible();
        final int itemCount = 10;

        // loadMore not triggered due to last visible item not falling into lookahead range.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD - 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStreamSurface.navigateIncognitoTab("");
        verify(mFeedStreamSurfaceJniMock, never())
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));

        // loadMore triggered.
        mLayoutManager.setLastVisiblePosition(itemCount - LOAD_MORE_TRIGGER_LOOKAHEAD + 1);
        mLayoutManager.setItemCount(itemCount);
        mFeedStreamSurface.navigateIncognitoTab("");
        verify(mFeedStreamSurfaceJniMock)
                .loadMore(anyLong(), any(FeedStreamSurface.class), any(Callback.class));
    }

    @Test
    @SmallTest
    public void testSurfaceOpenedAndClosed() {
        // The surface won't open until after startup().
        mFeedStreamSurface.setStreamVisibility(true);
        mFeedStreamSurface.setStreamContentVisibility(true);
        Assert.assertFalse(mFeedStreamSurface.isOpened());

        // Exercise turning off stream visibility while hidden.
        mFeedStreamSurface.setStreamVisibility(false);
        Assert.assertFalse(mFeedStreamSurface.isOpened());

        // Trigger open.
        mFeedStreamSurface.setStreamVisibility(true);
        FeedStreamSurface.startup();
        Assert.assertTrue(mFeedStreamSurface.isOpened());

        mFeedStreamSurface.setStreamVisibility(false);
        Assert.assertFalse(mFeedStreamSurface.isOpened());

        mFeedStreamSurface.setStreamVisibility(true);
        Assert.assertTrue(mFeedStreamSurface.isOpened());

        mFeedStreamSurface.setStreamContentVisibility(false);
        Assert.assertFalse(mFeedStreamSurface.isOpened());

        mFeedStreamSurface.setStreamContentVisibility(true);
        Assert.assertTrue(mFeedStreamSurface.isOpened());
    }

    @Test
    @SmallTest
    public void testClearAll() {
        InOrder order = Mockito.inOrder(mFeedStreamSurfaceJniMock, mProcessScope);
        startupAndSetVisible();
        order.verify(mFeedStreamSurfaceJniMock)
                .surfaceOpened(anyLong(), any(FeedStreamSurface.class));

        FeedStreamSurface.clearAll();
        order.verify(mFeedStreamSurfaceJniMock)
                .surfaceClosed(anyLong(), any(FeedStreamSurface.class));
        order.verify(mProcessScope).resetAccount();
        order.verify(mFeedStreamSurfaceJniMock)
                .surfaceOpened(anyLong(), any(FeedStreamSurface.class));
    }

    @Test
    @SmallTest
    public void testFindChildViewContainingDescendentNullParameters() {
        startupAndSetVisible();
        View v = new View(mActivity);
        assertEquals(null, mFeedStreamSurface.findChildViewContainingDescendent(null, v));
        assertEquals(null, mFeedStreamSurface.findChildViewContainingDescendent(v, null));
    }

    @Test
    @SmallTest
    public void testFindChildViewContainingDescendentNotADescendent() {
        startupAndSetVisible();
        View v1 = new View(mActivity);
        LinearLayout v2 = new LinearLayout(mActivity);
        View v2Child = new View(mActivity);
        v2.addView(v2Child);

        assertEquals(null, mFeedStreamSurface.findChildViewContainingDescendent(v1, v2));
        assertEquals(null, mFeedStreamSurface.findChildViewContainingDescendent(v1, v2Child));
    }

    @Test
    @SmallTest
    public void testFindChildViewContainingDescendentDirectDescendent() {
        startupAndSetVisible();
        LinearLayout parent = new LinearLayout(mActivity);
        View child = new View(mActivity);
        parent.addView(child);

        assertEquals(child, mFeedStreamSurface.findChildViewContainingDescendent(parent, child));
    }

    @Test
    @SmallTest
    public void testFindChildViewContainingDescendentIndirectDescendent() {
        startupAndSetVisible();
        LinearLayout parent = new LinearLayout(mActivity);
        LinearLayout child = new LinearLayout(mActivity);
        View grandChild = new View(mActivity);
        parent.addView(child);
        child.addView(grandChild);

        assertEquals(
                child, mFeedStreamSurface.findChildViewContainingDescendent(parent, grandChild));
    }

    @Test
    @SmallTest
    public void testOnStreamUpdatedIgnoredWhenNotOpen() {
        // Surface not opened initially.
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());

        assertEquals(0, mContentManager.getItemCount());
    }

    @Test
    @SmallTest
    public void testNavigateReportsCorrectSlice() {
        startupAndSetVisible();
        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());

        View childA = new View(mActivity);
        mRecyclerView.addView(childA);
        View childB = new View(mActivity);
        mRecyclerView.addView(childB);

        // findChildViewContainingDescendent() won't work on its own because mRecyclerView is a
        // mockito spy, and therefore child.getParent() != mRecyclerView.
        Mockito.doReturn(childA)
                .when(mFeedStreamSurface)
                .findChildViewContainingDescendent(mRecyclerView, childA);
        Mockito.doReturn(childB)
                .when(mFeedStreamSurface)
                .findChildViewContainingDescendent(mRecyclerView, childB);
        Mockito.doReturn(0).when(mRecyclerView).getChildAdapterPosition(childA);
        Mockito.doReturn(1).when(mRecyclerView).getChildAdapterPosition(childB);

        mFeedStreamSurface.navigateTab("http://someurl", childB);
        mFeedStreamSurface.navigateNewTab("http://someurl", childA);

        verify(mFeedStreamSurfaceJniMock)
                .reportOpenAction(anyLong(), any(FeedStreamSurface.class), eq("b"));
        verify(mFeedStreamSurfaceJniMock)
                .reportOpenInNewTabAction(anyLong(), any(FeedStreamSurface.class), eq("a"));
    }

    @Test
    @SmallTest
    public void testNavigateFromBottomSheetReportsCorrectSlice() {
        startupAndSetVisible();

        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());

        View childA = new View(mActivity);
        mRecyclerView.addView(childA);
        View childB = new View(mActivity);
        mRecyclerView.addView(childB);
        LinearLayout bottomSheetView = new LinearLayout(mActivity);
        View menuItem = new View(mActivity);
        bottomSheetView.addView(menuItem);

        // findChildViewContainingDescendent() won't work on its own because mRecyclerView is a
        // mockito spy, and therefore child.getParent() != mRecyclerView.
        Mockito.doReturn(childA)
                .when(mFeedStreamSurface)
                .findChildViewContainingDescendent(mRecyclerView, childA);
        Mockito.doReturn(childB)
                .when(mFeedStreamSurface)
                .findChildViewContainingDescendent(mRecyclerView, childB);
        Mockito.doReturn(0).when(mRecyclerView).getChildAdapterPosition(childA);
        Mockito.doReturn(1).when(mRecyclerView).getChildAdapterPosition(childB);

        mFeedStreamSurface.showBottomSheet(bottomSheetView, childB);
        mFeedStreamSurface.navigateTab("http://someurl", menuItem);
        mFeedStreamSurface.dismissBottomSheet();
        mFeedStreamSurface.navigateNewTab("http://someurl", menuItem);

        verify(mFeedStreamSurfaceJniMock)
                .reportOpenAction(anyLong(), any(FeedStreamSurface.class), eq("b"));
        // Bottom sheet closed for this navigation, so slice cannot be found.
        verify(mFeedStreamSurfaceJniMock)
                .reportOpenInNewTabAction(anyLong(), any(FeedStreamSurface.class), eq(""));
    }

    @Test
    @SmallTest
    public void testNavigateNoSliceFound() {
        startupAndSetVisible();

        StreamUpdate update = StreamUpdate.newBuilder()
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("a"))
                                      .addUpdatedSlices(createSliceUpdateForNewXSurfaceSlice("b"))
                                      .build();
        mFeedStreamSurface.onStreamUpdated(update.toByteArray());

        View nonConnectedView = new View(mActivity);

        // findChildViewContainingDescendent() won't work on its own because mRecyclerView is a
        // mockito spy, and therefore child.getParent() != mRecyclerView.
        Mockito.doReturn(null)
                .when(mFeedStreamSurface)
                .findChildViewContainingDescendent(mRecyclerView, nonConnectedView);

        mFeedStreamSurface.navigateTab("http://someurl", nonConnectedView);

        verify(mFeedStreamSurfaceJniMock)
                .reportOpenAction(anyLong(), any(FeedStreamSurface.class), eq(""));
    }

    @Test
    @SmallTest
    public void testScrollIsReportedOnIdle() {
        startupAndSetVisible();

        mFeedStreamSurface.streamScrolled(0, 100);
        Robolectric.getForegroundThreadScheduler().advanceBy(1000, TimeUnit.MILLISECONDS);

        verify(mFeedStreamSurfaceJniMock)
                .reportStreamScrollStart(anyLong(), any(FeedStreamSurface.class));
        verify(mFeedStreamSurfaceJniMock)
                .reportStreamScrolled(anyLong(), any(FeedStreamSurface.class), eq(100));
    }

    @Test
    @SmallTest
    public void testScrollIsReportedOnClose() {
        startupAndSetVisible();

        mFeedStreamSurface.streamScrolled(0, 100);
        mFeedStreamSurface.setStreamContentVisibility(false);

        verify(mFeedStreamSurfaceJniMock)
                .reportStreamScrollStart(anyLong(), any(FeedStreamSurface.class));
        verify(mFeedStreamSurfaceJniMock)
                .reportStreamScrolled(anyLong(), any(FeedStreamSurface.class), eq(100));
    }

    private SliceUpdate createSliceUpdateForExistingSlice(String sliceId) {
        return SliceUpdate.newBuilder().setSliceId(sliceId).build();
    }

    private SliceUpdate createSliceUpdateForNewXSurfaceSlice(String sliceId) {
        return SliceUpdate.newBuilder().setSlice(createXSurfaceSSlice(sliceId)).build();
    }

    private Slice createXSurfaceSSlice(String sliceId) {
        return Slice.newBuilder()
                .setSliceId(sliceId)
                .setXsurfaceSlice(XSurfaceSlice.newBuilder()
                                          .setXsurfaceFrame(ByteString.copyFromUtf8(TEST_DATA))
                                          .build())
                .build();
    }

    private View getNativeView(int index) {
        View view = ((FeedListContentManager.NativeViewContent) mContentManager.getContent(index))
                            .getNativeView(mParent);
        assertNotNull(view);
        assertTrue(view instanceof FrameLayout);
        return ((FrameLayout) view).getChildAt(0);
    }

    void startupAndSetVisible() {
        FeedStreamSurface.startup();
        mFeedStreamSurface.setStreamContentVisibility(true);
        mFeedStreamSurface.setStreamVisibility(true);
    }
}
