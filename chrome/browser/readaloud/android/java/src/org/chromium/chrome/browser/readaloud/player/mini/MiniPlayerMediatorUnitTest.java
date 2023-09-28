// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.VisibilityState;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MiniPlayerMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniPlayerMediatorUnitTest {
    private PropertyModel mModel;
    private MiniPlayerMediator mMediator;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build();
        mMediator = new MiniPlayerMediator(mModel);
    }

    @Test
    public void testShow() {
        mMediator.show(/*animate=*/false);
        assertEquals(false,
                (boolean) mModel.get(PlayerProperties.MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES));
        assertEquals(VisibilityState.VISIBLE, mMediator.getVisibility());
    }

    @Test
    public void testDismiss() {
        mMediator.dismiss(/*animate=*/false);
        assertEquals(false,
                (boolean) mModel.get(PlayerProperties.MINI_PLAYER_ANIMATE_VISIBILITY_CHANGES));
        assertEquals(VisibilityState.GONE, mMediator.getVisibility());
    }
}
