// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MiniPlayerMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniPlayerMediatorUnitTest {
    private PropertyModel mModel;
    private MiniPlayerMediator mMediator;

    @Mock private BrowserControlsSizer mBrowserControlsSizer;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build();
        mMediator = new MiniPlayerMediator(mModel, mBrowserControlsSizer);
    }

    @Test
    public void testPlaceSelfInModel() {
        assertEquals(mMediator, mModel.get(PlayerProperties.MINI_PLAYER_MEDIATOR));
    }

    @Test
    public void testShow() {
        mMediator.show(/* animate= */ false);
        assertEquals(
                false,
                (boolean) mModel.get(PlayerProperties.MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES));
        assertEquals(VisibilityState.SHOWING, mMediator.getVisibility());
    }

    @Test
    public void testDismiss() {
        mMediator.dismiss(/* animate= */ false);
        assertEquals(
                false,
                (boolean) mModel.get(PlayerProperties.MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES));
        assertEquals(VisibilityState.HIDING, mMediator.getVisibility());
    }
}
