// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.PlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link ExpandedPlayerCoordinator} and ExpandedPlayerViewBinder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExpandedPlayerCoordinatorUnitTest {
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Playback mPlayback;
    @Mock private PlayerCoordinator.Delegate mDelegate;
    private PropertyModel mModel;
    @Mock private InteractionHandler mHandler;
    @Mock private ExpandedPlayerMediator mMediator;
    @Mock private ExpandedPlayerSheetContent mSheetContent;
    @Mock private OptionsMenuSheetContent mOptionsMenuSheetContent;
    @Mock private VoiceMenu mVoiceMenu;
    private ExpandedPlayerCoordinator mCoordinator;
    @Captor ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;
    BottomSheetObserver mBottomSheetObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);
        mModel =
                new PropertyModel.Builder(PlayerProperties.ALL_KEYS)
                        .with(PlayerProperties.INTERACTION_HANDLER, mHandler)
                        .build();
        mCoordinator =
                new ExpandedPlayerCoordinator(
                        ApplicationProvider.getApplicationContext(),
                        mDelegate,
                        mModel,
                        mMediator,
                        mSheetContent);
        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        mBottomSheetObserver = mBottomSheetObserverCaptor.getValue();
    }

    @Test
    public void testShow() {
        mCoordinator.show();
        verify(mMediator).show();
    }

    @Test
    public void testDismiss() {
        mCoordinator.show();
        mCoordinator.dismiss();
        verify(mMediator, times(1)).dismiss();
        verify(mMediator).setShowMiniPlayerOnDismiss(eq(false));
    }

    @Test
    public void testGetVisibility() {
        when(mMediator.getVisibility()).thenReturn(VisibilityState.GONE);
        assertTrue(mCoordinator.getVisibility() == VisibilityState.GONE);
        when(mMediator.getVisibility()).thenReturn(VisibilityState.SHOWING);
        assertTrue(mCoordinator.getVisibility() == VisibilityState.SHOWING);
    }

    @Test
    public void testOnSheetContentChanged() {
        mCoordinator.setSheetContent(null);
        mBottomSheetObserver.onSheetContentChanged(mSheetContent);
        verify(mMediator).setVisibility(VisibilityState.GONE);
        verify(mMediator).setShowMiniPlayerOnDismiss(eq(true));
    }

    @Test
    public void testOnSheetContentChanged_dontShowMiniPlayer() {
        mCoordinator.setSheetContent(mSheetContent);
        mBottomSheetObserver.onSheetContentChanged(mOptionsMenuSheetContent);
        verify(mMediator, never()).setShowMiniPlayerOnDismiss(eq(true));
    }

    @Test
    public void testOnSheetOpened() {
        mCoordinator.setSheetContent(null);
        mBottomSheetObserver.onSheetOpened(StateChangeReason.NAVIGATION);
        verify(mMediator).setVisibility(VisibilityState.VISIBLE);
        verify(mHandler).onShouldHideMiniPlayer();
    }

    @Test
    public void testOnSheetClosed_dontShowMiniPlayer() {
        when(mMediator.getShowMiniPlayerOnDismiss()).thenReturn(false);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        mBottomSheetObserver.onSheetClosed(StateChangeReason.NONE);
        verify(mSheetContent).notifySheetClosed(eq(mSheetContent));
        verify(mHandler, never()).onShouldRestoreMiniPlayer();
    }

    @Test
    public void testOnSheetClosed_dontShowMiniPlayer_submenus() {
        when(mMediator.getShowMiniPlayerOnDismiss()).thenReturn(false);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mOptionsMenuSheetContent);
        mBottomSheetObserver.onSheetClosed(StateChangeReason.NONE);
        verify(mSheetContent).notifySheetClosed(eq(mOptionsMenuSheetContent));
        verify(mHandler, never()).onShouldRestoreMiniPlayer();
    }

    @Test
    public void testOnSheetClosed_showMiniPlayer() {
        when(mMediator.getShowMiniPlayerOnDismiss()).thenReturn(true);
        when(mBottomSheetController.getCurrentSheetContent()).thenReturn(mSheetContent);
        mBottomSheetObserver.onSheetClosed(StateChangeReason.BACK_PRESS);
        verify(mSheetContent).notifySheetClosed(eq(mSheetContent));
        verify(mHandler).onShouldRestoreMiniPlayer();
    }

    @Test
    public void testShowPlayer_resumePlaybackUpdates() {
        mModel.set(PlayerProperties.HIDDEN_AND_PLAYING, true);
        mBottomSheetObserver.onSheetContentChanged(mOptionsMenuSheetContent);
        mBottomSheetObserver.onSheetContentChanged(mSheetContent);
        verify(mMediator).setHiddenAndPlaying(eq(false));
    }

    @Test
    public void testBindVisibility() {
        mModel.set(PlayerProperties.EXPANDED_PLAYER_VISIBILITY, VisibilityState.HIDING);
        verify(mSheetContent).hide();
    }

    @Test
    public void testBindTitleAndPublisher() {
        assertTrue(mModel.get(PlayerProperties.TITLE) == null);
        mCoordinator.show();
        mModel.set(PlayerProperties.TITLE, "title");
        verify(mSheetContent).setTitle("title");
        assertTrue(mModel.get(PlayerProperties.TITLE).equals("title"));

        assertTrue(mModel.get(PlayerProperties.PUBLISHER) == null);
        mModel.set(PlayerProperties.PUBLISHER, "publisher");
        verify(mSheetContent).setPublisher("publisher");
        assertTrue(mModel.get(PlayerProperties.PUBLISHER).equals("publisher"));
    }

    @Test
    public void testBindElapsed() {
        mModel.set(PlayerProperties.ELAPSED_NANOS, 20L);
        verify(mSheetContent).setElapsed(20L);
        assertEquals(20L, mModel.get(PlayerProperties.ELAPSED_NANOS));
    }

    @Test
    public void testBindDuration() {
        mModel.set(PlayerProperties.DURATION_NANOS, 30L);
        verify(mSheetContent).setDuration(30L);
        assertEquals(30L, mModel.get(PlayerProperties.DURATION_NANOS));
    }

    @Test
    public void testBindProgress() {
        mModel.set(PlayerProperties.PROGRESS, 0.5f);
        verify(mSheetContent).setProgress(eq(0.5f));
    }

    @Test
    public void testBindSpeed() {
        mModel.set(PlayerProperties.SPEED, 2f);
        verify(mSheetContent).setSpeed(eq(2f));
    }

    @Test
    public void testBindPlaybackState() {
        mCoordinator.show();
        mModel.set(PlayerProperties.PLAYBACK_STATE, PlaybackListener.State.PLAYING);
        verify(mSheetContent).onPlaybackStateChanged(PlaybackListener.State.PLAYING);
        mModel.set(PlayerProperties.PLAYBACK_STATE, PlaybackListener.State.PAUSED);
        verify(mSheetContent).onPlaybackStateChanged(PlaybackListener.State.PAUSED);
    }

    @Test
    public void testBindVoiceList() {
        doReturn(mVoiceMenu).when(mSheetContent).getVoiceMenu();

        var voices = List.of(new PlaybackVoice("en", "a"));
        mModel.set(PlayerProperties.VOICES_LIST, voices);

        verify(mVoiceMenu).setVoices(eq(voices));
    }

    @Test
    public void testBindVoiceSelection() {
        doReturn(mVoiceMenu).when(mSheetContent).getVoiceMenu();

        mModel.set(PlayerProperties.SELECTED_VOICE_ID, "asdf");

        verify(mVoiceMenu).setVoiceSelection(eq("asdf"));
    }

    @Test
    public void testBindVoicePreviewState() {
        doReturn(mVoiceMenu).when(mSheetContent).getVoiceMenu();
        mModel.set(PlayerProperties.PREVIEWING_VOICE_ID, "asdf");
        mModel.set(PlayerProperties.VOICE_PREVIEW_PLAYBACK_STATE, PlaybackListener.State.PLAYING);
        verify(mVoiceMenu).updatePreviewButtons(eq("asdf"), eq(PlaybackListener.State.PLAYING));
    }
}
