// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import static org.junit.Assert.assertNotNull;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;

/**
 * Tests for {@link CreatorMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CreatorMediatorTest {
    private Activity mActivity;
    private CreatorMediator mCreatorMediator;

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private CreatorApiBridge.Natives mBridgeJniMock;

    @Before
    public void setUpTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mCreatorMediator = new CreatorMediator(mActivity);

        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(CreatorApiBridgeJni.TEST_HOOKS, mBridgeJniMock);
    }

    @Test
    public void testCreatorMediatorConstruction() {
        assertNotNull("Could not construct CreatorMediator", mCreatorMediator);
    }
}
