// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;

/** Tests for {@link PasswordManagerBackendSupportHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class FakePasswordManagerBackendSupportHelperTest {
    private FakePasswordManagerBackendSupportHelper mFakeHelper;

    @Before
    public void setUp() {
        mFakeHelper = new FakePasswordManagerBackendSupportHelper();
    }

    @Test
    public void testBackendNotPresent() {
        assertFalse(mFakeHelper.isBackendPresent());
    }

    @Test
    public void testUpdateNotNeeded() {
        assertFalse(mFakeHelper.isUpdateNeeded());
    }

    @Test
    public void testSetBackendPresentToTrue() {
        mFakeHelper.setBackendPresent(true);
        assertTrue(mFakeHelper.isBackendPresent());
    }

    @Test
    public void testSetUpdateNeededToTrue() {
        mFakeHelper.setUpdateNeeded(true);
        assertTrue(mFakeHelper.isUpdateNeeded());
    }
}
