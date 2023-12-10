// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud.player;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
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
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.readaloud.ReadAloudMiniPlayerSceneLayer;
import org.chromium.chrome.browser.readaloud.ReadAloudMiniPlayerSceneLayerJni;
import org.chromium.chrome.browser.readaloud.ReadAloudPrefs;
import org.chromium.chrome.browser.readaloud.player.expanded.ExpandedPlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.expanded.Menu;
import org.chromium.chrome.browser.readaloud.player.expanded.MenuItem;
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
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlayerCoordinatorUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock ReadAloudMiniPlayerSceneLayer.Natives mSceneLayerNativeMock;

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
    @Mock private SeekBar mSeekBar;
    private MockPrefServiceHelper mMockPrefServiceHelper;

    private PlayerCoordinator mPlayerCoordinator;

    @Mock private Player.Delegate mDelegate;
    @Mock private ExpandedPlayerCoordinator mExpandedPlayer;
    @Mock private Menu mMenu;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(ReadAloudMiniPlayerSceneLayerJni.TEST_HOOKS, mSceneLayerNativeMock);
        doReturn(123456789L).when(mSceneLayerNativeMock).init(any());
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
        doReturn(Mockito.mock(LinearLayout.class))
                .when(mExpandedPlayerContentView)
                .findViewById(R.id.normal_layout);
        doReturn(Mockito.mock(LinearLayout.class))
                .when(mExpandedPlayerContentView)
                .findViewById(R.id.error_layout);
        doReturn(Mockito.mock(SeekBar.class))
                .when(mExpandedPlayerContentView)
                .findViewById(R.id.readaloud_expanded_player_seek_bar);

        doReturn(mMenu).when(mLayoutInflater).inflate(eq(R.layout.readaloud_menu), any());
        doReturn(Mockito.mock(MenuItem.class))
                .when(mMenu)
                .addItem(anyInt(), anyInt(), any(), anyInt(), any());
        doReturn(Mockito.mock(MenuItem.class)).when(mMenu).getItem(anyInt());
        doReturn(Mockito.mock(TextView.class)).when(mMenu).findViewById(anyInt());
        doReturn(mResources).when(mActivity).getResources();
        doReturn("").when(mResources).getString(anyInt(), anyInt());

        doReturn(mMiniPlayerLayout).when(mMiniPlayerViewStub).inflate();
        doReturn(mBottomSheetController).when(mDelegate).getBottomSheetController();

        mMockPrefServiceHelper = new MockPrefServiceHelper();
        PrefService prefs = mMockPrefServiceHelper.getPrefService();
        ReadAloudPrefs.setSpeed(prefs, 2f);
        doReturn(prefs).when(mDelegate).getPrefService();
        doReturn(Mockito.mock(LayoutManager.class)).when(mDelegate).getLayoutManager();
        doReturn(Mockito.mock(BrowserControlsSizer.class))
                .when(mDelegate)
                .getBrowserControlsSizer();
        doReturn(new ObservableSupplierImpl<List<PlaybackVoice>>())
                .when(mDelegate)
                .getCurrentLanguageVoicesSupplier();
        doReturn(new ObservableSupplierImpl<String>()).when(mDelegate).getVoiceIdSupplier();

        mPlayerCoordinator = new PlayerCoordinator(mDelegate);

        // Mini player should be inflated and attached.
        verify(mMiniPlayerViewStub).inflate();
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
        doReturn(VisibilityState.VISIBLE).when(mExpandedPlayer).getVisibility();
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
    public void testExpand() {
        mPlayerCoordinator.playbackReady(mPlayback, PlaybackListener.State.PLAYING);
        mPlayerCoordinator.expand();
        verify(mExpandedPlayer).show();
    }

    @Test
    public void testRestoreMiniPlayer() {
        mPlayerCoordinator.restoreMiniPlayer();
        verify(mMiniPlayer).show(eq(true));
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
}
