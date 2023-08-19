// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.miniplayer;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MiniPlayerMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniPlayerMediatorUnitTest {
    @Mock
    private MiniPlayerCoordinator.Observer mObserver;

    private PropertyModel mModel;
    private MiniPlayerMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = new PropertyModel.Builder(MiniPlayerProperties.ALL_KEYS)
                         .with(MiniPlayerProperties.VIEW_VISIBILITY_KEY, View.GONE)
                         .build();

        mMediator = new MiniPlayerMediator(mModel, mObserver);
    }

    @Test
    public void testInitialStateAfterConstructMediator() {
        assertEquals(View.GONE, (int) mModel.get(MiniPlayerProperties.VIEW_VISIBILITY_KEY));
        assertNotNull(mModel.get(MiniPlayerProperties.ON_CLOSE_CLICK_KEY));
    }

    @Test
    public void testShow() {
        mMediator.show(/*animate=*/false, /*playback=*/null);
        assertEquals(View.VISIBLE, (int) mModel.get(MiniPlayerProperties.VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testDismiss() {
        mMediator.show(/*animate=*/false, /*playback=*/null);
        mMediator.dismiss(/*animate=*/false);
        assertEquals(View.GONE, (int) mModel.get(MiniPlayerProperties.VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testOnCloseClick() {
        mMediator.show(/*animate=*/false, /*playback=*/null);
        mModel.get(MiniPlayerProperties.ON_CLOSE_CLICK_KEY).onClick(null);
        verify(mObserver, times(1)).onCloseClicked();
    }

    @Test
    public void testOnExpandClick() {
        mMediator.show(/*animate=*/false, /*playback=*/null);
        mModel.get(MiniPlayerProperties.ON_EXPAND_CLICK_KEY).onClick(null);
        verify(mObserver, times(1)).onExpandRequested();
    }
}
