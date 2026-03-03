// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
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
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

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
        Assert.assertEquals(
                "Initial state: getAccounts must be empty",
                List.of(),
                mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Run test.
                    Assert.assertEquals(
                            "No account: getAccounts must be empty",
                            List.of(),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredAndNoSignedInUser() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Accounts should be available without being signed-in",
                            List.of(TestAccounts.ACCOUNT1),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
                });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListSingleAccountThenAddOne() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "One account available",
                            List.of(TestAccounts.ACCOUNT1),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
                });

        // Add another account.
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Two accounts available",
                            List.of(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
                });
    }

    @Test
    @MediumTest
    public void testAccountListNotUpdatedWhenFetchFailsAndListIsEmpty() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertEquals(
                                List.of(TestAccounts.ACCOUNT1),
                                mIdentityManager
                                        .getExtendedAccountInfoForAccountsWithRefreshToken()));

        // Simulate a transient system failure where the AccountManager returns 0 accounts.
        mSigninTestRule.setAccountFetchFailed();
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        // An empty account list should be ignored to prevent accidental sign-outs (and potential
        // data wipes).
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "IdentityManager should retain the account. An empty account list is"
                                    + " ignored when the fetch fails.",
                            List.of(TestAccounts.ACCOUNT1),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
                });
        assertNotNull(
                "primary account shoudld still be set",
                mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveSignedInOne() {
        // Add accounts.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Signed in and two accounts available",
                            List.of(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
                });

        mSigninTestRule.signOut();
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Only one account available, account1 should not be returned anymore",
                            List.of(TestAccounts.ACCOUNT2),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
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
                    Assert.assertEquals(
                            "Signed in and two accounts available",
                            List.of(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
                });

        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT2.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Only one account available, account2 should not be returned anymore",
                            List.of(TestAccounts.ACCOUNT1),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
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
                    Assert.assertEquals(
                            "Signed in and two accounts available",
                            List.of(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
                });

        // Remove all.
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT2.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "No account available",
                            List.of(),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
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
                    Assert.assertEquals(
                            "Signed in and two accounts available",
                            List.of(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
                });

        mSigninTestRule.signOut();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Two accounts available",
                            List.of(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
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
                            new HashSet<>(List.of(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2)),
                            new HashSet<>(
                                    mIdentityManager
                                            .getExtendedAccountInfoForAccountsWithRefreshToken()));
                });

        mSigninTestRule.signOut();
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT2.getId());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            "Not signed in and no accounts available",
                            List.of(),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.SIGNIN_MANAGER_SEEDING_FIX)
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
                    Assert.assertEquals(
                            List.of(),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
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
    public void testSignoutWhenAccountsNotAvailable() {
        HistogramWatcher signoutWatcher =
                HistogramWatcher.newSingleRecordWatcher("Signin.SignOut.Completed");
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        // Blocks updated the accounts list and ensures that {@link #getAccounts} returns an
        // unfulfilled promise.
        FakeAccountManagerFacade.UpdateBlocker blocker = mSigninTestRule.blockGetAccountsUpdate();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
                    Assert.assertFalse(mAccountManagerFacade.getAccounts().isFulfilled());
                    Assert.assertEquals(
                            List.of(TestAccounts.ACCOUNT1),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());

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
                    Assert.assertEquals(
                            "Accounts are available",
                            List.of(TestAccounts.ACCOUNT1),
                            mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
                });
        signoutWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testSignIn_SignInCompletedHistogramRecorded() {
        var signinHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SignIn.Completed", SigninAccessPoint.WEB_SIGNIN);

        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        signinHistogram.assertExpected(
                "Signin should be recorded with WEB_SIGNIN as the access point.");
    }
}
