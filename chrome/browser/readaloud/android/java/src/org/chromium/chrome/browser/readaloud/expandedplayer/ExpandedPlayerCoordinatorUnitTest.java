// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.expandedplayer;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.modules.readaloud.ExpandedPlayer.Observer;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Unit tests for {@link ExpandedPlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExpandedPlayerCoordinatorUnitTest {
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private Playback mPlayback;
    @Mock
    private Observer mObserver;

    private ExpandedPlayerCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mCoordinator = new ExpandedPlayerCoordinator(
                ApplicationProvider.getApplicationContext(), mBottomSheetController);
    }

    @Test
    public void testShowInflatesViewOnce() {
        mCoordinator.show(mPlayback);
        verify(mBottomSheetController, times(1)).requestShowContent(any(), eq(true));

        // Second show() shouldn't inflate the stub again.
        reset(mBottomSheetController);
        mCoordinator.show(mPlayback);
        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testDismiss() {
        mCoordinator.show(mPlayback);
        mCoordinator.dismiss();
        verify(mBottomSheetController)
                .hideContent(eq(mCoordinator.getSheetContentForTesting()), eq(true));
    }

    @Test
    public void testObserveClickClose() {
        mCoordinator.addObserver(mObserver);
        mCoordinator.show(mPlayback);

        assertTrue(mCoordinator.getSheetContentForTesting()
                           .getContentView()
                           .findViewById(R.id.readaloud_expanded_player_close_button)
                           .performClick());

        verify(mObserver, times(1)).onCloseClicked();
    }
}
