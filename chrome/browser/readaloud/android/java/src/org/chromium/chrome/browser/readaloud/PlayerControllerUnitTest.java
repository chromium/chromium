// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.miniplayer.MiniPlayerCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.readaloud.ExpandedPlayer;
import org.chromium.chrome.modules.readaloud.Playback;

/** Unit tests for {@link ReadAloudController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlayerControllerUnitTest {
    @Mock
    private MiniPlayerCoordinator mMiniPlayer;
    @Mock
    private ExpandedPlayer mExpandedPlayer;
    @Mock
    private Tab mTab;
    @Mock
    private Playback mPlayback;

    @Captor
    ArgumentCaptor<MiniPlayerCoordinator.Observer> mMiniObserverCaptor;
    @Captor
    ArgumentCaptor<ExpandedPlayer.Observer> mExpandedObserverCaptor;

    private PlayerController mPlayerController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mPlayerController = new PlayerController(mMiniPlayer, mExpandedPlayer);
    }

    @Test
    public void testPlaybackRequested_expandedPlayerGone() {
        doReturn(PlayerState.GONE).when(mExpandedPlayer).getState();

        mPlayerController.playTabRequested(null);
        verify(mMiniPlayer, times(1)).show(/*animate=*/eq(true), /*playback=*/eq(null));
    }

    @Test
    public void testPlaybackRequested_expandedPlayerShowing() {
        doReturn(PlayerState.SHOWING).when(mExpandedPlayer).getState();

        mPlayerController.playTabRequested(null);
        verify(mMiniPlayer, times(1)).show(/*animate=*/eq(true), /*playback=*/eq(null));
    }

    @Test
    public void testPlaybackRequested_expandedPlayerHiding() {
        doReturn(PlayerState.HIDING).when(mExpandedPlayer).getState();

        mPlayerController.playTabRequested(null);
        verify(mMiniPlayer, times(1)).show(/*animate=*/eq(true), /*playback=*/eq(null));
    }

    @Test
    public void testPlaybackRequested_expandedPlayerVisible() {
        doReturn(PlayerState.VISIBLE).when(mExpandedPlayer).getState();

        mPlayerController.playTabRequested(null);
        verify(mMiniPlayer, times(1)).show(/*animate=*/eq(false), /*playback=*/eq(null));
    }

    @Test
    public void testPlaybackReady_expandedPlayerShowingOrVisible() {
        doReturn(PlayerState.SHOWING).when(mExpandedPlayer).getState();

        mPlayerController.playbackReady(mPlayback);
        verify(mExpandedPlayer, times(1)).show(eq(mPlayback));

        doReturn(PlayerState.VISIBLE).when(mExpandedPlayer).getState();

        mPlayerController.playbackReady(mPlayback);
        verify(mExpandedPlayer, times(2)).show(eq(mPlayback));
    }

    @Test
    public void testPlaybackReady_expandedPlayerHidingOrGone() {
        doReturn(PlayerState.HIDING).when(mExpandedPlayer).getState();

        mPlayerController.playbackReady(mPlayback);
        verify(mExpandedPlayer, never()).show(any());

        doReturn(PlayerState.GONE).when(mExpandedPlayer).getState();

        mPlayerController.playbackReady(mPlayback);
        verify(mExpandedPlayer, never()).show(any());
    }

    @Test
    public void testPlaybackReady_miniPlayerShowingOrVisible() {
        doReturn(PlayerState.GONE).when(mExpandedPlayer).getState();
        doReturn(PlayerState.SHOWING).when(mMiniPlayer).getState();

        mPlayerController.playbackReady(mPlayback);
        verify(mMiniPlayer, times(1)).show(eq(true), eq(mPlayback));

        doReturn(PlayerState.VISIBLE).when(mMiniPlayer).getState();

        mPlayerController.playbackReady(mPlayback);
        verify(mMiniPlayer, times(2)).show(eq(true), eq(mPlayback));
    }

    @Test
    public void testPlaybackReady_miniPlayerHidingOrGone() {
        doReturn(PlayerState.GONE).when(mExpandedPlayer).getState();
        doReturn(PlayerState.HIDING).when(mMiniPlayer).getState();

        mPlayerController.playbackReady(mPlayback);
        verify(mMiniPlayer, never()).show(eq(true), eq(mPlayback));

        doReturn(PlayerState.GONE).when(mMiniPlayer).getState();

        mPlayerController.playbackReady(mPlayback);
        verify(mMiniPlayer, never()).show(eq(true), eq(mPlayback));
    }

    @Test
    public void testStopAndHideAll() {
        doReturn(PlayerState.GONE).when(mExpandedPlayer).getState();

        mPlayerController.playbackReady(mPlayback);
        mPlayerController.stopAndHideAll();
        verify(mPlayback, times(1)).release();
        verify(mMiniPlayer, times(1)).dismiss(eq(true));
        verify(mExpandedPlayer, times(1)).dismiss();
    }

    @Test
    public void testMiniPlayerRequestClose() {
        verify(mMiniPlayer).addObserver(mMiniObserverCaptor.capture());
        doReturn(PlayerState.GONE).when(mExpandedPlayer).getState();

        mMiniObserverCaptor.getValue().onCloseClicked();
        verify(mMiniPlayer, times(1)).dismiss(eq(true));
    }

    @Test
    public void testExpandedPlayerRequestClose() {
        verify(mExpandedPlayer).addObserver(mExpandedObserverCaptor.capture());
        doReturn(PlayerState.VISIBLE).when(mMiniPlayer).getState();
        doReturn(PlayerState.VISIBLE).when(mExpandedPlayer).getState();

        mExpandedObserverCaptor.getValue().onCloseClicked();
        verify(mMiniPlayer, times(1)).dismiss(eq(false));
        verify(mExpandedPlayer, times(1)).dismiss();
    }

    @Test
    public void testMiniPlayerRequestExpand() {
        verify(mMiniPlayer).addObserver(mMiniObserverCaptor.capture());
        doReturn(PlayerState.GONE).when(mExpandedPlayer).getState();
        mPlayerController.playbackReady(mPlayback);

        mMiniObserverCaptor.getValue().onExpandRequested();
        verify(mExpandedPlayer, times(1)).show(eq(mPlayback));
    }
}
