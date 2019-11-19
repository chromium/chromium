// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.accounts.Account;
import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.password.SavePasswordsPreferences;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.AccountManagerTestRule;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for verifying whether users are presented with the correct option of viewing
 * passwords according to the user group they belong to (syncing with sync passphrase,
 * syncing without sync passsphrase, non-syncing).
 */

@RunWith(BaseJUnit4ClassRunner.class)
public class PasswordViewingTypeTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Rule
    public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private MainPreferences mMainPreferences;
    private ChromeBasePreference mPasswordsPref;
    private Context mContext;
    private MockSyncContentResolverDelegate mSyncContentResolverDelegate;
    private String mAuthority;
    private Account mAccount;

    @Before
    public void setUp() {
        setupTestAccount();
        mSyncContentResolverDelegate = new MockSyncContentResolverDelegate();
        mContext = InstrumentationRegistry.getTargetContext();
        mMainPreferences = (MainPreferences) startMainPreferences(
                InstrumentationRegistry.getInstrumentation(), mContext)
                                   .getMainFragment();
        mPasswordsPref = (ChromeBasePreference) mMainPreferences.findPreference(
                MainPreferences.PREF_SAVED_PASSWORDS);
        AndroidSyncSettings.overrideForTests(mSyncContentResolverDelegate, null);
        mAuthority = AndroidSyncSettings.get().getContractAuthority();
        AndroidSyncSettings.get().updateAccount(mAccount);
        mActivityTestRule.loadNativeLibraryAndInitBrowserProcess();
    }

    private void setupTestAccount() {
        mAccount = AccountManagerFacade.createAccountFromName("account@example.com");
        AccountHolder.Builder accountHolder = AccountHolder.builder(mAccount).alwaysAccept(true);
        mAccountManagerTestRule.addAccount(accountHolder.build());
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> ProfileSyncService.resetForTests());
    }

    /**
     * Launches the main preferences.
     */
    private static Preferences startMainPreferences(Instrumentation instrumentation,
            final Context mContext) {
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(mContext,
                MainPreferences.class.getName());
        Activity activity = (Preferences) instrumentation.startActivitySync(intent);
        return (Preferences) activity;
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
        Assert.assertTrue(AndroidSyncSettings.get().isSyncEnabled());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(ProfileSyncService.get().isEngineInitialized());
            Assert.assertFalse(ProfileSyncService.get().isUsingSecondaryPassphrase());
        });
        Assert.assertEquals(
                SavePasswordsPreferences.class.getCanonicalName(), mPasswordsPref.getFragment());
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
                SavePasswordsPreferences.class.getCanonicalName(), mPasswordsPref.getFragment());
        Assert.assertNotNull(mMainPreferences.getActivity().getIntent());
    }

    /**
     * Verifies that non-syncing users are allowed to natively view passwords.
     */
    @Test
    @SmallTest
    public void testNonSyncingNativePasswordView() throws InterruptedException {
        setSyncability(false);
        Assert.assertEquals(
                SavePasswordsPreferences.class.getCanonicalName(), mPasswordsPref.getFragment());
    }

}
