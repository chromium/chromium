// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.PlayerCoordinator;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Unit tests for {@link ExpandedPlayerMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExpandedPlayerMediatorUnitTest {
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private PlayerCoordinator.Delegate mDelegate;
    @Mock private Playback mPlayback;

    private PropertyModel mModel;
    private ExpandedPlayerMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel =
                Mockito.spy(
                        new PropertyModel.Builder(PlayerProperties.ALL_KEYS)
                                .with(
                                        PlayerProperties.EXPANDED_PLAYER_VISIBILITY,
                                        VisibilityState.GONE)
                                .build());

        mMediator = new ExpandedPlayerMediator(mModel);
    }

    @Test
    public void testShow() {
        mMediator.show();
        assertEquals(VisibilityState.SHOWING, mMediator.getVisibility());
    }

    @Test
    public void testShowAlreadyShowing() {
        mModel.set(PlayerProperties.EXPANDED_PLAYER_VISIBILITY, VisibilityState.SHOWING);
        reset(mModel);
        mMediator.show();
        assertEquals(VisibilityState.SHOWING, mMediator.getVisibility());
        verify(mModel, never()).set(eq(PlayerProperties.EXPANDED_PLAYER_VISIBILITY), anyInt());

        mModel.set(PlayerProperties.EXPANDED_PLAYER_VISIBILITY, VisibilityState.VISIBLE);
        reset(mModel);
        mMediator.show();
        assertEquals(VisibilityState.VISIBLE, mMediator.getVisibility());
        verify(mModel, never()).set(eq(PlayerProperties.EXPANDED_PLAYER_VISIBILITY), anyInt());
    }

    @Test
    public void testDismissAlreadyHiding() {
        mModel.set(PlayerProperties.EXPANDED_PLAYER_VISIBILITY, VisibilityState.HIDING);
        reset(mModel);
        mMediator.dismiss();
        assertEquals(VisibilityState.HIDING, mMediator.getVisibility());
        verify(mModel, never()).set(eq(PlayerProperties.EXPANDED_PLAYER_VISIBILITY), anyInt());
        verify(mModel, never()).set(any(WritableObjectPropertyKey.class), any());

        mModel.set(PlayerProperties.EXPANDED_PLAYER_VISIBILITY, VisibilityState.GONE);
        reset(mModel);
        mMediator.dismiss();
        assertEquals(VisibilityState.GONE, mMediator.getVisibility());
        verify(mModel, never()).set(eq(PlayerProperties.EXPANDED_PLAYER_VISIBILITY), anyInt());
        verify(mModel, never()).set(any(WritableObjectPropertyKey.class), any());
    }

    @Test
    public void testDismiss() {
        mMediator.show();
        mMediator.dismiss();
        assertEquals(VisibilityState.HIDING, mMediator.getVisibility());
    }
}
