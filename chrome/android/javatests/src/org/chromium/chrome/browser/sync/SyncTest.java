// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.LocalDataDescription;
import org.chromium.components.sync.PassphraseType;
import org.chromium.components.sync.UserSelectableType;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/** Test suite for Sync. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(crbug.com/40743432): SyncTestRule doesn't support batching.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SyncTest {
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    /** Waits until {@link SyncService#isSyncingUnencryptedUrls} returns desired value. */
    private void waitForIsSyncingUnencryptedUrls(boolean desiredValue) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mSyncTestRule.getSyncService().isSyncingUnencryptedUrls(),
                            Matchers.is(desiredValue));
                },
                SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/1197554")
    public void testSignInAndOut() {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        // Signing out should disable sync.
        mSyncTestRule.signOut();
        Assert.assertFalse(SyncTestUtil.isSyncFeatureEnabled());

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
                () ->
                        IdentityServicesProvider.get()
                                .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                                .hasPrimaryAccount(ConsentLevel.SYNC),
                "Timed out checking that hasPrimaryAccount(ConsentLevel.SYNC) == true",
                SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);

        mSyncTestRule.clearServerData();

        // Clearing server data should turn off sync and sign out of chrome.
        Assert.assertNull(mSyncTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        Assert.assertFalse(SyncTestUtil.isSyncFeatureEnabled());
        CriteriaHelper.pollUiThread(
                () ->
                        !IdentityServicesProvider.get()
                                .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                                .hasPrimaryAccount(ConsentLevel.SYNC),
                "Timed out checking that hasPrimaryAccount(ConsentLevel.SYNC) == false",
                SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testStopAndStartSync() {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        Assert.assertEquals(accountInfo, mSyncTestRule.getPrimaryAccount(ConsentLevel.SYNC));

        // Signing out should disable sync.
        mSyncTestRule.signOut();
        Assert.assertFalse(SyncTestUtil.isSyncFeatureEnabled());
        Assert.assertNull(mSyncTestRule.getPrimaryAccount(ConsentLevel.SYNC));

        accountInfo = mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        Assert.assertTrue(SyncTestUtil.isSyncFeatureEnabled());
        Assert.assertEquals(accountInfo, mSyncTestRule.getPrimaryAccount(ConsentLevel.SYNC));
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testIsSyncingUnencryptedUrlsWhileUsingKeystorePassphrase() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        // By default Sync is being setup with KEYSTORE_PASSPHRASE and all types enabled.
        CriteriaHelper.pollUiThread(
                () ->
                        mSyncTestRule.getSyncService().getPassphraseType()
                                == PassphraseType.KEYSTORE_PASSPHRASE,
                "Timed out checking getPassphraseType() == PassphraseType.KEYSTORE_PASSPHRASE",
                SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);
        waitForIsSyncingUnencryptedUrls(true);

        // isSyncingUnencryptedUrls() should return false when history is disabled.
        mSyncTestRule.disableDataType(UserSelectableType.HISTORY);
        waitForIsSyncingUnencryptedUrls(false);

        // Now enable only history datatypes and verify that isSyncingUnencryptedUrls() returns true
        // again.
        mSyncTestRule.setSelectedTypes(
                false, new HashSet<>(Arrays.asList(UserSelectableType.HISTORY)));
        waitForIsSyncingUnencryptedUrls(true);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testIsSyncingUnencryptedUrlsWhileUsingTrustedVaultPassprhase() {
        mSyncTestRule.getFakeServerHelper().setTrustedVaultNigori(new byte[] {1, 2, 3, 4});
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();

        // isSyncingUnencryptedUrls() should treat TRUSTED_VAULT_PASSPHRASE in exactly the same way
        // as KEYSTORE_PASSPHRASE.
        CriteriaHelper.pollUiThread(
                () ->
                        mSyncTestRule.getSyncService().getPassphraseType()
                                == PassphraseType.TRUSTED_VAULT_PASSPHRASE,
                "Timed out checking getPassphraseType() == PassphraseType.TRUSTED_VAULT_PASSPHRASE",
                SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);
        waitForIsSyncingUnencryptedUrls(true);

        // isSyncingUnencryptedUrls() should return false when history is disabled.
        mSyncTestRule.disableDataType(UserSelectableType.HISTORY);
        waitForIsSyncingUnencryptedUrls(false);

        // Now enable only history datatypes and verify that isSyncingUnencryptedUrls() returns true
        // again.
        mSyncTestRule.setSelectedTypes(
                false, new HashSet<>(Arrays.asList(UserSelectableType.HISTORY)));
        waitForIsSyncingUnencryptedUrls(true);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testIsSyncingUnencryptedUrlsWhileUsingCustomPassphrase() {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.encryptWithPassphrase("passphrase");
        CriteriaHelper.pollUiThread(
                () ->
                        mSyncTestRule.getSyncService().getPassphraseType()
                                == PassphraseType.CUSTOM_PASSPHRASE,
                "Timed out checking getPassphraseType() == PassphraseType.CUSTOM_PASSPHRASE",
                SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);

        // isSyncingUnencryptedUrls() should return false with CUSTOM_PASSPHRASE no matter which
        // datatypes are enabled.
        waitForIsSyncingUnencryptedUrls(false);
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testGetLocalDataDescription() throws Exception {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndSignInForTesting();
        Assert.assertEquals(accountInfo, mSyncTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));

        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSyncTestRule
                            .getSyncService()
                            .getLocalDataDescriptions(
                                    Set.of(
                                            DataType.BOOKMARKS,
                                            DataType.PASSWORDS,
                                            DataType.READING_LIST),
                                    localDataDescriptionsMap -> {
                                        int sum =
                                                localDataDescriptionsMap.values().stream()
                                                        .map(LocalDataDescription::itemCount)
                                                        .reduce(0, Integer::sum);
                                        Assert.assertEquals(0, sum);
                                        callbackHelper.notifyCalled();
                                        return;
                                    });
                });
        callbackHelper.waitForOnly();
    }
}
