// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.Mockito.doReturn;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.SwipeRefreshHandler.BottomControlsStatus;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;

/** Unit tests for {@link org.chromium.chrome.browser.ShortcutHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SwipeRefreshHandlerUnitTest {
    private static final String OVERSCROLL_FROM_EDGE_UMA_NAME =
            "Android.OverscrollFromBottom.BottomControlsStatus";

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock BrowserControlsStateProvider mBrowserControls;

    /**
     * Test method for {@link
     * SwipeRefreshHandler#recordEdgeToEdgeOverscrollFromBottom(BrowserControlsStateProvider)} .}
     */
    @Test
    public void testRecordEdgeToEdgeOverscrollFromBottom() {
        doReturn(0).when(mBrowserControls).getBottomControlsHeight();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        OVERSCROLL_FROM_EDGE_UMA_NAME, BottomControlsStatus.HEIGHT_ZERO)) {
            SwipeRefreshHandler.recordEdgeToEdgeOverscrollFromBottom(mBrowserControls);
        }

        doReturn(50).when(mBrowserControls).getBottomControlsHeight();
        doReturn(50).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        OVERSCROLL_FROM_EDGE_UMA_NAME, BottomControlsStatus.HIDDEN)) {
            SwipeRefreshHandler.recordEdgeToEdgeOverscrollFromBottom(mBrowserControls);
        }

        doReturn(50).when(mBrowserControls).getBottomControlsHeight();
        doReturn(0).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        OVERSCROLL_FROM_EDGE_UMA_NAME, BottomControlsStatus.VISIBLE_FULL_HEIGHT)) {
            SwipeRefreshHandler.recordEdgeToEdgeOverscrollFromBottom(mBrowserControls);
        }

        doReturn(50).when(mBrowserControls).getBottomControlsHeight();
        doReturn(20).when(mBrowserControls).getBottomControlOffset();
        try (var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        OVERSCROLL_FROM_EDGE_UMA_NAME,
                        BottomControlsStatus.VISIBLE_PARTIAL_HEIGHT)) {
            SwipeRefreshHandler.recordEdgeToEdgeOverscrollFromBottom(mBrowserControls);
        }
    }
}
