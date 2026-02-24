// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler.OpenMode;
import org.chromium.chrome.browser.xsurface.SurfaceActionsHandler.OpenUrlOptions;
import org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScope;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.JUnitTestGURLs;

import java.util.function.Supplier;

/** Unit tests for {@link FeedStream} Ctrl+Click behavior. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedStreamCtrlClickTest {
    private static final String TEST_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private RecyclerView mRecyclerView;
    private View mActionSourceView;
    private FeedStream mFeedStream;
    private FeedListContentManager mContentManager;

    @Mock private FeedSurfaceRendererBridge mFeedSurfaceRendererBridgeMock;
    @Mock private FeedServiceBridge.Natives mFeedServiceBridgeJniMock;
    @Mock private FeedReliabilityLoggingBridge.Natives mFeedReliabilityLoggingBridgeJniMock;
    @Mock private WebFeedBridge.Natives mWebFeedBridgeJni;

    @Mock private SnackbarManager mSnackbarManager;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private Profile mProfileMock;
    @Mock private HybridListRenderer mRenderer;
    @Mock private ListLayoutHelper mListLayoutHelper;
    @Mock private FeedSurfaceScope mSurfaceScope;
    @Mock private RecyclerView.Adapter mAdapter;
    @Mock private FeedReliabilityLogger mReliabilityLogger;
    @Mock private FeedActionDelegate mActionDelegate;
    @Mock private FeedContentFirstLoadWatcher mFeedContentFirstLoadWatcher;
    @Mock private Stream.StreamsMediator mStreamsMediator;

    private FeedSurfaceRendererBridge.Renderer mBridgeRenderer;

    class FeedSurfaceRendererBridgeFactory implements FeedSurfaceRendererBridge.Factory {
        @Override
        public FeedSurfaceRendererBridge create(
                Profile profile,
                FeedSurfaceRendererBridge.Renderer renderer,
                FeedReliabilityLoggingBridge reliabilityLoggingBridge,
                @StreamKind int streamKind,
                SingleWebFeedParameters webFeedParameters) {
            mBridgeRenderer = renderer;
            return mFeedSurfaceRendererBridgeMock;
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();

        FeedServiceBridgeJni.setInstanceForTesting(mFeedServiceBridgeJniMock);
        FeedReliabilityLoggingBridgeJni.setInstanceForTesting(mFeedReliabilityLoggingBridgeJniMock);
        WebFeedBridgeJni.setInstanceForTesting(mWebFeedBridgeJni);
        ProfileManager.setLastUsedProfileForTesting(mProfileMock);

        when(mWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);
        when(mRenderer.getAdapter()).thenReturn(mAdapter);
        when(mRenderer.getListLayoutHelper()).thenReturn(mListLayoutHelper);

        mFeedStream =
                new FeedStream(
                        mActivity,
                        mProfileMock,
                        mSnackbarManager,
                        null,
                        mWindowAndroid,
                        mShareDelegateSupplier,
                        StreamKind.FOR_YOU,
                        mActionDelegate,
                        mFeedContentFirstLoadWatcher,
                        mStreamsMediator,
                        null,
                        new FeedSurfaceRendererBridgeFactory());

        mContentManager = new FeedListContentManager();
        mRecyclerView = new RecyclerView(mActivity);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(mActivity));
        mRecyclerView.setAdapter(mAdapter);
        mActionSourceView = new View(mActivity);
        mRecyclerView.addView(mActionSourceView);

        mFeedStream.bind(
                mRecyclerView,
                mContentManager,
                null,
                mSurfaceScope,
                mRenderer,
                mReliabilityLogger,
                0);
    }

    private OpenUrlOptions createOpenUrlOptions() {
        return new OpenUrlOptions() {
            @Override
            public View actionSourceView() {
                return mActionSourceView;
            }
        };
    }

    @Test
    @SmallTest
    public void testOpenUrlSameTab_withCtrlClick() {
        // Simulate Ctrl down touch event
        long downTime = SystemClock.uptimeMillis();
        long eventTime = SystemClock.uptimeMillis();
        int action = MotionEvent.ACTION_DOWN;
        float x = 0;
        float y = 0;
        int metaState = KeyEvent.META_CTRL_ON;

        MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, metaState);
        mRecyclerView.onInterceptTouchEvent(event);

        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.openUrl(OpenMode.SAME_TAB, TEST_URL, createOpenUrlOptions());
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mActionDelegate)
                .openSuggestionUrl(
                        eq(WindowOpenDisposition.NEW_BACKGROUND_TAB),
                        any(),
                        anyBoolean(),
                        anyInt(),
                        eq(handler),
                        any());
    }

    @Test
    @SmallTest
    public void testOpenUrlSameTab_withoutCtrlClick() {
        // Simulate normal touch event
        long downTime = SystemClock.uptimeMillis();
        long eventTime = SystemClock.uptimeMillis();
        int action = MotionEvent.ACTION_DOWN;
        float x = 0;
        float y = 0;
        int metaState = 0;

        MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, metaState);
        mRecyclerView.onInterceptTouchEvent(event);

        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.openUrl(OpenMode.SAME_TAB, TEST_URL, createOpenUrlOptions());
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mActionDelegate)
                .openSuggestionUrl(
                        eq(WindowOpenDisposition.CURRENT_TAB),
                        any(),
                        anyBoolean(),
                        anyInt(),
                        eq(handler),
                        any());
    }

    @Test
    @SmallTest
    public void testOpenUrlSameTab_withShiftClick() {
        // Simulate Shift down touch event
        long downTime = SystemClock.uptimeMillis();
        long eventTime = SystemClock.uptimeMillis();
        int action = MotionEvent.ACTION_DOWN;
        float x = 0;
        float y = 0;
        int metaState = KeyEvent.META_SHIFT_ON;

        MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, metaState);
        mRecyclerView.onInterceptTouchEvent(event);

        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.openUrl(OpenMode.SAME_TAB, TEST_URL, createOpenUrlOptions());
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mActionDelegate)
                .openSuggestionUrl(
                        eq(WindowOpenDisposition.NEW_WINDOW),
                        any(),
                        anyBoolean(),
                        anyInt(),
                        eq(handler),
                        any());
    }

    @Test
    @SmallTest
    public void testOpenUrlSameTab_withCtrlShiftClick() {
        // Simulate Ctrl+Shift down touch event
        long downTime = SystemClock.uptimeMillis();
        long eventTime = SystemClock.uptimeMillis();
        int action = MotionEvent.ACTION_DOWN;
        float x = 0;
        float y = 0;
        int metaState = KeyEvent.META_CTRL_ON | KeyEvent.META_SHIFT_ON;

        MotionEvent event = MotionEvent.obtain(downTime, eventTime, action, x, y, metaState);
        mRecyclerView.onInterceptTouchEvent(event);

        FeedStream.FeedSurfaceActionsHandler handler =
                (FeedStream.FeedSurfaceActionsHandler)
                        mContentManager.getContextValues(0).get(SurfaceActionsHandler.KEY);

        handler.openUrl(OpenMode.SAME_TAB, TEST_URL, createOpenUrlOptions());
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mActionDelegate)
                .openSuggestionUrl(
                        eq(WindowOpenDisposition.NEW_FOREGROUND_TAB),
                        any(),
                        anyBoolean(),
                        anyInt(),
                        eq(handler),
                        any());
    }
}
