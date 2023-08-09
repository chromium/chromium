// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.miniplayer;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link MiniPlayerMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MiniPlayerMediatorUnitTest {
    private PropertyModel mModel;
    private MiniPlayerMediator mMediator;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(MiniPlayerProperties.ALL_KEYS)
                         .with(MiniPlayerProperties.VIEW_VISIBILITY_KEY, View.GONE)
                         .build();

        mMediator = new MiniPlayerMediator(mModel);
    }

    @Test
    public void testInitialStateAfterConstructMediator() {
        assertEquals(View.GONE, (int) mModel.get(MiniPlayerProperties.VIEW_VISIBILITY_KEY));
        assertNotNull(mModel.get(MiniPlayerProperties.ON_CLOSE_CLICK_KEY));
    }

    @Test
    public void testShow() {
        mMediator.show();
        assertEquals(View.VISIBLE, (int) mModel.get(MiniPlayerProperties.VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testDismiss() {
        mMediator.show();
        mMediator.dismiss();
        assertEquals(View.GONE, (int) mModel.get(MiniPlayerProperties.VIEW_VISIBILITY_KEY));
    }

    @Test
    public void testOnCloseClick() {
        mMediator.show();
        mModel.get(MiniPlayerProperties.ON_CLOSE_CLICK_KEY).onClick(null);
        assertEquals(View.GONE, (int) mModel.get(MiniPlayerProperties.VIEW_VISIBILITY_KEY));
    }
}
