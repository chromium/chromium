// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;

/**
 * Test suite for Sync.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();
    @Rule
    public TestRule mProcessorRule = new Features.JUnitProcessor();

    private static final String TAG = "SyncTest";

    @DisabledTest(message = "https://crbug.com/1197554")
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testSignInAndOut() {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        // Signing out should disable sync.
        mSyncTestRule.signOut();
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        // Signing back in should re-enable sync.
        mSyncTestRule.signinAndEnableSync(accountInfo);
        Assert.assertTrue("Sync should be re-enabled.", SyncTestUtil.isSyncFeatureActive());
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testStopAndClear() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        CriteriaHelper.pollUiThread(
                ()
                        -> IdentityServicesProvider.get()
                                   .getIdentityManager(Profile.getLastUsedRegularProfile())
                                   .hasPrimaryAccount(ConsentLevel.SYNC),
                "Timed out checking that hasPrimaryAccount(ConsentLevel.SYNC) == true",
                SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);

        mSyncTestRule.clearServerData();

        // Clearing server data should turn off sync and sign out of chrome.
        Assert.assertNull(mSyncTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        CriteriaHelper.pollUiThread(
                ()
                        -> !IdentityServicesProvider.get()
                                    .getIdentityManager(Profile.getLastUsedRegularProfile())
                                    .hasPrimaryAccount(ConsentLevel.SYNC),
                "Timed out checking that hasPrimaryAccount(ConsentLevel.SYNC) == false",
                SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testStopAndStartSync() {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        mSyncTestRule.stopSync();
        Assert.assertEquals(accountInfo, mSyncTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        mSyncTestRule.startSyncAndWait();
    }
}
