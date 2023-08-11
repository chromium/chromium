// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.expandedplayer;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.modules.readaloud.ExpandedPlayer.State;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ExpandedPlayerMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExpandedPlayerMediatorUnitTest {
    @Mock
    private BottomSheetController mBottomSheetController;

    private PropertyModel mModel;
    private ExpandedPlayerMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = Mockito.spy(new PropertyModel.Builder(ExpandedPlayerProperties.ALL_KEYS)
                                     .with(ExpandedPlayerProperties.STATE_KEY, State.GONE)
                                     .build());

        mMediator = new ExpandedPlayerMediator(mBottomSheetController, mModel);
    }

    @Test
    public void testInitialStateAfterConstructMediator() {
        verify(mBottomSheetController, times(1)).addObserver(eq(mMediator));
        assertEquals(State.GONE, mMediator.getState());
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();
        verify(mBottomSheetController, times(1)).removeObserver(eq(mMediator));
    }

    @Test
    public void testShow() {
        mMediator.show();
        assertEquals(State.SHOWING, mMediator.getState());
    }

    @Test
    public void testShowAlreadyShowing() {
        mModel.set(ExpandedPlayerProperties.STATE_KEY, State.SHOWING);
        reset(mModel);
        mMediator.show();
        assertEquals(State.SHOWING, mMediator.getState());
        verify(mModel, never()).set(any(), any());

        mModel.set(ExpandedPlayerProperties.STATE_KEY, State.VISIBLE);
        reset(mModel);
        mMediator.show();
        assertEquals(State.VISIBLE, mMediator.getState());
        verify(mModel, never()).set(any(), any());
    }

    @Test
    public void testDismissAlreadyHiding() {
        mModel.set(ExpandedPlayerProperties.STATE_KEY, State.HIDING);
        reset(mModel);
        mMediator.dismiss();
        assertEquals(State.HIDING, mMediator.getState());
        verify(mModel, never()).set(any(), any());

        mModel.set(ExpandedPlayerProperties.STATE_KEY, State.GONE);
        reset(mModel);
        mMediator.dismiss();
        assertEquals(State.GONE, mMediator.getState());
        verify(mModel, never()).set(any(), any());
    }

    @Test
    public void testDismiss() {
        mMediator.show();
        mMediator.dismiss();
        assertEquals(State.HIDING, mMediator.getState());
    }
}
