// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_bottom_sheet.WebViewResizingHelper.ResizeLock;

/** Unit tests for {@link DragDirectionResizingStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DragDirectionResizingStrategyUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebViewResizingHelper mMockHelper;
    @Mock private ResizeLock mMockLock;

    private DragDirectionResizingStrategy mStrategy;

    @Before
    public void setUp() {
        when(mMockHelper.requestResize()).thenReturn(mMockLock);
        mStrategy = new DragDirectionResizingStrategy(mMockHelper);
    }

    @Test
    public void testUpwardDrag_OnlyShowsBetweenHalfAndFullHeight() {
        mStrategy.onSheetResizingStatusChanged(true);

        // Dragging upward from 0 to 150 (below halfHeight 200) -> lock not acquired.
        mStrategy.onSheetOffsetChanged(50f, 0f, 200f, 1000f);
        mStrategy.onSheetOffsetChanged(150f, 0f, 200f, 1000f);
        verify(mMockHelper, never()).requestResize();

        // Dragging upward into range (500) -> lock acquired.
        mStrategy.onSheetOffsetChanged(500f, 0f, 200f, 1000f);
        verify(mMockHelper).requestResize();
        verify(mMockHelper).updatePlaceholderHeight(500);

        // Stop resizing -> lock unlocked.
        mStrategy.onSheetResizingStatusChanged(false);
        verify(mMockLock).unlock();
    }

    @Test
    public void testDownwardDrag_FromFullHeight_HoldsUntilRelease() {
        mStrategy.onSheetResizingStatusChanged(true);

        // Initial offset at full height (1000)
        mStrategy.onSheetOffsetChanged(1000f, 0f, 200f, 1000f);

        // Drag downward to 800 -> downward drag detected, lock acquired.
        mStrategy.onSheetOffsetChanged(800f, 0f, 200f, 1000f);
        verify(mMockHelper).requestResize();

        // Continue drag downward below half height (150) -> lock remains held!
        mStrategy.onSheetOffsetChanged(150f, 0f, 200f, 1000f);
        verify(mMockLock, never()).unlock();
        verify(mMockHelper).updatePlaceholderHeight(150);

        // Finger release -> lock unlocked.
        mStrategy.onSheetResizingStatusChanged(false);
        verify(mMockLock).unlock();
    }

    @Test
    public void testDownwardDrag_ReachesPeekState_ResetsAndUnlocks() {
        mStrategy.onSheetResizingStatusChanged(true);

        // Start at full height (1000) and drag downward to 500 with peekHeight = 50 -> lock
        // acquired.
        mStrategy.onSheetOffsetChanged(1000f, 50f, 200f, 1000f);
        mStrategy.onSheetOffsetChanged(500f, 50f, 200f, 1000f);
        verify(mMockHelper).requestResize();

        // Drag all the way down to peek state (50 offset) -> lock unlocked and state reset.
        mStrategy.onSheetOffsetChanged(50f, 50f, 200f, 1000f);
        verify(mMockLock).unlock();

        // Dragging back up from 50 to 100 (below half height) should NOT re-acquire lock because
        // downward state was reset.
        clearInvocations(mMockHelper);
        mStrategy.onSheetOffsetChanged(100f, 50f, 200f, 1000f);
        verify(mMockHelper, never()).requestResize();
    }

    @Test
    public void testUpwardDragThenDownwardChange_HoldsUntilRelease() {
        mStrategy.onSheetResizingStatusChanged(true);

        // Drag upward from 50 to 150 (below half height)
        mStrategy.onSheetOffsetChanged(50f, 0f, 200f, 1000f);
        mStrategy.onSheetOffsetChanged(150f, 0f, 200f, 1000f);
        verify(mMockHelper, never()).requestResize();

        // Direction change downward from 150 to 120 -> lock acquired immediately!
        mStrategy.onSheetOffsetChanged(120f, 0f, 200f, 1000f);
        verify(mMockHelper).requestResize();
        verify(mMockHelper).updatePlaceholderHeight(120);

        // Release finger -> lock unlocked.
        mStrategy.onSheetResizingStatusChanged(false);
        verify(mMockLock).unlock();
    }

    @Test
    public void testDestroy_UnlocksHeldLock() {
        mStrategy.onSheetResizingStatusChanged(true);
        mStrategy.onSheetOffsetChanged(1000f, 0f, 200f, 1000f);
        mStrategy.onSheetOffsetChanged(800f, 0f, 200f, 1000f);
        verify(mMockHelper).requestResize();

        mStrategy.destroy();
        verify(mMockLock).unlock();
    }
}
