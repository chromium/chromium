// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.ViewStub;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MiniPlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniPlayerCoordinatorUnitTest {
    @Mock
    private ViewStub mViewStub;
    @Mock
    private MiniPlayerLayout mLayout;
    private PropertyModel mModel;

    private MiniPlayerCoordinator mCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mLayout).when(mViewStub).inflate();

        mModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build();

        mCoordinator = new MiniPlayerCoordinator(mViewStub, mModel);
    }

    @Test
    public void testShowInflatesViewOnce() {
        mCoordinator.show(/*animate=*/false);
        verify(mViewStub, times(1)).inflate();

        assertEquals(VisibilityState.VISIBLE, mCoordinator.getVisibility());

        // Second show() shouldn't inflate the stub again.
        reset(mViewStub);
        mCoordinator.show(/*animate=*/false);
        verify(mViewStub, never()).inflate();

        assertEquals(VisibilityState.VISIBLE, mCoordinator.getVisibility());
    }

    @Test
    public void testDismissWhenNeverShown() {
        // Check that methods depending on the mediator don't crash when it's null.
        assertEquals(VisibilityState.GONE, mCoordinator.getVisibility());
        mCoordinator.dismiss(false);
    }

    @Test
    public void testShowDismiss() {
        mCoordinator.show(/*animate=*/false);
        assertEquals(VisibilityState.VISIBLE, mCoordinator.getVisibility());
        mCoordinator.dismiss(/*animate=*/false);
        assertEquals(VisibilityState.GONE, mCoordinator.getVisibility());
    }

    @Test
    public void testBindPlaybackState() {
        mCoordinator.show(/*animate=*/true);
        mModel.set(PlayerProperties.PLAYBACK_STATE, PlaybackListener.State.PLAYING);
        verify(mLayout).onPlaybackStateChanged(eq(PlaybackListener.State.PLAYING));
    }
}
