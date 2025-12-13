// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import android.os.Build;

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
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features;
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
import org.chromium.components.signin.identitymanager.IdentityManagerImpl;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
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
@Features.EnableFeatures(SigninFeatures.MAKE_ACCOUNTS_AVAILABLE_IN_IDENTITY_MANAGER)
public class SigninManagerIntegrationTest {
    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private IdentityManagerImpl mIdentityManager;
    private AccountManagerFacade mAccountManagerFacade;
    private SigninManager mSigninManager;

    @Mock private SigninManager.SignInStateObserver mSignInStateObserverMock;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    mIdentityManager =
                            (IdentityManagerImpl)
                                    IdentityServicesProvider.get().getIdentityManager(profile);
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
    @Features.DisableFeatures(SigninFeatures.MAKE_ACCOUNTS_AVAILABLE_IN_IDENTITY_MANAGER)
    public void testUpdateAccountListOneAccountsRegisteredAndNoSignedInUserLegacy() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

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
    public void testUpdateAccountListOneAccountsRegisteredAndNoSignedInUser() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Accounts should be available without being signed-in",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(SigninFeatures.MAKE_ACCOUNTS_AVAILABLE_IN_IDENTITY_MANAGER)
    public void testUpdateAccountListOneAccountsRegisteredSignedIn() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Signed in: one account should be available",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListSingleAccountThenAddOne() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "One account available",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });

        // Add another account.
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Two accounts available",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(SigninFeatures.MAKE_ACCOUNTS_AVAILABLE_IN_IDENTITY_MANAGER)
    public void testUpdateAccountListSingleSignedInAccountThenAddOne() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Signed in and one account available",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });

        // Add another account.
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Signed in and two accounts available",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveSignedInOne() {
        // Add accounts.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Signed in and two accounts available",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });

        mSigninTestRule.signOut();
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Only one account available, account1 should not be returned anymore",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT2},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveNonSignedInOne() {
        // Add accounts.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Signed in and two accounts available",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });

        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT2.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Only one account available, account2 should not be returned anymore",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveAll() {
        // Add accounts.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Signed in and two accounts available",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });

        // Remove all.
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT2.getId());

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
    public void testUpdateAccountListTwoAccountsThenSignOut() {
        // Add accounts.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Signed in and two accounts available",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });

        mSigninTestRule.signOut();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Two accounts available",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_equals = Build.VERSION_CODES.S_V2, message = "crbug.com/41486307")
    public void testUpdateAccountListTwoAccountsThenRemoveAllSignOut() {
        // Add accounts.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Signed in and two accounts available",
                            new HashSet<>(
                                    Arrays.asList(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2)),
                            new HashSet<>(
                                    Arrays.asList(
                                            mIdentityManager.getAccountsWithRefreshTokens())));
                });

        mSigninTestRule.signOut();
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT2.getId());

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
    public void testPrimaryAccountRemoval_signsOut() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        SigninTestUtil.signin(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail(),
                            TestAccounts.ACCOUNT1.getEmail());
                });

        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNull(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN));
                    assertNull(
                            SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail());
                });
    }

    @Test
    @MediumTest
    public void testSignInAndSignOut_updatesLegacyPrimaryAccountEmail() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail(),
                            TestAccounts.ACCOUNT1.getEmail());
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
    public void testPrimaryAccountRenaming_updatesLegacyPrimaryAccountEmail() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            SigninPreferencesManager.getInstance().getLegacyPrimaryAccountEmail(),
                            TestAccounts.ACCOUNT1.getEmail());
                });

        AccountInfo renamedAccount =
                new AccountInfo.Builder("renamed@gmail.com", TestAccounts.ACCOUNT1.getGaiaId())
                        .build();
        mSigninTestRule.updateAccount(renamedAccount);

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
    public void testClearPrimaryAccount_signsOut() {
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
    public void testSignIn_waitForPrefCommit() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        SigninTestUtil.signinAndWaitForPrefsCommit(TestAccounts.ACCOUNT1);

        Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
        verify(mSignInStateObserverMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onSignedIn();
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(SigninFeatures.MAKE_ACCOUNTS_AVAILABLE_IN_IDENTITY_MANAGER)
    public void testSignoutWhenAccountsNotAvailableLegacy() {
        HistogramWatcher signoutWatcher =
                HistogramWatcher.newSingleRecordWatcher("Signin.SignOut.Completed");
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        // Blocks updated the accounts list and ensures that AccountManagerFacade#getAccounts()
        // returns an unfulfilled promise.
        FakeAccountManagerFacade.UpdateBlocker blocker =
                mSigninTestRule.blockGetAccountsUpdate(/* populateCache= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
                    Assert.assertFalse(mAccountManagerFacade.getAccounts().isFulfilled());
                    Assert.assertEquals(
                            List.of(TestAccounts.ACCOUNT1),
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
        // Check that the account is still signed out and that it has been removed from the
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

    @Test
    @MediumTest
    public void testSignoutWhenAccountsNotAvailable() {
        HistogramWatcher signoutWatcher =
                HistogramWatcher.newSingleRecordWatcher("Signin.SignOut.Completed");
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        // Blocks updated the accounts list and ensures that {@link #getAccounts} returns an
        // unfulfilled promise.
        FakeAccountManagerFacade.UpdateBlocker blocker =
                mSigninTestRule.blockGetAccountsUpdate(/* populateCache= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
                    Assert.assertFalse(mAccountManagerFacade.getAccounts().isFulfilled());
                    Assert.assertArrayEquals(
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1},
                            mIdentityManager.getAccountsWithRefreshTokens());

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
        // Check that the account is still signed out but the account is available in identity
        // manager.
        Assert.assertFalse(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertArrayEquals(
                            "Accounts are available",
                            new CoreAccountInfo[] {TestAccounts.ACCOUNT1},
                            mIdentityManager.getAccountsWithRefreshTokens());
                });
        signoutWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testSignIn_SignInCompletedHistogramRecorded() {
        var signinHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SignIn.Completed", SigninAccessPoint.UNKNOWN);

        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        signinHistogram.assertExpected(
                "Signin should be recorded with unknown as the access point.");
    }
}
