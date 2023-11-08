// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
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
        mMediator = new MiniPlayerMediator(mBrowserControlsSizer);
        mModel = mMediator.getModel();
    }

    @Test
    public void testInitialModelState() {
        assertEquals(VisibilityState.GONE, mModel.get(Properties.VISIBILITY));
        assertEquals(View.GONE, mModel.get(Properties.ANDROID_VIEW_VISIBILITY));
        assertFalse(mModel.get(Properties.COMPOSITED_VIEW_VISIBLE));
        assertEquals(mMediator, mModel.get(Properties.MEDIATOR));
    }

    @Test
    public void testShow() {
        mMediator.show(/* animate= */ false);
        assertFalse(mModel.get(Properties.ANIMATE_VISIBILITY_CHANGES));
        assertEquals(VisibilityState.SHOWING, mMediator.getVisibility());
    }

    @Test
    public void testDismiss() {
        mMediator.dismiss(/* animate= */ false);
        assertFalse(mModel.get(Properties.ANIMATE_VISIBILITY_CHANGES));
        assertEquals(VisibilityState.HIDING, mMediator.getVisibility());
    }
}
