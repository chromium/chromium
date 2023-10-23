// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.eq;
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
import org.chromium.chrome.browser.readaloud.player.PlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ExpandedPlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExpandedPlayerCoordinatorUnitTest {
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Playback mPlayback;
    @Mock private PlayerCoordinator.Delegate mDelegate;
    private PropertyModel mModel;
    @Mock private ExpandedPlayerMediator mMediator;
    @Mock private ExpandedPlayerSheetContent mSheetContent;
    private ExpandedPlayerCoordinator mCoordinator;
    @Captor ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;
    BottomSheetObserver mBottomSheetObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);
        mModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build();
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
    }

    @Test
    public void testOnSheetOpened() {
        mCoordinator.setSheetContent(null);
        mBottomSheetObserver.onSheetOpened(StateChangeReason.NAVIGATION);
        verify(mMediator).setVisibility(VisibilityState.VISIBLE);
    }

    @Test
    public void testOnSheetClosed() {
        mBottomSheetObserver.onSheetClosed(StateChangeReason.NAVIGATION);
        verify(mSheetContent).notifySheetClosed();
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
    public void testBindSpeed() {
        mModel.set(PlayerProperties.SPEED, 2f);
        verify(mSheetContent).setSpeed(eq(2f));
    }

    @Test
    public void testBindPlaybackState() {
        mCoordinator.show();
        mModel.set(PlayerProperties.PLAYBACK_STATE, PlaybackListener.State.PLAYING);
        verify(mSheetContent).setPlaying(true);
        mModel.set(PlayerProperties.PLAYBACK_STATE, PlaybackListener.State.PAUSED);
        verify(mSheetContent).setPlaying(false);
    }
}
