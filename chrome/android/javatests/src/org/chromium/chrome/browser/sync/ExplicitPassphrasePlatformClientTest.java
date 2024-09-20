// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.ExplicitPassphrasePlatformClient;
import org.chromium.components.sync.SyncService;

/**
 * Integration test for ExplicitPassphrasePlatformClient.
 *
 * <p>TODO(crbug.com/329409293): Test the case where the passphrase was already entered.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(crbug.com/40743432): SyncTestRule doesn't support batching.")
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    // Keep in sync with the corresponding string in sync_service_impl.cc.
    "ignore-min-gms-version-with-passphrase-support-for-test"
})
public class ExplicitPassphrasePlatformClientTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public final SyncTestRule mSyncTestRule = new SyncTestRule();

    @Mock ExplicitPassphrasePlatformClient mExplicitPassphrasePlatformClient;

    @Before
    public void setUp() {
        ServiceLoaderUtil.setInstanceForTesting(
                ExplicitPassphrasePlatformClient.class, mExplicitPassphrasePlatformClient);
    }

    @Test
    @MediumTest
    public void testInvokeIfCorrectDecryptionPassphraseSet() throws Exception {
        mSyncTestRule.getFakeServerHelper().setCustomPassphraseNigori("passphrase");
        CoreAccountInfo account = mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncService syncService = mSyncTestRule.getSyncService();
        CriteriaHelper.pollUiThread(() -> syncService.isPassphraseRequiredForPreferredDataTypes());

        ThreadUtils.runOnUiThreadBlocking(() -> syncService.setDecryptionPassphrase("passphrase"));

        verify(mExplicitPassphrasePlatformClient)
                .setExplicitDecryptionPassphrase(eq(account), notNull());
    }

    // TODO(crbug.com/329409290): Change the behavior to *not* invoke the API if the passphrase is
    // wrong.
    @Test
    @MediumTest
    public void testInvokeIfWrongDecryptionPassphraseSet() throws Exception {
        mSyncTestRule.getFakeServerHelper().setCustomPassphraseNigori("correctPassphrase");
        CoreAccountInfo account = mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncService syncService = mSyncTestRule.getSyncService();
        CriteriaHelper.pollUiThread(() -> syncService.isPassphraseRequiredForPreferredDataTypes());

        ThreadUtils.runOnUiThreadBlocking(
                () -> syncService.setDecryptionPassphrase("wrongPassphrase"));

        verify(mExplicitPassphrasePlatformClient)
                .setExplicitDecryptionPassphrase(eq(account), notNull());
    }
}
