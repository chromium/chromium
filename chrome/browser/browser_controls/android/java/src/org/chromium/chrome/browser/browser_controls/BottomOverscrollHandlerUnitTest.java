// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

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
import org.chromium.chrome.browser.browser_controls.BottomOverscrollHandler.BottomControlsStatus;

/** Unit test for {@link BottomOverscrollHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomOverscrollHandlerUnitTest {

    private static final String OVERSCROLL_FROM_EDGE_UMA_NAME =
            "Android.OverscrollFromBottom.BottomControlsStatus";

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    @Mock BrowserControlsStateProvider mBrowserControls;

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
}
