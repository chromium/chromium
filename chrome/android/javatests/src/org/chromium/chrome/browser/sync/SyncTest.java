// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;

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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;

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
                                   .hasPrimaryAccount(),
                "Timed out checking that hasPrimaryAccount() == true", SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);

        mSyncTestRule.clearServerData();

        // Clearing server data should turn off sync and sign out of chrome.
        Assert.assertNull(mSyncTestRule.getCurrentSignedInAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        CriteriaHelper.pollUiThread(
                ()
                        -> !IdentityServicesProvider.get()
                                    .getIdentityManager(Profile.getLastUsedRegularProfile())
                                    .hasPrimaryAccount(),
                "Timed out checking that hasPrimaryAccount() == false", SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testStopAndStartSync() {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        mSyncTestRule.stopSync();
        Assert.assertEquals(accountInfo, mSyncTestRule.getCurrentSignedInAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        mSyncTestRule.startSyncAndWait();
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testStopAndStartSyncThroughAndroidChromeSync() {
        Account account = CoreAccountInfo.getAndroidAccountFrom(
                mSyncTestRule.setUpAccountAndEnableSyncForTesting());
        String authority = AndroidSyncSettings.getContractAuthority();

        Assert.assertTrue(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
        Assert.assertTrue(AndroidSyncSettingsTestUtils.getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertTrue(SyncTestUtil.isSyncRequested());

        // Disabling Android sync should turn Chrome sync engine off.
        mSyncTestRule.getSyncContentResolver().setSyncAutomatically(account, authority, false);
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        // Enabling Android sync should turn Chrome sync engine on.
        mSyncTestRule.getSyncContentResolver().setSyncAutomatically(account, authority, true);
        SyncTestUtil.waitForSyncFeatureActive();
    }

    @Test
    @LargeTest
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    @Feature({"Sync"})
    @DisabledTest(message = "crbug.com/1103515")
    public void testStopAndStartSyncThroughAndroidMasterSync() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        Assert.assertTrue(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
        Assert.assertTrue(AndroidSyncSettingsTestUtils.getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertTrue(SyncTestUtil.isSyncRequested());

        // Disabling Android's master sync should turn Chrome sync engine off.
        mSyncTestRule.getSyncContentResolver().setMasterSyncAutomatically(false);
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        // Enabling Android's master sync should turn Chrome sync engine on.
        mSyncTestRule.getSyncContentResolver().setMasterSyncAutomatically(true);
        SyncTestUtil.waitForSyncFeatureActive();
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    @DisabledTest(message = "Test is flaky crbug.com/1100890")
    public void testReenableMasterSyncFirst() {
        Account account = CoreAccountInfo.getAndroidAccountFrom(
                mSyncTestRule.setUpAccountAndEnableSyncForTesting());
        String authority = AndroidSyncSettings.getContractAuthority();

        Assert.assertTrue(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
        Assert.assertTrue(AndroidSyncSettingsTestUtils.getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertTrue(SyncTestUtil.isSyncRequested());
        Assert.assertTrue(SyncTestUtil.canSyncFeatureStart());

        // Disable Chrome sync first. Sync should be off.
        mSyncTestRule.getSyncContentResolver().setSyncAutomatically(account, authority, false);
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        Assert.assertFalse(SyncTestUtil.canSyncFeatureStart());

        // Also disable master sync.
        mSyncTestRule.getSyncContentResolver().setMasterSyncAutomatically(false);
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        Assert.assertFalse(SyncTestUtil.canSyncFeatureStart());

        // Re-enabling master sync should not turn sync back on.
        mSyncTestRule.getSyncContentResolver().setMasterSyncAutomatically(true);
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        Assert.assertFalse(SyncTestUtil.canSyncFeatureStart());

        // But then re-enabling Chrome sync should.
        mSyncTestRule.getSyncContentResolver().setSyncAutomatically(account, authority, true);
        Assert.assertTrue(SyncTestUtil.canSyncFeatureStart());
        SyncTestUtil.waitForSyncFeatureActive();
    }

    @Test
    @LargeTest
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    @Feature({"Sync"})
    public void testReenableChromeSyncFirst() {
        Account account = CoreAccountInfo.getAndroidAccountFrom(
                mSyncTestRule.setUpAccountAndEnableSyncForTesting());
        String authority = AndroidSyncSettings.getContractAuthority();

        Assert.assertTrue(AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread());
        Assert.assertTrue(AndroidSyncSettingsTestUtils.getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertTrue(SyncTestUtil.isSyncRequested());
        Assert.assertTrue(SyncTestUtil.canSyncFeatureStart());

        // Disabling master sync first. Sync should be off.
        mSyncTestRule.getSyncContentResolver().setMasterSyncAutomatically(false);
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        Assert.assertFalse(SyncTestUtil.canSyncFeatureStart());

        // Also disable Chrome sync.
        mSyncTestRule.getSyncContentResolver().setSyncAutomatically(account, authority, false);
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        Assert.assertFalse(SyncTestUtil.canSyncFeatureStart());

        // Re-enabling Chrome sync should not turn sync back on.
        mSyncTestRule.getSyncContentResolver().setSyncAutomatically(account, authority, true);
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        Assert.assertFalse(SyncTestUtil.canSyncFeatureStart());

        // But then re-enabling master sync should.
        mSyncTestRule.getSyncContentResolver().setMasterSyncAutomatically(true);
        Assert.assertTrue(SyncTestUtil.canSyncFeatureStart());
        SyncTestUtil.waitForSyncFeatureActive();
    }

    @Test
    @LargeTest
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    @Feature({"Sync"})
    public void testMasterSyncBlocksSyncStart() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        mSyncTestRule.stopSync();
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        mSyncTestRule.getSyncContentResolver().setMasterSyncAutomatically(false);
        mSyncTestRule.startSync();
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
    }
}
