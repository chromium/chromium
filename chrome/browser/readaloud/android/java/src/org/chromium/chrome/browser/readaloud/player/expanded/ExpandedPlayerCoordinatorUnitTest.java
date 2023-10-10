// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.anyFloat;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

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

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mDelegate.getBottomSheetController()).thenReturn(mBottomSheetController);
        mModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build();
        mCoordinator =
                Mockito.spy(
                        new ExpandedPlayerCoordinator(
                                ApplicationProvider.getApplicationContext(), mDelegate, mModel));

        doAnswer(
                        invocation -> {
                            mCoordinator.setSheetContentForTesting(mSheetContent);
                            return null;
                        })
                .when(mCoordinator)
                .makeSheetContent();
        doNothing().when(mSheetContent).setSpeed(anyFloat());

        doAnswer(
                        invocation -> {
                            mCoordinator.setMediatorForTesting(mMediator);
                            when(mMediator.getVisibility()).thenReturn(VisibilityState.SHOWING);
                            return null;
                        })
                .when(mCoordinator)
                .makeMediator();
    }

    @Test
    public void testShowInflatesViewOnce() {
        mCoordinator.show();
        verify(mCoordinator, times(1)).makeSheetContent();

        // Second show() shouldn't inflate the stub again.
        reset(mCoordinator);
        mCoordinator.show();
        verify(mCoordinator, never()).makeSheetContent();
    }

    @Test
    public void testDismiss() {
        mCoordinator.show();
        mCoordinator.dismiss();
        verify(mMediator, times(1)).dismiss();
    }

    @Test
    public void testGetVisibility() {
        assertTrue(mCoordinator.getVisibility() == VisibilityState.GONE);
        mCoordinator.show();
        assertTrue(mCoordinator.getVisibility() == VisibilityState.SHOWING);
    }

    @Test
    public void testBindVisibility() {
        mCoordinator.show();
        mModel.set(PlayerProperties.EXPANDED_PLAYER_VISIBILITY, VisibilityState.HIDING);
        verify(mSheetContent).hide();
    }

    @Test
    public void testBindSpeed() {
        mCoordinator.show();
        mModel.set(PlayerProperties.SPEED, 2f);
        verify(mSheetContent).setSpeed(eq(2f));
    }
}
