// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.util.ArrayMap;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.AppCompatTextView;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
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
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
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
import org.chromium.chrome.browser.xsurface.feed.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScope;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feed.proto.wire.ReliabilityLoggingEnums.DiscoverLaunchResult;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

import java.nio.charset.StandardCharsets;
import java.util.Map;

/** Unit tests for {@link FeedStream}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
// TODO(crbug.com/40182398): Rewrite using paused loop. See crbug for details.
@LooperMode(LooperMode.Mode.LEGACY)
public class SingleWebFeedStreamTest {
    private static final int LOAD_MORE_TRIGGER_LOOKAHEAD = 5;
    private static final int LOAD_MORE_TRIGGER_SCROLL_DISTANCE_DP = 100;
    private static final String TEST_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    private static final OpenUrlOptions DEFAULT_OPEN_URL_OPTIONS = new OpenUrlOptions() {};

    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private FakeLinearLayoutManager mLayoutManager;
    private FeedStream mFeedStream;
    private FeedListContentManager mContentManager;

    @Mock private FeedSurfaceRendererBridge mFeedSurfaceRendererBridgeMock;
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private FeedReliabilityLoggingBridge.Natives mFeedReliabilityLoggingBridgeJniMock;

    @Mock private SnackbarManager mSnackbarManager;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private FeedSurfaceScope mSurfaceScope;
    @Mock private FeedReliabilityLogger mReliabilityLogger;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;
    private StubSnackbarController mSnackbarController = new StubSnackbarController();
    @Mock private Runnable mMockRunnable;
    @Mock private Callback<Boolean> mMockRefreshCallback;
    @Mock private FeedStream.ShareHelperWrapper mShareHelper;
    @Mock private Profile mProfileMock;
    @Mock private HybridListRenderer mRenderer;
    @Mock private RecyclerView.Adapter mAdapter;
    @Mock private FeedLaunchReliabilityLogger mLaunchReliabilityLogger;
    @Mock private FeedActionDelegate mActionDelegate;
    @Mock WebFeedBridge.Natives mWebFeedBridgeJni;

    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;
    @Captor private ArgumentCaptor<Callback<FollowResults>> mFollowResultsCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<UnfollowResults>> mUnfollowResultsCallbackCaptor;
    @Mock private WebFeedFollowUpdate.Callback mWebFeedFollowUpdateCallback;
    @Mock private FeedContentFirstLoadWatcher mFeedContentFirstLoadWatcher;
    @Mock private Stream.StreamsMediator mStreamsMediator;

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

    @Rule public JniMocker mocker = new JniMocker();

    private void setFeatureOverrides(boolean feedLoadingPlaceholderOn) {
        Map<String, Boolean> overrides = new ArrayMap<>();
        overrides.put(ChromeFeatureList.FEED_LOADING_PLACEHOLDER, feedLoadingPlaceholderOn);
        FeatureList.setTestFeatures(overrides);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).get();

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
                        /* shareSupplier= */ mShareDelegateSupplier,
                        StreamKind.SINGLE_WEB_FEED,
                        mActionDelegate,
                        /* FeedContentFirstLoadWatcher= */ null,
                        /* streamsMediator= */ null,
                        new SingleWebFeedParameters(
                                "WebFeedId".getBytes(), SingleWebFeedEntryPoint.OTHER),
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
    public void testBind() {
        bindToView();
        // Called surfaceOpened.
        verify(mFeedSurfaceRendererBridgeMock).surfaceOpened();
        // Set handlers in contentmanager.
        assertEquals(2, mContentManager.getContextValues(0).size());
    }

    @Test
    public void testUnbind() {
        bindToView();
        mFeedStream.unbind(false, false);
        verify(mFeedSurfaceRendererBridgeMock).surfaceClosed();
        // Unset handlers in contentmanager.
        assertEquals(0, mContentManager.getContextValues(0).size());
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
    public void testLogLaunchFinishedOnOpenSuggestionUrlNewTab() {
        when(mLaunchReliabilityLogger.isLaunchInProgress()).thenReturn(true);
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);
        handler.openUrl(OpenMode.NEW_TAB, TEST_URL, DEFAULT_OPEN_URL_OPTIONS);

        // Don't log "launch finished" if the card was opened in a new tab in the background.
        verify(mLaunchReliabilityLogger, never())
                .logLaunchFinished(anyLong(), eq(DiscoverLaunchResult.CARD_TAPPED.getNumber()));
    }

    @Test
    @SmallTest
    public void testLogLaunchFinishedOnOpenUrlNewTab() {
        when(mLaunchReliabilityLogger.isLaunchInProgress()).thenReturn(true);
        bindToView();
        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);
        handler.openUrl(OpenMode.NEW_TAB, TEST_URL, DEFAULT_OPEN_URL_OPTIONS);
        // Don't log "launch finished" if the card was opened in a new tab in the background.
        verify(mLaunchReliabilityLogger, never())
                .logLaunchFinished(anyLong(), eq(DiscoverLaunchResult.CARD_TAPPED.getNumber()));
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
