// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/**
 * Integration test for the IdentityManager.
 *
 * <p>These tests initialize the native part of the service.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@DoNotBatch(reason = "Integration test suite that changes the list of accounts")
public class SigninManagerIntegrationTest {
    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private IdentityManager mIdentityManager;
    private AccountManagerFacade mAccountManagerFacade;
    private SigninManager mSigninManager;

    @Mock private SigninManager.SignInStateObserver mSignInStateObserverMock;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    mIdentityManager = IdentityServicesProvider.get().getIdentityManager(profile);
                    mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();
                    mSigninManager = IdentityServicesProvider.get().getSigninManager(profile);
                    mSigninManager.addSignInStateObserver(mSignInStateObserverMock);
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListNoAccountsRegisteredAndNoSignedInUser() {
        Assert.assertArrayEquals(
                "Initial state: getAccounts must be empty",
                new CoreAccountInfo[] {},
                mIdentityManager.getAccountsWithRefreshTokens());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Run test.
                    Assert.assertArrayEquals(
                            "No account: getAccounts must be empty",
                            new CoreAccountInfo[] {},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredAndNoSignedInUser() {
        mSigninTestRule.addAccount(SigninTestRule.TEST_ACCOUNT_1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "No signed in account: getAccounts must be empty",
                            new CoreAccountInfo[] {},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredSignedIn() {
        mSigninTestRule.addAccountThenSignin(SigninTestRule.TEST_ACCOUNT_1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Signed in: one account should be available",
                            new CoreAccountInfo[] {SigninTestRule.TEST_ACCOUNT_1},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListSingleAccountThenAddOne() {
        mSigninTestRule.addAccountThenSignin(SigninTestRule.TEST_ACCOUNT_1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Signed in and one account available",
                            new CoreAccountInfo[] {SigninTestRule.TEST_ACCOUNT_1},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });

        // Add another account.
        mSigninTestRule.addAccount(SigninTestRule.TEST_ACCOUNT_2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Signed in and two accounts available",
                            new HashSet<>(
                                    Arrays.asList(
                                            SigninTestRule.TEST_ACCOUNT_1,
                                            SigninTestRule.TEST_ACCOUNT_2)),
                            new HashSet<>(
                                    Arrays.asList(
                                            mIdentityManager.getAccountsWithRefreshTokens())));
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveOne() {
        // Add accounts.
        mSigninTestRule.addAccountThenSignin(SigninTestRule.TEST_ACCOUNT_1);
        mSigninTestRule.addAccount(SigninTestRule.TEST_ACCOUNT_2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Signed in and two accounts available",
                            new HashSet<>(
                                    Arrays.asList(
                                            SigninTestRule.TEST_ACCOUNT_1,
                                            SigninTestRule.TEST_ACCOUNT_2)),
                            new HashSet<>(
                                    Arrays.asList(
                                            mIdentityManager.getAccountsWithRefreshTokens())));
                });

        mSigninTestRule.removeAccount(SigninTestRule.TEST_ACCOUNT_2.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Only one account available, account2 should not be returned anymore",
                            new CoreAccountInfo[] {SigninTestRule.TEST_ACCOUNT_1},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveAll() {
        // Add accounts.
        mSigninTestRule.addAccountThenSignin(SigninTestRule.TEST_ACCOUNT_1);
        mSigninTestRule.addAccount(SigninTestRule.TEST_ACCOUNT_2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Signed in and two accounts available",
                            new HashSet<>(
                                    Arrays.asList(
                                            SigninTestRule.TEST_ACCOUNT_1,
                                            SigninTestRule.TEST_ACCOUNT_2)),
                            new HashSet<>(
                                    Arrays.asList(
                                            mIdentityManager.getAccountsWithRefreshTokens())));
                });

        // Remove all.
        mSigninTestRule.removeAccount(SigninTestRule.TEST_ACCOUNT_1.getId());
        mSigninTestRule.removeAccount(SigninTestRule.TEST_ACCOUNT_2.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "No account available",
                            new CoreAccountInfo[] {},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveAllSignOut() {
        // Add accounts.
        mSigninTestRule.addAccountThenSignin(SigninTestRule.TEST_ACCOUNT_1);
        mSigninTestRule.addAccount(SigninTestRule.TEST_ACCOUNT_2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Signed in and two accounts available",
                            new HashSet<>(
                                    Arrays.asList(
                                            SigninTestRule.TEST_ACCOUNT_1,
                                            SigninTestRule.TEST_ACCOUNT_2)),
                            new HashSet<>(
                                    Arrays.asList(
                                            mIdentityManager.getAccountsWithRefreshTokens())));
                });

        mSigninTestRule.signOut();
        mSigninTestRule.removeAccount(SigninTestRule.TEST_ACCOUNT_1.getId());
        mSigninTestRule.removeAccount(SigninTestRule.TEST_ACCOUNT_2.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Not signed in and no accounts available",
                            new CoreAccountInfo[] {},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsRegisteredAndOneSignedIn() {
        // Add accounts.
        mSigninTestRule.addAccountThenSignin(SigninTestRule.TEST_ACCOUNT_1);
        mSigninTestRule.addAccount(SigninTestRule.TEST_ACCOUNT_2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Signed in and two accounts available",
                            new HashSet<>(
                                    Arrays.asList(
                                            SigninTestRule.TEST_ACCOUNT_1,
                                            SigninTestRule.TEST_ACCOUNT_2)),
                            new HashSet<>(
                                    Arrays.asList(
                                            mIdentityManager.getAccountsWithRefreshTokens())));
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.USE_CONSENT_LEVEL_SIGNIN_FOR_LEGACY_ACCOUNT_EMAIL_PREF)
    public void testPrimaryAccountRemoval_signsOut() {
        mSigninTestRule.addAccount(SigninTestRule.TEST_ACCOUNT_1);
        SigninTestUtil.signinAndEnableSync(SigninTestRule.TEST_ACCOUNT_1, /* syncService= */ null);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail(),
                            SigninTestRule.TEST_ACCOUNT_1.getEmail());
                });

        mSigninTestRule.removeAccount(SigninTestRule.TEST_ACCOUNT_1.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNull(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN));
                    assertNull(
                            SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail());
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.USE_CONSENT_LEVEL_SIGNIN_FOR_LEGACY_ACCOUNT_EMAIL_PREF)
    public void testPrimaryAccountRenaming_updatesLegacySyncAccountEmail() {
        mSigninTestRule.addAccount(SigninTestRule.TEST_ACCOUNT_1);
        SigninTestUtil.signinAndEnableSync(SigninTestRule.TEST_ACCOUNT_1, /* syncService= */ null);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail(),
                            SigninTestRule.TEST_ACCOUNT_1.getEmail());
                });

        AccountInfo renamedAccount =
                new AccountInfo.Builder(
                                "renamed@gmail.com", SigninTestRule.TEST_ACCOUNT_1.getGaiaId())
                        .build();
        try (var ignored = mSigninTestRule.blockGetCoreAccountInfosUpdate(true)) {
            mSigninTestRule.removeAccount(SigninTestRule.TEST_ACCOUNT_1.getId());
            mSigninTestRule.addAccount(renamedAccount);
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SYNC).getEmail(),
                            renamedAccount.getEmail());
                    assertEquals(
                            SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail(),
                            renamedAccount.getEmail());
                });
    }

    @Test
    @MediumTest
    @EnableFeatures({SigninFeatures.USE_CONSENT_LEVEL_SIGNIN_FOR_LEGACY_ACCOUNT_EMAIL_PREF})
    public void testSignInAndSignOut_updateLegacySyncAccountEmail() {
        mSigninTestRule.addAccountThenSignin(SigninTestRule.TEST_ACCOUNT_1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail(),
                            SigninTestRule.TEST_ACCOUNT_1.getEmail());
                });

        mSigninTestRule.signOut();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNull(
                            SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail());
                });
    }

    @Test
    @MediumTest
    @EnableFeatures({SigninFeatures.USE_CONSENT_LEVEL_SIGNIN_FOR_LEGACY_ACCOUNT_EMAIL_PREF})
    public void testPrimaryAccountRenaming_updatesLegacySyncAccountEmail_whenSignedIn() {
        mSigninTestRule.addAccountThenSignin(SigninTestRule.TEST_ACCOUNT_1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail(),
                            SigninTestRule.TEST_ACCOUNT_1.getEmail());
                });

        AccountInfo renamedAccount =
                new AccountInfo.Builder(
                                "renamed@gmail.com", SigninTestRule.TEST_ACCOUNT_1.getGaiaId())
                        .build();

        try (var ignored = mSigninTestRule.blockGetCoreAccountInfosUpdate(true)) {
            mSigninTestRule.removeAccount(SigninTestRule.TEST_ACCOUNT_1.getId());
            mSigninTestRule.addAccount(renamedAccount);
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN).getEmail(),
                            renamedAccount.getEmail());
                    assertEquals(
                            SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail(),
                            renamedAccount.getEmail());
                });
    }

    @Test
    @MediumTest
    public void testClearPrimaryAccountWithSyncNotEnabled_signsOut() {
        // Add accounts.
        mSigninTestRule.addTestAccountThenSignin();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));

                    // Run test.
                    mSigninManager.signOut(SignoutReason.TEST);

                    // Check the account is signed out
                    Assert.assertFalse(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
                });

        // Wait for the operation to have completed.
        verify(mSignInStateObserverMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onSignedOut();
    }

    @Test
    @MediumTest
    public void testClearPrimaryAccountWithSyncEnabled_signsOut() {
        // Add accounts.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC));

                    // Run test.
                    mSigninManager.signOut(SignoutReason.TEST);

                    Assert.assertFalse(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
                });

        // Wait for the operation to have completed - the revokeSyncConsent processing calls back
        // SigninManager, and if we don't wait for this to complete before test teardown then we
        // can hit a race condition where this async processing overlaps with the signout causing
        // teardown to fail.
        verify(mSignInStateObserverMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onSignedOut();
    }

    @Test
    @MediumTest
    public void testRevokeSyncConsent_disablesSync() {
        // Add account.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC));

                    // Run test.
                    mSigninManager.revokeSyncConsent(SignoutReason.TEST, null, false);

                    Assert.assertFalse(mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC));
                    Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
                });

        // Wait for the operation to have completed - the revokeSyncConsent processing calls back
        // SigninManager, and if we don't wait for this to complete before test teardown then we
        // can hit a race condition where this async processing overlaps with the signout causing
        // teardown to fail.
        verify(mSignInStateObserverMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onSignedOut();
    }

    @Test
    @MediumTest
    public void testSignInWithoutSync_waitForPrefCommit() {
        mSigninTestRule.addAccount(SigninTestRule.TEST_ACCOUNT_1);
        SigninTestUtil.signinAndWaitForPrefsCommit(SigninTestRule.TEST_ACCOUNT_1);

        Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
        verify(mSignInStateObserverMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onSignedIn();
    }

    @Test
    @MediumTest
    public void testSignoutWhenAccountsNotAvailable() {
        HistogramWatcher signoutWatcher =
                HistogramWatcher.newSingleRecordWatcher("Signin.SignOut.Completed");
        mSigninTestRule.addAccountThenSignin(SigninTestRule.TEST_ACCOUNT_1);
        // Blocks updated the accounts list and ensures that {@link #getCoreAccountInfos} returns an
        // unfulfilled promise.
        FakeAccountManagerFacade.UpdateBlocker blocker =
                mSigninTestRule.blockGetCoreAccountInfosUpdate(/* populateCache= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
                    Assert.assertFalse(mAccountManagerFacade.getCoreAccountInfos().isFulfilled());
                    Assert.assertEquals(
                            List.of(SigninTestRule.TEST_ACCOUNT_1),
                            Arrays.asList(mIdentityManager.getAccountsWithRefreshTokens()));

                    // Sign-out should be allowed even if the list of accounts isn't available yet.
                    mSigninManager.signOut(SignoutReason.TEST);

                    // Check the account is signed out
                    Assert.assertFalse(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
                });

        // Wait for the operation to have completed.
        verify(mSignInStateObserverMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onSignedOut();

        // Unblocks the updates.
        blocker.close();
        // Check that the account is still signed out and that is has been removed from the
        // IdentityManager.
        Assert.assertFalse(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "No accounts available",
                            new CoreAccountInfo[] {},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
        signoutWatcher.assertExpected();
    }
}
