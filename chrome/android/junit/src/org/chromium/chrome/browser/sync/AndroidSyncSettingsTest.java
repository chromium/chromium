// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.sync.AndroidSyncSettingsTestUtils.getDoesMasterSyncAllowSyncOnUiThread;
import static org.chromium.chrome.browser.sync.AndroidSyncSettingsTestUtils.getIsChromeSyncEnabledOnUiThread;

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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * Tests for AndroidSyncSettings.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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

    // Should live on the UI thread as a normal AndroidSyncSettings.Delegate.
    private static class MockSyncSettingsDelegate implements AndroidSyncSettings.Delegate {
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

    @Rule
    public TestRule mProcessorRule = new Features.JUnitProcessor();
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private AndroidSyncSettings mAndroidSyncSettings;
    private CountingMockSyncContentResolverDelegate mSyncContentResolverDelegate;
    @Mock
    private ProfileSyncService mProfileSyncService;
    private Account mAccount;
    private Account mAlternateAccount;
    private String mAuthority = AndroidSyncSettings.getContractAuthority();

    @Before
    public void setUp() {
        mSyncContentResolverDelegate = new CountingMockSyncContentResolverDelegate();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SyncContentResolverDelegate.overrideForTests(mSyncContentResolverDelegate);
            ProfileSyncService.overrideForTests(mProfileSyncService);
        });

        FakeAccountManagerFacade fakeAccountManagerFacade = new FakeAccountManagerFacade(null);
        AccountManagerFacadeProvider.setInstanceForTests(fakeAccountManagerFacade);
        mAccount = AccountUtils.createAccountFromName("account@example.com");
        fakeAccountManagerFacade.addAccount(mAccount);
        mAlternateAccount = AccountUtils.createAccountFromName("alternate@example.com");
        fakeAccountManagerFacade.addAccount(mAlternateAccount);
    }

    private void createAndroidSyncSettings() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // TODO(crbug.com/1105795): Consider setting the fake account in the identity manager
            // so there's no need to inject it here.
            mAndroidSyncSettings = new AndroidSyncSettings(mAccount);
            AndroidSyncSettings.overrideForTests(mAndroidSyncSettings);
        });

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)) {
            Assert.assertTrue(
                    mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority) <= 0);
        } else {
            Assert.assertTrue(mSyncContentResolverDelegate.getIsSyncable(mAccount, mAuthority) > 0);
        }
    }

    @After
    public void tearDown() {
        AccountManagerFacadeProvider.resetInstanceForTests();
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testAccountInitialization() {
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
    @Features.EnableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testToggleMasterSyncFromSettingsWithDecouplingEnabled() {
        createAndroidSyncSettings();

        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());

        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testToggleMasterSyncFromSettingsWithDecouplingDisabled() {
        createAndroidSyncSettings();

        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());

        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        Assert.assertFalse(getDoesMasterSyncAllowSyncOnUiThread());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.EnableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testToggleChromeSyncFromSettingsWithDecouplingEnabled() {
        createAndroidSyncSettings();

        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);

        // Enable sync for Chrome app.
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertTrue(getIsChromeSyncEnabledOnUiThread());

        // Disable sync for Chrome app.
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, false);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertFalse(getIsChromeSyncEnabledOnUiThread());

        // Re-enable sync for Chrome app.
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertTrue(getIsChromeSyncEnabledOnUiThread());

        // Disable master sync (all apps).
        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertTrue(getIsChromeSyncEnabledOnUiThread());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testToggleChromeSyncFromSettingsWithDecouplingDisabled() {
        createAndroidSyncSettings();

        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);

        // Enable sync for Chrome app.
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertTrue(getIsChromeSyncEnabledOnUiThread());

        // Disable sync for Chrome app.
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, false);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertFalse(getIsChromeSyncEnabledOnUiThread());

        // Re-enable sync for Chrome app.
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertTrue(getIsChromeSyncEnabledOnUiThread());

        // Disable master sync (all apps).
        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        Assert.assertFalse(getDoesMasterSyncAllowSyncOnUiThread());
        Assert.assertTrue(getIsChromeSyncEnabledOnUiThread());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.EnableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testToggleAccountSyncFromApplicationWithDecouplingEnabled() {
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);

        createAndroidSyncSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> mAndroidSyncSettings.enableChromeSync());
        Assert.assertTrue(getIsChromeSyncEnabledOnUiThread());

        TestThreadUtils.runOnUiThreadBlocking(() -> mAndroidSyncSettings.disableChromeSync());
        Assert.assertFalse(getIsChromeSyncEnabledOnUiThread());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testToggleAccountSyncFromApplicationWithDecouplingDisabled() {
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);

        createAndroidSyncSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> mAndroidSyncSettings.enableChromeSync());
        Assert.assertTrue(getIsChromeSyncEnabledOnUiThread());

        TestThreadUtils.runOnUiThreadBlocking(() -> mAndroidSyncSettings.disableChromeSync());
        Assert.assertFalse(getIsChromeSyncEnabledOnUiThread());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.EnableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testToggleSyncabilityForMultipleAccountsWithDecouplingEnabled() {
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);

        createAndroidSyncSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertTrue(mAndroidSyncSettings.isChromeSyncEnabled());

            mAndroidSyncSettings.updateAccount(mAlternateAccount);
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertTrue(mAndroidSyncSettings.isChromeSyncEnabled());

            mAndroidSyncSettings.disableChromeSync();
            Assert.assertFalse(mAndroidSyncSettings.isChromeSyncEnabled());
            mAndroidSyncSettings.updateAccount(mAccount);
            Assert.assertTrue(mAndroidSyncSettings.isChromeSyncEnabled());

            // Ensure we don't erroneously re-use cached data.
            mAndroidSyncSettings.updateAccount(null);
            Assert.assertFalse(mAndroidSyncSettings.isChromeSyncEnabled());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testToggleSyncabilityForMultipleAccountsWithDecouplingDisabled() {
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);

        createAndroidSyncSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertTrue(mAndroidSyncSettings.isChromeSyncEnabled());

            mAndroidSyncSettings.updateAccount(mAlternateAccount);
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertTrue(mAndroidSyncSettings.isChromeSyncEnabled());

            mAndroidSyncSettings.disableChromeSync();
            Assert.assertFalse(mAndroidSyncSettings.isChromeSyncEnabled());
            mAndroidSyncSettings.updateAccount(mAccount);
            Assert.assertTrue(mAndroidSyncSettings.isChromeSyncEnabled());

            // Ensure we don't erroneously re-use cached data.
            mAndroidSyncSettings.updateAccount(null);
            Assert.assertFalse(mAndroidSyncSettings.isChromeSyncEnabled());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.EnableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testSyncSettingsCachingWithDecouplingEnabled() {
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);

        createAndroidSyncSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertTrue(mAndroidSyncSettings.isChromeSyncEnabled());

            int masterSyncAutomaticallyCalls =
                    mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get();
            int isSyncableCalls = mSyncContentResolverDelegate.mGetIsSyncableCalls.get();
            int getSyncAutomaticallyCalls =
                    mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get();

            // Do a bunch of reads.
            mAndroidSyncSettings.doesMasterSyncSettingAllowChromeSync();
            mAndroidSyncSettings.isChromeSyncEnabled();

            // Ensure values were read from cache.
            Assert.assertEquals(masterSyncAutomaticallyCalls,
                    mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get());
            Assert.assertEquals(
                    isSyncableCalls, mSyncContentResolverDelegate.mGetIsSyncableCalls.get());
            Assert.assertEquals(getSyncAutomaticallyCalls,
                    mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get());

            // Do a bunch of reads for alternate account.
            mAndroidSyncSettings.updateAccount(mAlternateAccount);
            mAndroidSyncSettings.doesMasterSyncSettingAllowChromeSync();
            mAndroidSyncSettings.isChromeSyncEnabled();

            // Ensure settings were only fetched once.
            Assert.assertEquals(masterSyncAutomaticallyCalls + 1,
                    mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get());
            Assert.assertEquals(
                    isSyncableCalls + 1, mSyncContentResolverDelegate.mGetIsSyncableCalls.get());
            Assert.assertEquals(getSyncAutomaticallyCalls + 1,
                    mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testSyncSettingsCachingWithDecouplingDisabled() {
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);

        createAndroidSyncSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertTrue(mAndroidSyncSettings.isChromeSyncEnabled());

            int masterSyncAutomaticallyCalls =
                    mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get();
            int isSyncableCalls = mSyncContentResolverDelegate.mGetIsSyncableCalls.get();
            int getSyncAutomaticallyCalls =
                    mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get();

            // Do a bunch of reads.
            mAndroidSyncSettings.doesMasterSyncSettingAllowChromeSync();
            mAndroidSyncSettings.isChromeSyncEnabled();

            // Ensure values were read from cache.
            Assert.assertEquals(masterSyncAutomaticallyCalls,
                    mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get());
            Assert.assertEquals(
                    isSyncableCalls, mSyncContentResolverDelegate.mGetIsSyncableCalls.get());
            Assert.assertEquals(getSyncAutomaticallyCalls,
                    mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get());

            // Do a bunch of reads for alternate account.
            mAndroidSyncSettings.updateAccount(mAlternateAccount);
            mAndroidSyncSettings.doesMasterSyncSettingAllowChromeSync();
            mAndroidSyncSettings.isChromeSyncEnabled();

            // Ensure settings were only fetched once.
            Assert.assertEquals(masterSyncAutomaticallyCalls + 1,
                    mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls.get());
            Assert.assertEquals(
                    isSyncableCalls + 1, mSyncContentResolverDelegate.mGetIsSyncableCalls.get());
            Assert.assertEquals(getSyncAutomaticallyCalls + 1,
                    mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls.get());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.EnableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testAndroidSyncSettingsNotifiesDelegateWithDecouplingEnabled() {
        createAndroidSyncSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            MockSyncSettingsDelegate syncSettingsDelegate = new MockSyncSettingsDelegate();
            mAndroidSyncSettings.setDelegate(syncSettingsDelegate);

            syncSettingsDelegate.clearNotification();
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertTrue("enableChromeSync should notify delegate",
                    syncSettingsDelegate.receivedNotification());

            syncSettingsDelegate.clearNotification();
            mAndroidSyncSettings.updateAccount(mAlternateAccount);
            Assert.assertTrue("switching to account with different settings should notify",
                    syncSettingsDelegate.receivedNotification());

            syncSettingsDelegate.clearNotification();
            mAndroidSyncSettings.updateAccount(mAccount);
            Assert.assertTrue("switching to account with different settings should notify",
                    syncSettingsDelegate.receivedNotification());

            syncSettingsDelegate.clearNotification();
            mAndroidSyncSettings.enableChromeSync();
            // enableChromeSync() was a no-op because sync was already enabled for this account. So
            // there shouldn't be a notification.
            Assert.assertFalse("enableChromeSync shouldn't notify delegate",
                    syncSettingsDelegate.receivedNotification());

            syncSettingsDelegate.clearNotification();
            mAndroidSyncSettings.disableChromeSync();
            Assert.assertTrue("disableChromeSync should notify delegate",
                    syncSettingsDelegate.receivedNotification());

            syncSettingsDelegate.clearNotification();
            mAndroidSyncSettings.disableChromeSync();
            // disableChromeSync() was a no-op because sync was already disabled. So there shouldn't
            // be a notification.
            Assert.assertFalse("disableChromeSync shouldn't notify delegate",
                    syncSettingsDelegate.receivedNotification());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testAndroidSyncSettingsNotifiesDelegateWithDecouplingDisabled() {
        createAndroidSyncSettings();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            MockSyncSettingsDelegate syncSettingsDelegate = new MockSyncSettingsDelegate();
            mAndroidSyncSettings.setDelegate(syncSettingsDelegate);

            syncSettingsDelegate.clearNotification();
            mAndroidSyncSettings.enableChromeSync();
            Assert.assertTrue("enableChromeSync should notify delegate",
                    syncSettingsDelegate.receivedNotification());

            syncSettingsDelegate.clearNotification();
            mAndroidSyncSettings.updateAccount(mAlternateAccount);
            Assert.assertTrue("switching to account with different settings should notify",
                    syncSettingsDelegate.receivedNotification());

            syncSettingsDelegate.clearNotification();
            mAndroidSyncSettings.updateAccount(mAccount);
            Assert.assertTrue("switching to account with different settings should notify",
                    syncSettingsDelegate.receivedNotification());

            syncSettingsDelegate.clearNotification();
            mAndroidSyncSettings.enableChromeSync();
            // enableChromeSync() was a no-op because sync was already enabled for this account.
            // So there shouldn't be a notification.
            Assert.assertFalse("enableChromeSync shouldn't notify delegate",
                    syncSettingsDelegate.receivedNotification());

            syncSettingsDelegate.clearNotification();
            mAndroidSyncSettings.disableChromeSync();
            Assert.assertTrue("disableChromeSync should notify delegate",
                    syncSettingsDelegate.receivedNotification());

            syncSettingsDelegate.clearNotification();
            mAndroidSyncSettings.disableChromeSync();

            // disableChromeSync() was a no-op because sync was already disabled. So there shouldn't
            // be a notification.
            Assert.assertFalse("disableChromeSync shouldn't notify delegate",
                    syncSettingsDelegate.receivedNotification());
        });
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testIsSyncableOnSigninAndNotOnSignout() {
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
    public void testSyncableIsAlwaysSetWhenSignedIn() {
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
    public void testMasterSyncAllowsSyncIfReEnabledOnce() {
        // Master sync is disabled on startup and the user hasn't gone through the decoupling flow
        // in the past.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(mProfileSyncService.getDecoupledFromAndroidMasterSync()).thenReturn(false);
        });

        // Sync must remain disallowed as long as master sync is not re-enabled.
        createAndroidSyncSettings();
        Assert.assertFalse(getDoesMasterSyncAllowSyncOnUiThread());

        // Re-enable master sync at least once. Sync is now allowed and decoupled from master
        // sync.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> doNothing().when(mProfileSyncService).setDecoupledFromAndroidMasterSync());
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> verify(mProfileSyncService).setDecoupledFromAndroidMasterSync());
        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.EnableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testSyncStateRespectsPersistedDecouplingStateIfFeatureEnabled() {
        // Master sync is disabled on startup, but the user has gone through the decoupling flow in
        // the past and has the DecoupleSyncFromAndroidMasterSync feature enabled.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(mProfileSyncService.getDecoupledFromAndroidMasterSync()).thenReturn(true);
        });

        // Chrome becomes aware of the previous decoupling, so Sync can be enabled despite master
        // sync being disabled.
        createAndroidSyncSettings();
        Assert.assertTrue(getDoesMasterSyncAllowSyncOnUiThread());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    @Features.DisableFeatures(ChromeFeatureList.DECOUPLE_SYNC_FROM_ANDROID_MASTER_SYNC)
    public void testSyncStateDoesNotRespectPersistedDecouplingStateIfFeatureDisabled() {
        // The user went through the master sync decoupling flow in the past, but has the
        // DecoupleSyncFromAndroidMasterSync feature disabled.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(mProfileSyncService.getDecoupledFromAndroidMasterSync()).thenReturn(true);
        });

        // Chrome should ignore that a previous decoupling happened. Sync should again respect
        // master sync, so it's not allowed.
        createAndroidSyncSettings();
        Assert.assertFalse(getDoesMasterSyncAllowSyncOnUiThread());
    }
}
