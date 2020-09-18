// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static org.chromium.chrome.browser.sync.AndroidSyncSettingsTestUtils.getDoesMasterSyncAllowSyncOnUiThread;
import static org.chromium.chrome.browser.sync.AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread;
import static org.chromium.chrome.browser.sync.AndroidSyncSettingsTestUtils.getIsSyncEnabledOnUiThread;

import android.accounts.Account;
import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.sync.AndroidSyncSettings.AndroidSyncSettingsObserver;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for AndroidSyncSettings.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AndroidSyncSettingsTest {
    // Thread-safe class to fake user interaction with auto-sync settings which also counts calls
    // to certain methods.
    // When a setter is called, this results into a task being immediately posted to the thread
    // where AndroidSyncSettings lives (UI) to update its internal state. Consequently, if the
    // setter is called on the test thread, and an assertion for the expected state is then posted
    // to the UI thread, the assertion task is guaranteed to be executed after the update task. So
    // this is safe. Getters on the other hand can be called wherever, with many of them being
    // batched with other code running inside runOnUiThreadBlocking() to make the code succinct.
    private static class CountingMockSyncContentResolverDelegate
            extends MockSyncContentResolverDelegate {
        private final AtomicInteger mGetMasterSyncAutomaticallyCalls = new AtomicInteger();
        private final AtomicInteger mGetSyncAutomaticallyCalls = new AtomicInteger();
        private final AtomicInteger mGetIsSyncableCalls = new AtomicInteger();
        private final AtomicInteger mSetIsSyncableCalls = new AtomicInteger();
        private final AtomicInteger mSetSyncAutomaticallyCalls = new AtomicInteger();
        private final AtomicInteger mRemovePeriodicSyncCalls = new AtomicInteger();

        @Override
        public boolean getMasterSyncAutomatically() {
            mGetMasterSyncAutomaticallyCalls.getAndIncrement();
            return super.getMasterSyncAutomatically();
        }

        @Override
        public boolean getSyncAutomatically(Account account, String authority) {
            mGetSyncAutomaticallyCalls.getAndIncrement();
            return super.getSyncAutomatically(account, authority);
        }

        @Override
        public int getIsSyncable(Account account, String authority) {
            mGetIsSyncableCalls.getAndIncrement();
            return super.getIsSyncable(account, authority);
        }

        @Override
        public void setIsSyncable(Account account, String authority, int syncable) {
            mSetIsSyncableCalls.getAndIncrement();
            super.setIsSyncable(account, authority, syncable);
        }

        @Override
        public void setSyncAutomatically(Account account, String authority, boolean sync) {
            mSetSyncAutomaticallyCalls.getAndIncrement();
            super.setSyncAutomatically(account, authority, sync);
        }

        @Override
        public void removePeriodicSync(Account account, String authority, Bundle extras) {
            mRemovePeriodicSyncCalls.getAndIncrement();
            super.removePeriodicSync(account, authority, extras);
        }
    }

    // Should live on the UI thread as normal a AndroidSyncSettingsObserver.
    private static class MockSyncSettingsObserver implements AndroidSyncSettingsObserver {
        private boolean mReceivedNotification;

        public void clearNotification() {
            mReceivedNotification = false;
        }

        public boolean receivedNotification() {
            return mReceivedNotification;
        }

        @Override
        public void androidSyncSettingsChanged() {
            mReceivedNotification = true;
        }
    }

    // |mChromeBrowserRule| is used to wait for the native feature list to initialize.
    @Rule
    public TestRule mChromeBrowserRule = new ChromeBrowserTestRule();
    @Rule
    public TestRule mProcessorRule = new Features.JUnitProcessor();

    private AndroidSyncSettings mAndroidSyncSettings;
    private CountingMockSyncContentResolverDelegate mSyncContentResolverDelegate;
    private Account mAccount;
    private Account mAlternateAccount;
    private String mAuthority = AndroidSyncSettings.getContractAuthority();

    @Before
    public void setUp() throws Exception {
        FeatureList.setTestCanUseDefaultsForTesting();

        mSyncContentResolverDelegate = new CountingMockSyncContentResolverDelegate();

        FakeAccountManagerFacade fakeAccountManagerFacade = new FakeAccountManagerFacade(null);
        AccountManagerFacadeProvider.setInstanceForTests(fakeAccountManagerFacade);
        mAccount = AccountUtils.createAccountFromName("account@example.com");
        fakeAccountManagerFacade.addAccount(mAccount);
        mAlternateAccount = AccountUtils.createAccountFromName("alternate@example.com");
        fakeAccountManagerFacade.addAccount(mAlternateAccount);
    }

    private void createAndroidSyncSettings() throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAndroidSyncSettings = new AndroidSyncSettings(mSyncContentResolverDelegate, mAccount);
            AndroidSyncSettings.overrideForTests(mAndroidSyncSettings);
        });

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)) {
            Assert.assertTrue(
                    mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority) <= 0);
        } else {
            Assert.assertTrue(mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority) > 0);
        }
    }

    private void setMasterSyncAllowsChromeSync() {
        if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)) {
            mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
            Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
        }
        // If DecoupleSyncFromAndroidMasterSync is enabled, no need for any
        // setup since master sync doesn't influence sync.
    }

    @After
    public void tearDown() throws Exception {
        AccountManagerFacadeProvider.resetInstanceForTests();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAccountInitialization() throws TimeoutException {
        createAndroidSyncSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // mAccount was set to be syncable and not have periodic syncs.
            Assert.assertEquals(1, mSyncContentResolverDelegate.mSetIsSyncableCalls.get());
            Assert.assertEquals(1, mSyncContentResolverDelegate.mRemovePeriodicSyncCalls.get());

            mAndroidSyncSettings.updateAccount(null);

            // mAccount was set to be not syncable.
            Assert.assertEquals(2, mSyncContentResolverDelegate.mSetIsSyncableCalls.get());
            Assert.assertEquals(1, mSyncContentResolverDelegate.mRemovePeriodicSyncCalls.get());
            mAndroidSyncSettings.updateAccount(mAlternateAccount);
            // mAlternateAccount was set to be syncable and not have periodic syncs.
            Assert.assertEquals(3, mSyncContentResolverDelegate.mSetIsSyncableCalls.get());
            Assert.assertEquals(2, mSyncContentResolverDelegate.mRemovePeriodicSyncCalls.get());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testToggleMasterSyncFromSettings() throws TimeoutException {
        createAndroidSyncSettings();

        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());

        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)) {
            Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
        } else {
            Assert.assertFalse(getDoesMasterSyncAllowSyncOnUiThread());
        }
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testToggleChromeSyncFromSettings() throws TimeoutException {
        createAndroidSyncSettings();

        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);

        // First sync
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        Assert.assertTrue(getIsSyncEnabledOnUiThread());
        Assert.assertTrue(getIsChromeSyncEnabledOnUiThread());

        // Disable sync automatically for the app
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, false);
        Assert.assertFalse(getIsSyncEnabledOnUiThread());
        Assert.assertFalse(getIsChromeSyncEnabledOnUiThread());

        // Re-enable sync
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        Assert.assertTrue(getIsSyncEnabledOnUiThread());
        Assert.assertTrue(getIsChromeSyncEnabledOnUiThread());

        // Disable master sync
        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        Assert.assertTrue(getIsChromeSyncEnabledOnUiThread());
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)) {
            Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
            Assert.assertTrue(getIsSyncEnabledOnUiThread());
        } else {
            Assert.assertFalse(getDoesMasterSyncAllowSyncOnUiThread());
            Assert.assertFalse(getIsSyncEnabledOnUiThread());
        }
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testToggleAccountSyncFromApplication() throws TimeoutException {
        createAndroidSyncSettings();

        setMasterSyncAllowsChromeSync();

        TestThreadUtils.runOnUiThreadBlocking(() -> mAndroidSyncSettings.enableChromeSync());
        Assert.assertTrue(getIsSyncEnabledOnUiThread());

        TestThreadUtils.runOnUiThreadBlocking(() -> mAndroidSyncSettings.disableChromeSync());
        Assert.assertFalse(getIsSyncEnabledOnUiThread());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testToggleSyncabilityForMultipleAccounts() throws TimeoutException {
        createAndroidSyncSettings();

        setMasterSyncAllowsChromeSync();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertTrue(mAndroidSyncSettings.isSyncEnabled());

            mAndroidSyncSettings.updateAccount(mAlternateAccount);
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertTrue(mAndroidSyncSettings.isSyncEnabled());

            mAndroidSyncSettings.disableChromeSync();
            Assert.assertFalse(mAndroidSyncSettings.isSyncEnabled());
            mAndroidSyncSettings.updateAccount(mAccount);
            Assert.assertTrue(mAndroidSyncSettings.isSyncEnabled());

            // Ensure we don't erroneously re-use cached data.
            mAndroidSyncSettings.updateAccount(null);
            Assert.assertFalse(mAndroidSyncSettings.isSyncEnabled());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testSyncSettingsCaching() throws TimeoutException {
        createAndroidSyncSettings();

        setMasterSyncAllowsChromeSync();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertTrue(mAndroidSyncSettings.isSyncEnabled());

            int masterSyncAutomaticallyCalls =
                    mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get();
            int isSyncableCalls = mSyncContentResolverDelegate.mGetIsSyncableCalls.get();
            int getSyncAutomaticallyAcalls =
                    mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get();

            // Do a bunch of reads.
            mAndroidSyncSettings.doesMasterSyncSettingAllowChromeSync();
            mAndroidSyncSettings.isSyncEnabled();
            mAndroidSyncSettings.isChromeSyncEnabled();

            // Ensure values were read from cache.
            Assert.assertEquals(masterSyncAutomaticallyCalls,
                    mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get());
            Assert.assertEquals(
                    isSyncableCalls, mSyncContentResolverDelegate.mGetIsSyncableCalls.get());
            Assert.assertEquals(getSyncAutomaticallyAcalls,
                    mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get());

            // Do a bunch of reads for alternate account.
            mAndroidSyncSettings.updateAccount(mAlternateAccount);
            mAndroidSyncSettings.doesMasterSyncSettingAllowChromeSync();
            mAndroidSyncSettings.isSyncEnabled();
            mAndroidSyncSettings.isChromeSyncEnabled();

            // Ensure settings were only fetched once.
            Assert.assertEquals(masterSyncAutomaticallyCalls + 1,
                    mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get());
            Assert.assertEquals(
                    isSyncableCalls + 1, mSyncContentResolverDelegate.mGetIsSyncableCalls.get());
            Assert.assertEquals(getSyncAutomaticallyAcalls + 1,
                    mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testAndroidSyncSettingsPostsNotifications() throws TimeoutException {
        createAndroidSyncSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            MockSyncSettingsObserver syncSettingsObserver = new MockSyncSettingsObserver();
            mAndroidSyncSettings.registerObserver(syncSettingsObserver);

            syncSettingsObserver.clearNotification();
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertTrue("enableChromeSync should trigger observers",
                    syncSettingsObserver.receivedNotification());

            syncSettingsObserver.clearNotification();
            mAndroidSyncSettings.updateAccount(mAlternateAccount);
            Assert.assertTrue("switching to account with different settings should notify",
                    syncSettingsObserver.receivedNotification());

            syncSettingsObserver.clearNotification();
            mAndroidSyncSettings.updateAccount(mAccount);
            Assert.assertTrue("switching to account with different settings should notify",
                    syncSettingsObserver.receivedNotification());

            syncSettingsObserver.clearNotification();
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertFalse("enableChromeSync shouldn't trigger observers",
                    syncSettingsObserver.receivedNotification());

            syncSettingsObserver.clearNotification();
            mAndroidSyncSettings.disableChromeSync();
            Assert.assertTrue("disableChromeSync should trigger observers",
                    syncSettingsObserver.receivedNotification());

            syncSettingsObserver.clearNotification();
            mAndroidSyncSettings.disableChromeSync();
            Assert.assertFalse("disableChromeSync shouldn't observers",
                    syncSettingsObserver.receivedNotification());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testIsSyncableOnSigninAndNotOnSignout() throws TimeoutException {
        createAndroidSyncSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority) > 0);

            mAndroidSyncSettings.updateAccount(null);
            Assert.assertTrue(
                    mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority) <= 0);

            Assert.assertEquals(
                    0, mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority));
            mAndroidSyncSettings.updateAccount(mAccount);
            Assert.assertTrue(mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority) > 0);
        });
    }

    /**
     * Regression test for crbug.com/475299.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testSyncableIsAlwaysSetWhenSignedIn() throws TimeoutException {
        // Set up bad state where an account is stored as not syncable.
        mSyncContentResolverDelegate.setIsSyncable(mAccount, mAuthority, 0);
        // When the account is signed-in, AndroidSyncSettings makes sure it is set
        // to be syncable.
        createAndroidSyncSettings();
        Assert.assertTrue(mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority) > 0);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.EnableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    // TODO(crbug.com/1105795): Remove this test after DecoupleSyncFromAndroidMasterSync has
    // launched, since testToggleChromeSyncFromSettings() covers the same functionality.
    public void testSyncStateDoesNotDependOnMasterSync() throws TimeoutException {
        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        createAndroidSyncSettings();

        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        Assert.assertTrue(getIsChromeSyncEnabledOnUiThread());
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertTrue(getIsSyncEnabledOnUiThread());

        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, false);
        Assert.assertFalse(getIsChromeSyncEnabledOnUiThread());
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertFalse(getIsSyncEnabledOnUiThread());
    }
}
