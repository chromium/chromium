// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.support.test.InstrumentationRegistry;

import androidx.preference.Preference;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.ui.PassphraseDialogFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Test for ManageSyncSettings with FakeProfileSyncService.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManageSyncSettingsWithFakeProfileSyncServiceTest {

    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule() {
        @Override
        protected FakeProfileSyncService createProfileSyncService() {
            return new FakeProfileSyncService();
        }
    };
    @Rule
    public SettingsActivityTestRule<ManageSyncSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(ManageSyncSettings.class);

    private SettingsActivity mSettingsActivity;

    /**
     * Test that triggering OnPassphraseAccepted dismisses PassphraseDialogFragment.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    @DisabledTest(message = "https://crbug.com/986243")
    public void testPassphraseDialogDismissed() {
        final FakeProfileSyncService fakeProfileSyncService =
                (FakeProfileSyncService) mSyncTestRule.getProfileSyncService();

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        // Trigger PassphraseDialogFragment to be shown when taping on Encryption.
        fakeProfileSyncService.setPassphraseRequiredForPreferredDataTypes(true);

        final ManageSyncSettings fragment = startManageSyncPreferences();
        Preference encryption = fragment.findPreference(ManageSyncSettings.PREF_ENCRYPTION);
        clickPreference(encryption);

        final PassphraseDialogFragment passphraseFragment = ActivityUtils.waitForFragment(
                mSettingsActivity, ManageSyncSettings.FRAGMENT_ENTER_PASSPHRASE);
        Assert.assertTrue(passphraseFragment.isAdded());

        // Simulate OnPassphraseAccepted from external event by setting PassphraseRequired to false
        // and triggering syncStateChanged().
        // PassphraseDialogFragment should be dismissed.
        fakeProfileSyncService.setPassphraseRequiredForPreferredDataTypes(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            fragment.getFragmentManager().executePendingTransactions();
            Assert.assertNull("PassphraseDialogFragment should be dismissed.",
                    mSettingsActivity.getFragmentManager().findFragmentByTag(
                            ManageSyncSettings.FRAGMENT_ENTER_PASSPHRASE));
        });
    }

    private ManageSyncSettings startManageSyncPreferences() {
        mSettingsActivity = mSettingsActivityTestRule.startSettingsActivity();
        return mSettingsActivityTestRule.getFragment();
    }

    private void clickPreference(final Preference pref) {
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> pref.getOnPreferenceClickListener().onPreferenceClick(pref));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }
}
