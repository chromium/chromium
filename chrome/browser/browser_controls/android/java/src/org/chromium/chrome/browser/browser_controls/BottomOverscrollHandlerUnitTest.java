// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.cc.input.BrowserControlsState;

/** Unit test for {@link BottomOverscrollHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomOverscrollHandlerUnitTest {
    private static final String CAN_START_OVERSCROLL_UMA_NAME =
            "Android.OverscrollFromBottom.CanStart";
    private static final String DID_TRIGGER_OVERSCROLL_UMA_NAME =
            "Android.OverscrollFromBottom.DidTriggerOverscroll";

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock private BrowserControlsVisibilityManager mBrowserControls;

    private FakeBrowserStateBrowserControlsVisibilityDelegate mDelegate;
    private BottomOverscrollHandler mHandler;

    private static class FakeBrowserStateBrowserControlsVisibilityDelegate
            extends BrowserStateBrowserControlsVisibilityDelegate {
        public int showControlsTransientCallCount;

        public FakeBrowserStateBrowserControlsVisibilityDelegate() {
            super(ObservableSuppliers.createNonNull(false));
        }

        @Override
        public void showControlsTransient() {
            showControlsTransientCallCount++;
        }
    }

    @Before
    public void setUp() {
        mDelegate = new FakeBrowserStateBrowserControlsVisibilityDelegate();
        doReturn(mDelegate).when(mBrowserControls).getBrowserVisibilityDelegate();
        mHandler = new BottomOverscrollHandler(mBrowserControls);
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
        mDelegate.set(BrowserControlsState.HIDDEN);
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(CAN_START_OVERSCROLL_UMA_NAME, false)) {
            assertFalse(mHandler.start());
        }
    }

    @Test
    public void testStart_controlsFullyVisible() {
        mDelegate.set(BrowserControlsState.BOTH);
        doReturn(0).when(mBrowserControls).getTopControlOffset();
        doReturn(0).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(CAN_START_OVERSCROLL_UMA_NAME, false)) {
            assertFalse(mHandler.start());
        }
    }

    @Test
    public void testStart_success() {
        mDelegate.set(BrowserControlsState.BOTH);
        doReturn(1).when(mBrowserControls).getTopControlOffset();
        doReturn(1).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(CAN_START_OVERSCROLL_UMA_NAME, true)) {
            assertTrue(mHandler.start());
        }
    }

    @Test
    public void testRelease_showControls() {
        mDelegate.set(BrowserControlsState.BOTH);
        doReturn(1).when(mBrowserControls).getTopControlOffset();
        doReturn(1).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(DID_TRIGGER_OVERSCROLL_UMA_NAME, true)) {
            mHandler.start();
            mHandler.release(true);
            RobolectricUtil.runAllBackgroundAndUi();
            assertEquals(1, mDelegate.showControlsTransientCallCount);
        }
    }

    @Test
    public void testRelease_notAllowed() {
        mDelegate.set(BrowserControlsState.BOTH);
        doReturn(1).when(mBrowserControls).getTopControlOffset();
        doReturn(1).when(mBrowserControls).getBottomControlOffset();
        mHandler.start();
        assertEquals(0, mDelegate.showControlsTransientCallCount);
    }

    @Test
    public void testRelease_notStarted() {
        mHandler.release(true);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(0, mDelegate.showControlsTransientCallCount);
    }

    @Test
    public void testReset() {
        mDelegate.set(BrowserControlsState.BOTH);
        doReturn(1).when(mBrowserControls).getTopControlOffset();
        doReturn(1).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(DID_TRIGGER_OVERSCROLL_UMA_NAME, false)) {
            mHandler.start();
            mHandler.reset();
            mHandler.release(true);
            RobolectricUtil.runAllBackgroundAndUi();
            assertEquals(0, mDelegate.showControlsTransientCallCount);
        }
    }

    @Test
    public void testReset_notStarted() {
        mDelegate.set(BrowserControlsState.BOTH);
        doReturn(0).when(mBrowserControls).getTopControlOffset();
        doReturn(0).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(DID_TRIGGER_OVERSCROLL_UMA_NAME)
                        .build()) {
            assertFalse(mHandler.start());
            mHandler.reset();
            mHandler.release(true);
            RobolectricUtil.runAllBackgroundAndUi();
        }
    }
}
