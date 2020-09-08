// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.accounts.Account;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.AndroidSyncSettings;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for verifying whether users are presented with the correct option of viewing
 * passwords according to the user group they belong to (syncing with sync passphrase,
 * syncing without sync passsphrase, non-syncing).
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PasswordViewingTypeTest {
    private final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private final SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    // We need to destroy the SettingsActivity before tearing down the mock sign-in environment
    // setup in ChromeBrowserTestRule to avoid code crash.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mChromeBrowserTestRule).around(mSettingsActivityTestRule);

    private ChromeBasePreference mPasswordsPref;
    private MockSyncContentResolverDelegate mSyncContentResolverDelegate;
    private String mAuthority;
    private Account mAccount;

    @Before
    public void setUp() {
        mAccount = mChromeBrowserTestRule.addAccount("account@example.com");
        mSyncContentResolverDelegate = new MockSyncContentResolverDelegate();
        mSettingsActivityTestRule.startSettingsActivity();
        MainSettings mainSettings = mSettingsActivityTestRule.getFragment();
        mPasswordsPref =
                (ChromeBasePreference) mainSettings.findPreference(MainSettings.PREF_PASSWORDS);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AndroidSyncSettings.overrideForTests(
                    new AndroidSyncSettings(mSyncContentResolverDelegate));
            mAuthority = AndroidSyncSettings.getContractAuthority();
            AndroidSyncSettings.get().updateAccount(mAccount);
        });
    }

    /**
     * Override ProfileSyncService using FakeProfileSyncService.
     */
    private void overrideProfileSyncService(final boolean usingPassphrase) {
        class FakeProfileSyncService extends ProfileSyncService {
            @Override
            public boolean isUsingSecondaryPassphrase() {
                return usingPassphrase;
            }

            @Override
            public boolean isEngineInitialized() {
                return true;
            }
        }
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ProfileSyncService.overrideForTests(new FakeProfileSyncService()); });
    }

    /**
     * Turn syncability on/off.
     */
    private void setSyncability(boolean syncState) throws InterruptedException {
        // Turn on syncability
        mSyncContentResolverDelegate.setMasterSyncAutomatically(syncState);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        // First sync
        mSyncContentResolverDelegate.setIsSyncable(mAccount, mAuthority, (syncState) ? 1 : 0);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        if (syncState) {
            mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, syncState);
            mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        }
    }

    /**
     * Verifies that sync settings are being set up correctly in the case of redirecting users.
     * Checks that sync users are allowed to view passwords natively.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testUserRedirectSyncSettings() throws InterruptedException {
        setSyncability(true);
        overrideProfileSyncService(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(AndroidSyncSettings.get().isSyncEnabled());
            Assert.assertTrue(ProfileSyncService.get().isEngineInitialized());
            Assert.assertFalse(ProfileSyncService.get().isUsingSecondaryPassphrase());
        });
        Assert.assertEquals(
                PasswordSettings.class.getCanonicalName(), mPasswordsPref.getFragment());
    }

    /**
     * Verifies that syncing users with a custom passphrase are allowed to
     * natively view passwords.
     */
    @Test
    @SmallTest
    public void testSyncingNativePasswordView() throws InterruptedException {
        setSyncability(true);
        overrideProfileSyncService(true);
        Assert.assertEquals(
                PasswordSettings.class.getCanonicalName(), mPasswordsPref.getFragment());
        Assert.assertNotNull(mSettingsActivityTestRule.getActivity().getIntent());
    }

    /**
     * Verifies that non-syncing users are allowed to natively view passwords.
     */
    @Test
    @SmallTest
    public void testNonSyncingNativePasswordView() throws InterruptedException {
        setSyncability(false);
        Assert.assertEquals(
                PasswordSettings.class.getCanonicalName(), mPasswordsPref.getFragment());
    }
}
