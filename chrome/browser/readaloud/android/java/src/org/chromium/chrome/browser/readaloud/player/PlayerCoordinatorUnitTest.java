// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud.player;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.view.ViewStub;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.mini.MiniPlayerLayout;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlayerCoordinatorUnitTest {
    @Mock
    private ViewStub mMiniPlayerViewStub;
    @Mock
    private MiniPlayerLayout mLayout;
    @Mock
    private Playback mPlayback;
    @Mock
    private PlayerCoordinator.Observer mObserver;

    private PlayerCoordinator mPlayerCoordinator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mLayout).when(mMiniPlayerViewStub).inflate();
        mPlayerCoordinator = new PlayerCoordinator(
                ApplicationProvider.getApplicationContext(), mMiniPlayerViewStub);
        mModel = mPlayerCoordinator.getModelForTesting();
    }

    @Test
    public void testInitialModelState() {
        assertNotNull(mModel.get(PlayerProperties.INTERACTION_HANDLER));
    }

    @Test
    public void testPlayTabRequested() {
        mPlayerCoordinator.playTabRequested();
        // Mini player shows in buffering state
        assertEquals(PlaybackListener.State.BUFFERING,
                (int) mModel.get(PlayerProperties.PLAYBACK_STATE));
        verify(mMiniPlayerViewStub).inflate();
        assertEquals(
                VisibilityState.VISIBLE, (int) mModel.get(PlayerProperties.MINI_PLAYER_VISIBILITY));
        assertEquals(true,
                (boolean) mModel.get(PlayerProperties.MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES));
    }

    @Test
    public void testPlaybackReady() {
        mPlayerCoordinator.playTabRequested();
        mPlayerCoordinator.playbackReady(mPlayback, PlaybackListener.State.PLAYING);
        assertEquals(
                PlaybackListener.State.PLAYING, (int) mModel.get(PlayerProperties.PLAYBACK_STATE));
    }

    @Test
    public void testPlaybackFailed() {
        mPlayerCoordinator.playTabRequested();
        mPlayerCoordinator.playbackFailed();
        assertEquals(
                PlaybackListener.State.ERROR, (int) mModel.get(PlayerProperties.PLAYBACK_STATE));
    }

    @Test
    public void testDismissPlayers() {
        mPlayerCoordinator.playTabRequested();
        mPlayerCoordinator.dismissPlayers();

        assertEquals(
                PlaybackListener.State.STOPPED, (int) mModel.get(PlayerProperties.PLAYBACK_STATE));
        assertEquals(true,
                (boolean) mModel.get(PlayerProperties.MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES));
        assertEquals(
                VisibilityState.GONE, (int) mModel.get(PlayerProperties.MINI_PLAYER_VISIBILITY));
    }

    @Test
    public void testCloseClicked() {
        mPlayerCoordinator.addObserver(mObserver);
        mPlayerCoordinator.closeClicked();
        verify(mObserver).onRequestClosePlayers();
    }

    @Test
    public void testDestroy() {
        mPlayerCoordinator.addObserver(mObserver);
        // Show mini player
        mPlayerCoordinator.playTabRequested();

        mPlayerCoordinator.destroy();

        // Mini player is gone.
        assertEquals(
                PlaybackListener.State.STOPPED, (int) mModel.get(PlayerProperties.PLAYBACK_STATE));
        assertEquals(true,
                (boolean) mModel.get(PlayerProperties.MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES));
        assertEquals(
                VisibilityState.GONE, (int) mModel.get(PlayerProperties.MINI_PLAYER_VISIBILITY));
        verify(mObserver, never()).onRequestClosePlayers();
    }
}
