// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninHelperProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.MockChangeEventChecker;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

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
    @DisabledTest(message = "crbug.com/1144221")
    public void testSignInAndOut() {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        // Signing out should disable sync.
        mSyncTestRule.signOut();
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        // Signing back in should re-enable sync.
        mSyncTestRule.signinAndEnableSync(accountInfo);
        Assert.assertTrue("Sync should be re-enabled.", SyncTestUtil.isSyncActive());
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

    /*
     * @FlakyTest
     * @LargeTest
     * @Feature({"Sync"})
     */
    @Test
    @DisabledTest(message = "crbug.com/588050,crbug.com/595893")
    public void testRename() {
        // The two accounts object that would represent the account rename.
        final Account oldAccount = CoreAccountInfo.getAndroidAccountFrom(
                mSyncTestRule.setUpAccountAndEnableSyncForTesting());
        final Account newAccount =
                CoreAccountInfo.getAndroidAccountFrom(mSyncTestRule.addAccount("test2@gmail.com"));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // First, we force a call to updateAccountRenameData. In the real world,
            // this should be called by one of our broadcast listener that listens to
            // real account rename events instead of the mocks.
            MockChangeEventChecker eventChecker = new MockChangeEventChecker();
            eventChecker.insertRenameEvent(oldAccount.name, newAccount.name);
            SigninHelper.updateAccountRenameData(eventChecker, oldAccount.name);

            // Tell the fake content resolver that a rename had happen and copy over the sync
            // settings.
            MockSyncContentResolverDelegate contentResolver =
                    mSyncTestRule.getSyncContentResolver();
            String authority = AndroidSyncSettings.getContractAuthority();
            int oldIsSyncable = contentResolver.getIsSyncable(oldAccount, authority);
            contentResolver.setIsSyncable(newAccount, authority, oldIsSyncable);
            if (oldIsSyncable > 0) {
                contentResolver.setSyncAutomatically(newAccount, authority,
                        contentResolver.getSyncAutomatically(oldAccount, authority));
            }

            // Starts the rename process. Normally, this is triggered by the broadcast
            // listener as well.
            SigninHelperProvider.get().validateAccountSettings(true);
        });

        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(mSyncTestRule.getCurrentSignedInAccount().getEmail(),
                    Matchers.is(newAccount.name));
        });
        SyncTestUtil.waitForSyncActive();
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
        SyncTestUtil.waitForSyncActive();
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
        SyncTestUtil.waitForSyncActive();
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
        SyncTestUtil.waitForSyncActive();
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
        SyncTestUtil.waitForSyncActive();
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
