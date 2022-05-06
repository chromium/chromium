// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

import android.os.Looper;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;

/**
 * Tests for {@link ChildAccountStatusSupplier}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class ChildAccountStatusSupplierTest {
    private static final String ADULT_ACCOUNT_EMAIL = "adult.account@gmail.com";
    private static final String CHILD_ACCOUNT_EMAIL =
            AccountManagerTestRule.generateChildEmail(/*baseName=*/"account@gmail.com");

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private ChildAccountStatusSupplier mSupplier;

    @Before
    public void setUp() {
        mSupplier = new ChildAccountStatusSupplier();
    }

    @After
    public void tearDown() {
        UmaRecorderHolder.resetForTesting();
        ShadowRecordHistogram.reset();
    }

    @Test
    public void testUnsetByDefault() {
        // Supplier shouldn't be set and should not record any histograms until it can obtain the
        // list of accounts from AccountManagerFacade.
        assertNull(mSupplier.get());
        assertEquals(0,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.ChildAccountStatusDuration"));
    }

    @Test
    public void testNoAccounts() {
        mSupplier.startFetchingChildAccountStatus();
        shadowOf(Looper.getMainLooper()).idle();

        assertFalse(mSupplier.get());
        assertEquals(1,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.ChildAccountStatusDuration"));
    }

    @Test
    public void testOneChildAccount() {
        mAccountManagerTestRule.addAccount(CHILD_ACCOUNT_EMAIL);

        mSupplier.startFetchingChildAccountStatus();
        shadowOf(Looper.getMainLooper()).idle();

        assertTrue(mSupplier.get());
        assertEquals(1,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.ChildAccountStatusDuration"));
    }

    @Test
    public void testNonChildAccount() {
        mAccountManagerTestRule.addAccount(ADULT_ACCOUNT_EMAIL);

        mSupplier.startFetchingChildAccountStatus();
        shadowOf(Looper.getMainLooper()).idle();

        assertFalse(mSupplier.get());
        assertEquals(1,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.ChildAccountStatusDuration"));
    }

    @Test
    public void testOneChildAccountWithNonChildAccounts() {
        mAccountManagerTestRule.addAccount(CHILD_ACCOUNT_EMAIL);
        mAccountManagerTestRule.addAccount(ADULT_ACCOUNT_EMAIL);

        mSupplier.startFetchingChildAccountStatus();
        shadowOf(Looper.getMainLooper()).idle();

        assertTrue(mSupplier.get());
        assertEquals(1,
                ShadowRecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.ChildAccountStatusDuration"));
    }
}
