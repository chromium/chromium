// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;

/** Tests for the methods of {@link FakePasswordSettingsAccessorFactoryImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class FakePasswordSettingsAccessorFactoryImplTest {
    FakePasswordSettingsAccessorFactoryImpl mFakePasswordSettingsAccessorFactoryImpl;

    @Before
    public void setUp() {
        mFakePasswordSettingsAccessorFactoryImpl = new FakePasswordSettingsAccessorFactoryImpl();
    }

    @Test
    public void testCreateAccessor() {
        assertTrue(
                mFakePasswordSettingsAccessorFactoryImpl.createAccessor()
                        instanceof FakePasswordSettingsAccessor);
    }
}
