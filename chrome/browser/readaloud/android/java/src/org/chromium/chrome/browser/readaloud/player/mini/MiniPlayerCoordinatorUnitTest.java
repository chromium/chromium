// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.readaloud.ReadAloudMiniPlayerSceneLayer;
import org.chromium.chrome.browser.readaloud.ReadAloudMiniPlayerSceneLayerJni;
import org.chromium.chrome.browser.readaloud.player.PlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MiniPlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniPlayerCoordinatorUnitTest {
    private static final String TITLE = "Title";
    private static final String PUBLISHER = "Publisher";

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock ReadAloudMiniPlayerSceneLayer.Natives mSceneLayerNativeMock;

    @Mock private Activity mActivity;
    @Mock private Context mContextForInflation;
    @Mock private LayoutInflater mLayoutInflater;
    @Mock private ViewStub mViewStub;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private BottomControlsStacker mBottomControlsStacker;
    @Mock private LayoutManager mLayoutManager;
    @Mock private MiniPlayerLayout mLayout;
    @Mock private MiniPlayerMediator mMediator;
    @Mock private ReadAloudMiniPlayerSceneLayer mSceneLayer;
    @Mock private PlayerCoordinator mPlayerCoordinator;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private View mView;
    private PropertyModel mSharedModel;
    private PropertyModel mModel;

    private MiniPlayerCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mLayout).when(mViewStub).inflate();
        doReturn(mViewStub).when(mActivity).findViewById(eq(R.id.readaloud_mini_player_stub));
        doReturn(mLayoutInflater)
                .when(mContextForInflation)
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSharedModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build();
        mModel = new PropertyModel.Builder(Properties.ALL_KEYS).build();
        mJniMocker.mock(ReadAloudMiniPlayerSceneLayerJni.TEST_HOOKS, mSceneLayerNativeMock);
        doReturn(123456789L).when(mSceneLayerNativeMock).init(any());
        doReturn(mModel).when(mMediator).getModel();
        doReturn(mBrowserControlsStateProvider).when(mBottomControlsStacker).getBrowserControls();
        mCoordinator =
                new MiniPlayerCoordinator(
                        mContextForInflation,
                        mSharedModel,
                        mMediator,
                        mLayout,
                        mSceneLayer,
                        mLayoutManager,
                        mPlayerCoordinator,
                        mUserEducationHelper);
    }

    @Test
    public void testViewInflated() {
        // Test the real constructor
        reset(mViewStub);
        doReturn(mLayout).when(mViewStub).inflate();
        mCoordinator =
                new MiniPlayerCoordinator(
                        mActivity,
                        mContextForInflation,
                        mSharedModel,
                        mBottomControlsStacker,
                        mLayoutManager,
                        mPlayerCoordinator,
                        mUserEducationHelper);
        verify(mViewStub).inflate();
        verify(mLayoutManager).addSceneOverlay(eq(mSceneLayer));
    }

    @Test
    public void testShow() {
        mCoordinator.show(/* animate= */ false);
        verify(mMediator).show(eq(false));

        // Second show() shouldn't inflate the stub again.
        reset(mViewStub);
        mCoordinator.show(/* animate= */ false);
        verify(mMediator, times(2)).show(eq(false));
    }

    @Test
    public void testOnShown_requestingIPH() {
        // if there's no container to anchor IPH against, don't request it.
        mCoordinator.onShown(/*container*/ null);
        verify(mUserEducationHelper, never()).requestShowIPH(any(IPHCommand.class));

        mCoordinator.onShown(mView);
        verify(mUserEducationHelper).requestShowIPH(any(IPHCommand.class));
    }

    @Test
    public void testDismissWhenNeverShown() {
        // Ensure there's no crash.
        assertEquals(VisibilityState.GONE, mCoordinator.getVisibility());
        mCoordinator.dismiss(false);
    }

    @Test
    public void testDismiss() {
        mCoordinator.dismiss(/* animate= */ false);
        verify(mMediator).dismiss(eq(false));
    }

    @Test
    public void testBindPlaybackState() {
        mCoordinator.show(/* animate= */ true);
        mSharedModel.set(PlayerProperties.PLAYBACK_STATE, PlaybackListener.State.PLAYING);
        verify(mLayout).onPlaybackStateChanged(eq(PlaybackListener.State.PLAYING));
    }

    @Test
    public void testBindTitle() {
        mCoordinator.show(/* animate= */ true);
        mSharedModel.set(PlayerProperties.TITLE, TITLE);
        verify(mLayout).setTitle(eq(TITLE));
    }

    @Test
    public void testBindPublisher() {
        mCoordinator.show(/* animate= */ true);
        mSharedModel.set(PlayerProperties.PUBLISHER, PUBLISHER);
        verify(mLayout).setPublisher(eq(PUBLISHER));
    }

    @Test
    public void testBindProgress() {
        mCoordinator.show(/* animate= */ true);
        mSharedModel.set(PlayerProperties.PROGRESS, 0.5f);
        verify(mLayout).setProgress(eq(0.5f));
    }

    @Test
    public void testBindYOffset() {
        mCoordinator.show(/* animate= */ true);
        mModel.set(Properties.Y_OFFSET, -100);
        verify(mLayout).setYOffset(eq(-100));
    }
}
