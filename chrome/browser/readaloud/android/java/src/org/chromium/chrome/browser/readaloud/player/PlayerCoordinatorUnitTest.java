// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud.player;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.ReadAloudPrefs;
import org.chromium.chrome.browser.readaloud.player.expanded.ExpandedPlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.expanded.Menu;
import org.chromium.chrome.browser.readaloud.player.mini.MiniPlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.mini.MiniPlayerLayout;
import org.chromium.chrome.browser.readaloud.testing.MockPrefServiceHelper;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.chrome.modules.readaloud.Player;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlayerCoordinatorUnitTest {
    @Mock private Activity mActivity;
    @Mock private LayoutInflater mLayoutInflater;
    @Mock private ViewStub mMiniPlayerViewStub;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private MiniPlayerLayout mMiniPlayerLayout;
    @Mock private MiniPlayerCoordinator mMiniPlayerCoordinator;
    @Mock private Playback mPlayback;
    @Mock private PlayerCoordinator.Observer mObserver;
    @Mock private PlayerMediator mMediator;
    @Mock private MiniPlayerCoordinator mMiniPlayer;
    @Mock private View mExpandedPlayerContentView;
    @Mock private TextView mSpeedButton;
    @Mock private View mForwardButton;
    @Mock private View mBackButton;
    @Mock private Resources mResources;
    private MockPrefServiceHelper mMockPrefServiceHelper;

    private PlayerCoordinator mPlayerCoordinator;
    private PropertyModel mModel;

    @Mock private Player.Delegate mDelegate;
    @Mock private ExpandedPlayerCoordinator mExpandedPlayer;
    @Mock private Menu mMenu;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mPlayerCoordinator =
                new PlayerCoordinator(mMiniPlayer, mMediator, mDelegate, mExpandedPlayer);
    }

    @Test
    public void testConstructor() {
        doReturn(mActivity).when(mDelegate).getActivity();
        doReturn(mMiniPlayerViewStub)
                .when(mActivity)
                .findViewById(eq(R.id.readaloud_mini_player_stub));
        doReturn(mLayoutInflater)
                .when(mActivity)
                .getSystemService(eq(Context.LAYOUT_INFLATER_SERVICE));

        doReturn(mExpandedPlayerContentView)
                .when(mLayoutInflater)
                .inflate(eq(R.layout.readaloud_expanded_player_layout), any());
        doReturn(Mockito.mock(TextView.class))
                .when(mExpandedPlayerContentView)
                .findViewById(anyInt());

        doReturn(mMenu).when(mLayoutInflater).inflate(eq(R.layout.readaloud_menu), any());
        doReturn(Mockito.mock(TextView.class)).when(mMenu).findViewById(anyInt());
        doReturn(mResources).when(mActivity).getResources();
        doReturn("").when(mResources).getString(anyInt(), anyInt());

        doReturn(mMiniPlayerLayout).when(mMiniPlayerViewStub).inflate();
        doReturn(mBottomSheetController).when(mDelegate).getBottomSheetController();

        mMockPrefServiceHelper = new MockPrefServiceHelper();
        PrefService prefs = mMockPrefServiceHelper.getPrefService();
        ReadAloudPrefs.setSpeed(prefs, 2f);
        doReturn(prefs).when(mDelegate).getPrefService();

        mPlayerCoordinator = new PlayerCoordinator(mDelegate);

        // Mini player should be inflated and attached.
        verify(mMiniPlayerViewStub).inflate();
        // User prefs should be read into the model.
        verify(prefs).getString(eq("readaloud.speed"));
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
    public void testExpand() {
        mPlayerCoordinator.expand();
        verify(mExpandedPlayer, never()).show();
        mPlayerCoordinator.playbackReady(mPlayback, PlaybackListener.State.PLAYING);
        mPlayerCoordinator.expand();
        verify(mExpandedPlayer).show();
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
}
