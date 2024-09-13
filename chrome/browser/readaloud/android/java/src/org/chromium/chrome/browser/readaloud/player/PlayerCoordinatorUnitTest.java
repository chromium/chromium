// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud.player;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.ViewStub;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.readaloud.ReadAloudMiniPlayerSceneLayer;
import org.chromium.chrome.browser.readaloud.ReadAloudMiniPlayerSceneLayerJni;
import org.chromium.chrome.browser.readaloud.ReadAloudPrefs;
import org.chromium.chrome.browser.readaloud.player.expanded.ExpandedPlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.mini.MiniPlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.mini.MiniPlayerLayout;
import org.chromium.chrome.browser.readaloud.testing.MockPrefServiceHelper;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.Player;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.prefs.PrefService;

import java.util.List;

/** Unit tests for {@link PlayerCoordinator}. */
@Config(manifest = Config.NONE)
@RunWith(BaseRobolectricTestRunner.class)
public class PlayerCoordinatorUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock ReadAloudMiniPlayerSceneLayer.Natives mSceneLayerNativeMock;

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private Playback mPlayback;
    @Mock private PlayerCoordinator.Observer mObserver;
    @Mock private PlayerMediator mMediator;
    @Mock private MiniPlayerCoordinator mMiniPlayer;
    private MockPrefServiceHelper mMockPrefServiceHelper;

    private PlayerCoordinator mPlayerCoordinator;

    @Mock private Player.Delegate mDelegate;
    @Mock private ExpandedPlayerCoordinator mExpandedPlayer;

    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(ReadAloudMiniPlayerSceneLayerJni.TEST_HOOKS, mSceneLayerNativeMock);
        doReturn(123456789L).when(mSceneLayerNativeMock).init(any());
        doReturn(mBottomControlsStacker).when(mDelegate).getBottomControlsStacker();
        doReturn(mBrowserControlsStateProvider).when(mBottomControlsStacker).getBrowserControls();
        mPlayerCoordinator =
                new PlayerCoordinator(mMiniPlayer, mMediator, mDelegate, mExpandedPlayer);
    }

    @After
    public void tearDown() {
        MiniPlayerCoordinator.setViewStubForTesting(null);
    }

    @Test
    public void testConstructor() {
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        // Need to set theme before inflating layout.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        ViewStub mockMiniPlayerStub = Mockito.mock(ViewStub.class);
        MiniPlayerCoordinator.setViewStubForTesting(mockMiniPlayerStub);
        var miniPlayerLayout =
                (MiniPlayerLayout)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.readaloud_mini_player_layout, null);
        doReturn(miniPlayerLayout).when(mockMiniPlayerStub).inflate();

        doReturn(mBottomSheetController).when(mDelegate).getBottomSheetController();

        mMockPrefServiceHelper = new MockPrefServiceHelper();
        PrefService prefs = mMockPrefServiceHelper.getPrefService();
        ReadAloudPrefs.setSpeed(prefs, 2f);
        doReturn(prefs).when(mDelegate).getPrefService();
        doReturn(Mockito.mock(LayoutManager.class)).when(mDelegate).getLayoutManager();
        doReturn(new ObservableSupplierImpl<List<PlaybackVoice>>())
                .when(mDelegate)
                .getCurrentLanguageVoicesSupplier();
        doReturn(new ObservableSupplierImpl<String>()).when(mDelegate).getVoiceIdSupplier();
        doReturn(mActivity).when(mDelegate).getActivity();

        mPlayerCoordinator = new PlayerCoordinator(mDelegate);

        // Mini player should be inflated and attached.
        verify(mockMiniPlayerStub).inflate();
        // User prefs should be read into the model.
        verify(prefs).getDouble(eq("readaloud.speed"));
    }

    @Test
    public void testPlayTabRequested() {
        mPlayerCoordinator.playTabRequested();

        // Mini player shows in buffering state
        verify(mMediator).setPlayback(eq(null));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.BUFFERING));
        verify(mMiniPlayer).show(eq(true));
    }

    @Test
    public void testPlayTabRequested_withExpandedPlayerVisible() {
        doReturn(true).when(mExpandedPlayer).anySheetShowing();
        mPlayerCoordinator.playTabRequested();

        // Mini player is not shown.
        verify(mMediator).setPlayback(eq(null));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.BUFFERING));
        verify(mMiniPlayer, never()).show(anyBoolean());
    }

    @Test
    public void testPlaybackReady() {
        mPlayerCoordinator.playTabRequested();
        verify(mMediator).setPlayback(eq(null));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.BUFFERING));
        reset(mMediator);
        mPlayerCoordinator.playbackReady(mPlayback, PlaybackListener.State.PLAYING);

        verify(mMediator).setPlayback(eq(mPlayback));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.PLAYING));
    }

    @Test
    public void testPlaybackFailed() {
        mPlayerCoordinator.playTabRequested();
        verify(mMediator).setPlayback(eq(null));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.BUFFERING));
        reset(mMediator);
        mPlayerCoordinator.playbackFailed();

        verify(mMediator).setPlayback(eq(null));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.ERROR));
    }

    @Test
    public void testRecordPlaybackDuration() {
        mPlayerCoordinator.recordPlaybackDuration();
        verify(mMediator).recordPlaybackDuration();
    }

    @Test
    public void testExpand() {
        mPlayerCoordinator.playbackReady(mPlayback, PlaybackListener.State.PLAYING);
        mPlayerCoordinator.expand();
        verify(mExpandedPlayer).show();
        verify(mMiniPlayer).dismiss(/* animate= */ eq(false));
    }

    @Test
    public void testRestoreMiniPlayer() {
        mPlayerCoordinator.restoreMiniPlayer();
        verify(mMiniPlayer).show(eq(true));
        verify(mMediator).setHiddenAndPlaying(eq(false));
    }

    @Test
    public void testDismissPlayers() {
        mPlayerCoordinator.playTabRequested();
        verify(mMediator).setPlayback(eq(null));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.BUFFERING));
        reset(mMediator);
        mPlayerCoordinator.dismissPlayers();

        verify(mMediator).setPlayback(eq(null));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.STOPPED));
        verify(mMiniPlayer).dismiss(eq(true));
        verify(mMediator).setHiddenAndPlaying(eq(false));
    }

    @Test
    public void testDismissPlayers_restorablePlayer() {
        mPlayerCoordinator.playTabRequested();
        verify(mMediator).setPlayback(eq(null));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.BUFFERING));
        reset(mMediator);

        doReturn(true).when(mMediator).isPlayerRestorable();
        mPlayerCoordinator.dismissPlayers();

        verify(mMediator).setPlayback(eq(null));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.STOPPED));
        verify(mMiniPlayer, never()).dismiss(eq(true));
        verify(mExpandedPlayer, never()).dismiss();
        verify(mMediator).setHiddenAndPlaying(eq(false));
    }

    @Test
    public void testHideMiniPlayer_visible() {
        doReturn(VisibilityState.VISIBLE).when(mMiniPlayer).getVisibility();
        mPlayerCoordinator.playbackReady(mPlayback, PlaybackListener.State.PLAYING);

        verify(mMediator).setPlayback(eq(mPlayback));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.PLAYING));

        mPlayerCoordinator.hideMiniPlayer();
        verify(mMiniPlayer).getVisibility();
        verify(mMediator).setPlayback(eq(mPlayback));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.PLAYING));
        verify(mMiniPlayer).dismiss(eq(true));
        verify(mMediator).setHiddenAndPlaying(eq(true));
    }

    @Test
    public void testHideMiniPlayer_noopWhenHidden() {
        doReturn(VisibilityState.HIDING).when(mMiniPlayer).getVisibility();
        mPlayerCoordinator.playbackReady(mPlayback, PlaybackListener.State.PLAYING);

        verify(mMediator).setPlayback(eq(mPlayback));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.PLAYING));
        reset(mMediator);
        mPlayerCoordinator.hideMiniPlayer();
        verify(mMiniPlayer).getVisibility();
        verify(mMediator, never()).setPlayback(eq(mPlayback));
        verify(mMediator, never()).setPlaybackState(eq(PlaybackListener.State.PLAYING));
        verify(mMiniPlayer, never()).dismiss(eq(true));
        verify(mMediator, never()).setHiddenAndPlaying(eq(true));
    }

    @Test
    public void testHideAndRestoreMiniPlayer() {
        doReturn(VisibilityState.VISIBLE).when(mMiniPlayer).getVisibility();
        doReturn(VisibilityState.GONE).when(mExpandedPlayer).getVisibility();

        mPlayerCoordinator.playbackReady(mPlayback, PlaybackListener.State.PLAYING);

        verify(mMediator).setPlayback(eq(mPlayback));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.PLAYING));

        mPlayerCoordinator.hidePlayers();
        verify(mMiniPlayer).getVisibility();
        verify(mExpandedPlayer).getVisibility();
        verify(mMediator).setPlayback(eq(mPlayback));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.PLAYING));
        verify(mMiniPlayer).dismiss(eq(true));
        verify(mMediator).setHiddenAndPlaying(eq(true));
        verify(mExpandedPlayer, never()).dismiss();

        mPlayerCoordinator.restorePlayers();
        verify(mMiniPlayer).show(eq(true));
        verify(mMediator).setHiddenAndPlaying(eq(false));
        verify(mExpandedPlayer, never()).show();
    }

    @Test
    public void testHideAndRestoreExpandedPlayer() {
        doReturn(VisibilityState.GONE).when(mMiniPlayer).getVisibility();
        doReturn(VisibilityState.VISIBLE).when(mExpandedPlayer).getVisibility();

        mPlayerCoordinator.playbackReady(mPlayback, PlaybackListener.State.PLAYING);

        verify(mMediator).setPlayback(eq(mPlayback));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.PLAYING));

        mPlayerCoordinator.hidePlayers();
        verify(mMiniPlayer).getVisibility();
        verify(mExpandedPlayer).getVisibility();
        verify(mMediator).setPlayback(eq(mPlayback));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.PLAYING));
        verify(mExpandedPlayer).dismiss();
        verify(mMiniPlayer, never()).dismiss(eq(true));
        verify(mMediator).setHiddenAndPlaying(eq(true));

        mPlayerCoordinator.restorePlayers();
        verify(mExpandedPlayer).show();
        verify(mMiniPlayer, never()).show(eq(true));
        verify(mMediator).setHiddenAndPlaying(eq(false));
    }

    @Test
    public void testHideAndRestoreNoPlayerVisible() {
        mPlayerCoordinator.hidePlayers();
        verify(mExpandedPlayer, never()).dismiss();
        verify(mMiniPlayer, never()).dismiss(eq(true));

        mPlayerCoordinator.restorePlayers();
        verify(mExpandedPlayer, never()).show();
        verify(mMiniPlayer, never()).show(eq(true));
    }

    @Test
    public void testOnScreenStatusChanged() {
        mPlayerCoordinator.onScreenStatusChanged(true);
        verify(mMediator).onScreenStatusChanged(true);
    }

    @Test
    public void testSetPlayerRestorable() {
        mPlayerCoordinator.setPlayerRestorable(true);
        verify(mMediator).setPlayerRestorable(true);
    }

    @Test
    public void testCloseClicked() {
        mPlayerCoordinator =
                new PlayerCoordinator(mMiniPlayer, mMediator, mDelegate, mExpandedPlayer);
        mPlayerCoordinator.addObserver(mObserver);
        mPlayerCoordinator.closeClicked();
        verify(mObserver).onRequestClosePlayers();
    }

    @Test
    public void testVoiceMenuClosed() {
        mPlayerCoordinator.addObserver(mObserver);
        mPlayerCoordinator.voiceMenuClosed();
        verify(mObserver).onVoiceMenuClosed();
    }

    @Test
    public void testDestroy() {
        mPlayerCoordinator.addObserver(mObserver);
        // Show mini player
        mPlayerCoordinator.playTabRequested();
        reset(mMediator);

        mPlayerCoordinator.destroy();

        verify(mMediator).setPlayback(eq(null));
        verify(mMediator).setPlaybackState(eq(PlaybackListener.State.STOPPED));
        verify(mMediator).destroy();
        verify(mMiniPlayer).dismiss(Mockito.anyBoolean());
        verify(mExpandedPlayer).dismiss();
        verify(mObserver, never()).onRequestClosePlayers();
    }

    @Test
    public void testHideExpandedPlayer() {
        mPlayerCoordinator.hideExpandedPlayer();

        verify(mExpandedPlayer).dismiss(true);
        verify(mMiniPlayer, never()).dismiss(anyBoolean());
    }
}
