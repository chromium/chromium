// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
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

/** Unit tests for {@link DefaultResizingStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DefaultResizingStrategyUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebViewResizingHelper mMockHelper;
    @Mock private ResizeLock mMockLock;

    private DefaultResizingStrategy mStrategy;

    @Before
    public void setUp() {
        when(mMockHelper.requestResize()).thenReturn(mMockLock);
        mStrategy = new DefaultResizingStrategy(mMockHelper);
    }

    @Test
    public void testLockLifecycle_AcquiresAndUnlocksCorrectly() {
        mStrategy.onSheetOffsetChanged(
                /* offsetPx= */ 100f, /* halfHeightPx= */ 200f, /* fullHeightPx= */ 1000f);

        // Not resizing yet -> requestResize not called.
        verify(mMockHelper, never()).requestResize();

        // Start resizing, offset between half (200) and full (1000) -> lock acquired.
        mStrategy.onSheetOffsetChanged(500f, 200f, 1000f);
        mStrategy.onSheetResizingStatusChanged(true);
        verify(mMockHelper).requestResize();

        // Offset drops below half -> lock unlocked.
        mStrategy.onSheetOffsetChanged(150f, 200f, 1000f);
        verify(mMockLock).unlock();

        // Offset moves back into range -> lock acquired again and placeholder height updated.
        clearInvocations(mMockHelper);
        mStrategy.onSheetOffsetChanged(600f, 200f, 1000f);
        verify(mMockHelper).requestResize();
        verify(mMockHelper).updatePlaceholderHeight(600);

        // Stop resizing -> lock unlocked.
        mStrategy.onSheetResizingStatusChanged(false);
        verify(mMockLock, times(2)).unlock();
    }

    @Test
    public void testDestroy_UnlocksHeldLock() {
        mStrategy.onSheetOffsetChanged(500f, 200f, 1000f);
        mStrategy.onSheetResizingStatusChanged(true);
        verify(mMockHelper).requestResize();

        mStrategy.destroy();
        verify(mMockLock).unlock();
    }

    @Test
    public void testNoLockAcquired_WhenNotResizing() {
        mStrategy.onSheetOffsetChanged(500f, 200f, 1000f);
        verify(mMockHelper, never()).requestResize();
    }

    @Test
    public void testNoLockAcquired_WhenOffsetAtOrBelowHalfHeight() {
        mStrategy.onSheetResizingStatusChanged(true);
        mStrategy.onSheetOffsetChanged(200f, 200f, 1000f);
        verify(mMockHelper, never()).requestResize();

        mStrategy.onSheetOffsetChanged(100f, 200f, 1000f);
        verify(mMockHelper, never()).requestResize();
    }

    @Test
    public void testNoLockAcquired_WhenOffsetAtOrAboveFullHeight() {
        mStrategy.onSheetResizingStatusChanged(true);
        mStrategy.onSheetOffsetChanged(1000f, 200f, 1000f);
        verify(mMockHelper, never()).requestResize();

        mStrategy.onSheetOffsetChanged(1200f, 200f, 1000f);
        verify(mMockHelper, never()).requestResize();
    }

    @Test
    public void testResizingStrategyFactory_CreatesNonNullInstance() {
        ResizingStrategy strategy = ResizingStrategyFactory.create(mMockHelper);
        assertNotNull(strategy);
        strategy.destroy();
    }
}
