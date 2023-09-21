// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.readaloud.player;

import static org.junit.Assert.assertNotNull;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link PlayerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlayerCoordinatorUnitTest {
    private PlayerCoordinator mPlayerCoordinator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mPlayerCoordinator = new PlayerCoordinator(ApplicationProvider.getApplicationContext());
        mModel = mPlayerCoordinator.getModelForTesting();
    }

    @Test
    public void testInitialModelState() {
        assertNotNull(mModel.get(PlayerProperties.INTERACTION_HANDLER));
    }
}
