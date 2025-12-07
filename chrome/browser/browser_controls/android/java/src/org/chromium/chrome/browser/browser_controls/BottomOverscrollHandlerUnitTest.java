// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BottomOverscrollHandler.BottomControlsStatus;

/** Unit test for {@link BottomOverscrollHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomOverscrollHandlerUnitTest {
    private static final String OVERSCROLL_FROM_EDGE_UMA_NAME =
            "Android.OverscrollFromBottom.BottomControlsStatus";
    private static final String CAN_START_OVERSCROLL_UMA_NAME =
            "Android.OverscrollFromBottom.CanStart";
    private static final String DID_TRIGGER_OVERSCROLL_UMA_NAME =
            "Android.OverscrollFromBottom.DidTriggerOverscroll";

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private BrowserControlsVisibilityManager mBrowserControls;
    @Mock private BrowserStateBrowserControlsVisibilityDelegate mDelegate;

    private BottomOverscrollHandler mHandler;

    @Before
    public void setUp() {
        doReturn(mDelegate).when(mBrowserControls).getBrowserVisibilityDelegate();
        mHandler = new BottomOverscrollHandler(mBrowserControls);
    }

    /**
     * Test method for {@link
     * BottomOverscrollHandler#recordEdgeToEdgeOverscrollFromBottom(BrowserControlsStateProvider)}
     * .}
     */
    @Test
    public void testRecordEdgeToEdgeOverscrollFromBottom_Zero() {
        doReturn(0).when(mBrowserControls).getBottomControlsHeight();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        OVERSCROLL_FROM_EDGE_UMA_NAME, BottomControlsStatus.HEIGHT_ZERO)) {
            BottomOverscrollHandler.recordEdgeToEdgeOverscrollFromBottom(mBrowserControls);
        }
    }

    @Test
    public void testRecordEdgeToEdgeOverscrollFromBottom_Hidden() {
        doReturn(50).when(mBrowserControls).getBottomControlsHeight();
        doReturn(50).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        OVERSCROLL_FROM_EDGE_UMA_NAME, BottomControlsStatus.HIDDEN)) {
            BottomOverscrollHandler.recordEdgeToEdgeOverscrollFromBottom(mBrowserControls);
        }
    }

    @Test
    public void testRecordEdgeToEdgeOverscrollFromBottom_Full() {
        doReturn(50).when(mBrowserControls).getBottomControlsHeight();
        doReturn(0).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        OVERSCROLL_FROM_EDGE_UMA_NAME, BottomControlsStatus.VISIBLE_FULL_HEIGHT)) {
            BottomOverscrollHandler.recordEdgeToEdgeOverscrollFromBottom(mBrowserControls);
        }
    }

    @Test
    public void testRecordEdgeToEdgeOverscrollFromBottom_Partial() {
        doReturn(50).when(mBrowserControls).getBottomControlsHeight();
        doReturn(20).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        OVERSCROLL_FROM_EDGE_UMA_NAME,
                        BottomControlsStatus.VISIBLE_PARTIAL_HEIGHT)) {
            BottomOverscrollHandler.recordEdgeToEdgeOverscrollFromBottom(mBrowserControls);
        }
    }

    @Test
    public void testStart_visibilityForced() {
        doReturn(true).when(mBrowserControls).isVisibilityForced();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(CAN_START_OVERSCROLL_UMA_NAME, false)) {
            assertFalse(mHandler.start());
        }
    }

    @Test
    public void testStart_notBothState() {
        doReturn(BrowserControlsState.HIDDEN).when(mDelegate).get();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(CAN_START_OVERSCROLL_UMA_NAME, false)) {
            assertFalse(mHandler.start());
        }
    }

    @Test
    public void testStart_controlsFullyVisible() {
        doReturn(BrowserControlsState.BOTH).when(mDelegate).get();
        doReturn(0).when(mBrowserControls).getTopControlOffset();
        doReturn(0).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(CAN_START_OVERSCROLL_UMA_NAME, false)) {
            assertFalse(mHandler.start());
        }
    }

    @Test
    public void testStart_success() {
        doReturn(BrowserControlsState.BOTH).when(mDelegate).get();
        doReturn(1).when(mBrowserControls).getTopControlOffset();
        doReturn(1).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(CAN_START_OVERSCROLL_UMA_NAME, true)) {
            assertTrue(mHandler.start());
        }
    }

    @Test
    public void testRelease_showControls() {
        doReturn(BrowserControlsState.BOTH).when(mDelegate).get();
        doReturn(1).when(mBrowserControls).getTopControlOffset();
        doReturn(1).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(DID_TRIGGER_OVERSCROLL_UMA_NAME, true)) {
            mHandler.start();
            mHandler.release(true);
            ShadowLooper.runUiThreadTasks();
            verify(mDelegate).showControlsTransient();
        }
    }

    @Test
    public void testRelease_notAllowed() {
        doReturn(BrowserControlsState.BOTH).when(mDelegate).get();
        doReturn(1).when(mBrowserControls).getTopControlOffset();
        doReturn(1).when(mBrowserControls).getBottomControlOffset();
        mHandler.start();
        verify(mDelegate, never()).showControlsTransient();
    }

    @Test
    public void testRelease_notStarted() {
        mHandler.release(true);
        ShadowLooper.runUiThreadTasks();
        verify(mDelegate, never()).showControlsTransient();
    }

    @Test
    public void testReset() {
        doReturn(BrowserControlsState.BOTH).when(mDelegate).get();
        doReturn(1).when(mBrowserControls).getTopControlOffset();
        doReturn(1).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(DID_TRIGGER_OVERSCROLL_UMA_NAME, false)) {
            mHandler.start();
            mHandler.reset();
            mHandler.release(true);
            ShadowLooper.runUiThreadTasks();
            verify(mDelegate, never()).showControlsTransient();
        }
    }

    @Test
    public void testReset_notStarted() {
        doReturn(BrowserControlsState.BOTH).when(mDelegate).get();
        doReturn(0).when(mBrowserControls).getTopControlOffset();
        doReturn(0).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(DID_TRIGGER_OVERSCROLL_UMA_NAME)
                        .build()) {
            assertFalse(mHandler.start());
            mHandler.reset();
            mHandler.release(true);
            ShadowLooper.runUiThreadTasks();
        }
    }
}
