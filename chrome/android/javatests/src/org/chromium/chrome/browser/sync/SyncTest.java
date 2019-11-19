// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.app.Activity;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.MockChangeEventChecker;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.AccountIdProvider;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Test suite for Sync.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String TAG = "SyncTest";

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testFlushDirectoryDoesntBreakSync() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        final Activity activity = mSyncTestRule.getActivity();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                ApplicationStatus.onStateChangeForTesting(activity, ActivityState.PAUSED);
            }
        });

        // TODO(pvalenzuela): When available, check that sync is still functional.
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testSignInAndOut() throws InterruptedException {
        Account account = mSyncTestRule.setUpTestAccountAndSignIn();

        // Signing out should disable sync.
        mSyncTestRule.signOut();
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        // Signing back in should re-enable sync.
        mSyncTestRule.signinAndEnableSync(account);
        SyncTestUtil.waitForSyncActive();
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testStopAndClear() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        CriteriaHelper.pollUiThread(
                new Criteria("Timed out checking that hasPrimaryAccount() == true") {
                    @Override
                    public boolean isSatisfied() {
                        return IdentityServicesProvider.getIdentityManager().hasPrimaryAccount();
                    }
                },
                SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);

        mSyncTestRule.clearServerData();

        // Clearing server data should turn off sync and sign out of chrome.
        Assert.assertNull(SigninTestUtil.getCurrentAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        CriteriaHelper.pollUiThread(
                new Criteria("Timed out checking that hasPrimaryAccount() == false") {
                    @Override
                    public boolean isSatisfied() {
                        return !IdentityServicesProvider.getIdentityManager().hasPrimaryAccount();
                    }
                },
                SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
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
        final Account oldAccount = mSyncTestRule.setUpTestAccountAndSignIn();
        final Account newAccount = SigninTestUtil.addTestAccount("test2@gmail.com");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // First, we force a call to updateAccountRenameData. In the real world,
            // this should be called by one of our broadcast listener that listens to
            // real account rename events instead of the mocks.
            MockChangeEventChecker eventChecker = new MockChangeEventChecker();
            eventChecker.insertRenameEvent(oldAccount.name, newAccount.name);
            SigninHelper.updateAccountRenameData(eventChecker);

            // Tell the fake content resolver that a rename had happen and copy over the sync
            // settings. This would normally be done by the
            // SystemSyncTestRule.getSyncContentResolver().
            mSyncTestRule.getSyncContentResolver().renameAccounts(
                    oldAccount, newAccount, AndroidSyncSettings.get().getContractAuthority());

            // Inform the AccountTracker, these would normally be done by account validation
            // or signin. We will only be calling the testing versions of it.
            AccountIdProvider provider = AccountIdProvider.getInstance();
            String[] accountNames = {oldAccount.name, newAccount.name};
            String[] accountIds = {
                    provider.getAccountId(accountNames[0]), provider.getAccountId(accountNames[1])};
            IdentityServicesProvider.getAccountTrackerService().syncForceRefreshForTest(
                    accountIds, accountNames);

            // Starts the rename process. Normally, this is triggered by the broadcast
            // listener as well.
            SigninHelper.get().validateAccountSettings(true);
        });

        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return newAccount.equals(SigninTestUtil.getCurrentAccount());
            }
        });
        SyncTestUtil.waitForSyncActive();
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testStopAndStartSync() {
        Account account = mSyncTestRule.setUpTestAccountAndSignIn();

        mSyncTestRule.stopSync();
        Assert.assertEquals(account, SigninTestUtil.getCurrentAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        mSyncTestRule.startSyncAndWait();
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testStopAndStartSyncThroughAndroidChromeSync() {
        Account account = mSyncTestRule.setUpTestAccountAndSignIn();
        String authority = AndroidSyncSettings.get().getContractAuthority();

        Assert.assertTrue(AndroidSyncSettings.get().isSyncEnabled());
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
    @Feature({"Sync"})
    public void testStopAndStartSyncThroughAndroidMasterSync() {
        mSyncTestRule.setUpTestAccountAndSignIn();

        Assert.assertTrue(AndroidSyncSettings.get().isSyncEnabled());
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
    public void testReenableMasterSyncFirst() {
        Account account = mSyncTestRule.setUpTestAccountAndSignIn();
        String authority = AndroidSyncSettings.get().getContractAuthority();

        Assert.assertTrue(AndroidSyncSettings.get().isSyncEnabled());
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
    @Feature({"Sync"})
    public void testReenableChromeSyncFirst() {
        Account account = mSyncTestRule.setUpTestAccountAndSignIn();
        String authority = AndroidSyncSettings.get().getContractAuthority();

        Assert.assertTrue(AndroidSyncSettings.get().isSyncEnabled());
        Assert.assertTrue(SyncTestUtil.isSyncRequested());
        Assert.assertTrue(SyncTestUtil.canSyncFeatureStart());

        // Disabling master sync first. Sync should be off.
        mSyncTestRule.getSyncContentResolver().setMasterSyncAutomatically(false);
        // TODO(crbug.com/921025): Master sync shouldn't influence isSyncRequested, so at this point
        // isSyncRequested should be true; only canSyncFeatureStart should be false.
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        Assert.assertFalse(SyncTestUtil.canSyncFeatureStart());

        // Also disable Chrome sync.
        mSyncTestRule.getSyncContentResolver().setSyncAutomatically(account, authority, false);
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        Assert.assertFalse(SyncTestUtil.canSyncFeatureStart());

        // Re-enabling Chrome sync should not turn sync back on.
        mSyncTestRule.getSyncContentResolver().setSyncAutomatically(account, authority, true);
        // TODO(crbug.com/921025): Master sync (which is still disabled) shouldn't influence
        // isSyncRequested, so at this point isSyncRequested should be true (but
        // canSyncFeatureStart should remain false).
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        Assert.assertFalse(SyncTestUtil.canSyncFeatureStart());

        // But then re-enabling master sync should.
        mSyncTestRule.getSyncContentResolver().setMasterSyncAutomatically(true);
        Assert.assertTrue(SyncTestUtil.canSyncFeatureStart());
        SyncTestUtil.waitForSyncActive();
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testMasterSyncBlocksSyncStart() {
        mSyncTestRule.setUpTestAccountAndSignIn();
        mSyncTestRule.stopSync();
        Assert.assertFalse(SyncTestUtil.isSyncRequested());

        mSyncTestRule.getSyncContentResolver().setMasterSyncAutomatically(false);
        mSyncTestRule.startSync();
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
    }
}
