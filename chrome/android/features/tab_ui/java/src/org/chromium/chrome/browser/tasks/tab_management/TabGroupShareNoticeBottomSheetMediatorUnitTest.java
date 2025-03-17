// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupShareNoticeBottomSheetCoordinator.TabGroupShareNoticeBottomSheetCoordinatorDelegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/** Unit tests for {@link TabGroupShareNoticeBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupShareNoticeBottomSheetMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TabGroupShareNoticeBottomSheetCoordinatorDelegate mDelegate;
    private TabGroupShareNoticeBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mMediator = new TabGroupShareNoticeBottomSheetMediator(mBottomSheetController, mDelegate);
        when(mDelegate.requestShowContent()).thenReturn(true);
    }

    @Test
    public void testRequestShowContent() {
        mMediator.requestShowContent();
        verify(mDelegate).requestShowContent();
    }

    @Test
    public void testHide() {
        mMediator.hide(StateChangeReason.SWIPE);
        verify(mDelegate).hide(StateChangeReason.SWIPE);
    }

    @Test
    public void testObserver_OnSheetClosed() {
        BottomSheetObserver bottomSheetObserver = mMediator.getBottomSheetObserver();
        bottomSheetObserver.onSheetClosed(StateChangeReason.SWIPE);
        verify(mDelegate).onSheetClosed();
    }

    @Test
    public void testObserver_OnSheetHidden() {
        BottomSheetObserver bottomSheetObserver = mMediator.getBottomSheetObserver();
        bottomSheetObserver.onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.SWIPE);
        verify(mDelegate).onSheetClosed();
    }

    @Test
    public void testObserver_OnSheetNotHidden() {
        BottomSheetObserver bottomSheetObserver = mMediator.getBottomSheetObserver();
        bottomSheetObserver.onSheetStateChanged(SheetState.HALF, StateChangeReason.SWIPE);
        verify(mDelegate, never()).onSheetClosed();
    }
}
