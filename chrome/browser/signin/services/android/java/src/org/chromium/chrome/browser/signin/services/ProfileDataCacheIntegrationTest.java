// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.hamcrest.Matchers.hasItem;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountCapabilitiesConstants;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Integration tests for {@link ProfileDataCache}.
 *
 * <p>The goal is to verify real-world behavior of {@link ProfileDataCache} with real
 * implementations of {@link IdentityManager}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@DoNotBatch(reason = "Integration test suite that changes the list of accounts")
public class ProfileDataCacheIntegrationTest {
    /** Counts the calls of each {@link ProfileDataCache.Observer} method. */
    private static class CountingObserver implements ProfileDataCache.Observer {
        private int mAccountsUpdatedCount;
        private int mProfileDataUpdatedCount;

        @Override
        public void onAccountsUpdated(List<DisplayableProfileData> accounts) {
            mAccountsUpdatedCount++;
        }

        @Override
        public void onProfileDataUpdated(DisplayableProfileData profileData) {
            mProfileDataUpdatedCount++;
        }
    }

    private static final int LARGE_ACCOUNTS_LIST_SIZE = 10;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Mock private ExternalAuthUtils mExternalAuthUtils;

    private final List<AccountInfo> mLargeAccountsList =
            createAccountsList(LARGE_ACCOUNTS_LIST_SIZE);
    private final CountingObserver mObserver = new CountingObserver();

    private IdentityManager mIdentityManager;
    private ProfileDataCache mProfileDataCache;

    @Before
    public void setUp() {
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);
        when(mExternalAuthUtils.isGooglePlayServicesMissing(any())).thenReturn(false);
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    mIdentityManager = IdentityServicesProvider.get().getIdentityManager(profile);
                    IdentityServicesProvider.get().getSigninManager(profile);
                });
    }

    @After
    public void tearDown() {
        if (mProfileDataCache != null) {
            ThreadUtils.runOnUiThreadBlocking(() -> mProfileDataCache.removeObserver(mObserver));
        }
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void testNoChangesAfterInitializationShouldHaveNoObserverCalls_withAmfSourceOfTruth() {
        addAccounts(mLargeAccountsList);

        createProfileDataCache();

        assertAccountsUpdatedCount(0);
        assertProfileDataUpdatedCount(0);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void
            testNoChangesAfterInitializationShouldHaveNoObserverCalls_withIdentityManagerSourceOfTruth() {
        addAccounts(mLargeAccountsList);

        createProfileDataCache();

        assertAccountsUpdatedCount(0);
        assertProfileDataUpdatedCount(0);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void
            testAddFirstAccountShouldNotifyAboutAccountsListAndNewlyAddedAccountChange_withAmfSourceOfTruth() {
        createProfileDataCache();

        final AccountInfo account = mLargeAccountsList.get(0);
        mSigninTestRule.addAccount(account);
        waitForCachedAccounts(List.of(account));

        // Adding an account triggers an onAccountsUpdated(), which caused a cache rebuild and
        // trigger onAccountsUpdated() and onProfileDataUpdated() for that account.
        // AccountTrackerService triggers two onExtendedAccountInfoUpdated() for each added account
        // - one for the account info, another for the avatar image.
        assertAccountsUpdatedCount(1);
        assertProfileDataUpdatedCount(3);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void
            testAddFirstAccountShouldNotifyAboutAccountsListAndNewlyAddedAccountChange_withIdentityManagerSourceOfTruth() {
        createProfileDataCache();

        final AccountInfo account = mLargeAccountsList.get(0);
        mSigninTestRule.addAccount(account);
        waitForCachedAccounts(List.of(account));

        // Adding an account triggers an onRefreshTokenUpdatedForAccount(), which caused a cache
        // rebuild and trigger onAccountsUpdated() and onProfileDataUpdated() for that account.
        // AccountTrackerService triggers two onExtendedAccountInfoUpdated() for each added account
        // - one for the account info, another for the avatar image.
        assertAccountsUpdatedCount(1);
        assertProfileDataUpdatedCount(3);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void
            testAddNextAccountShouldNotifyAboutAccountsListAndNewlyAddedAccountChange_withAmfSourceOfTruth() {
        addAccounts(mLargeAccountsList);
        createProfileDataCache();

        final AccountInfo newAccount = createAccount(LARGE_ACCOUNTS_LIST_SIZE);
        mSigninTestRule.addAccount(newAccount);
        final List<AccountInfo> expectedAccounts = new ArrayList<>(mLargeAccountsList);
        expectedAccounts.add(newAccount);
        waitForCachedAccounts(expectedAccounts);

        // ProfileDataCache triggers an onProfileDataUpdated() for each account as the
        // AccountsChangeObserver#onAccountsUpdated callback does not contain information which
        // account has been changed.
        // LARGE_ACCOUNTS_LIST_SIZE + 1 is triggered by the cache rebuild caused by
        // onAccountsUpdated.
        // +2 is triggered by the onExtendedAccountInfoUpdated, caused by the double notification
        // triggered by AccountTrackerService. One notification for the newly added account, second
        // for the account's avatar image.
        assertAccountsUpdatedCount(1);
        // TODO(crbug.com/569949003): Limit number of onProfileDataUpdated callbacks
        assertProfileDataUpdatedCount((LARGE_ACCOUNTS_LIST_SIZE + 1) + 2);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void
            testUpdateExistingAccountShouldNotifyAboutAccountsListAndUpdatedAccountChange_withAmfSourceOfTruth() {
        addAccounts(mLargeAccountsList);
        createProfileDataCache();

        final AccountInfo account = mLargeAccountsList.get(0);
        final String newFullName = "Updated Full Name";
        mSigninTestRule.updateAccount(
                new AccountInfo.Builder(account).fullName(newFullName).build());
        CriteriaHelper.pollUiThread(
                () -> newFullName.equals(mProfileDataCache.getById(account.getId()).getFullName()),
                "The cache wasn't updated with the new full name");

        // ProfileDataCache triggers an onProfileDataUpdated() for each account as the
        // AccountsChangeObserver#onAccountsUpdated callback does not contain information which
        // account has been changed.
        assertAccountsUpdatedCount(1);
        assertProfileDataUpdatedCount(LARGE_ACCOUNTS_LIST_SIZE + 1);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void
            testExtendedAccountInfoUpdateShouldNotifyOnlyAboutUpdatedAccount_withAmfSourceOfTruth() {
        addAccounts(mLargeAccountsList);
        createProfileDataCache();

        final AccountInfo account = mLargeAccountsList.get(0);
        final CoreAccountId accountId = account.getId();
        final AccountInfo updatedAccount =
                new AccountInfo.Builder(account).fullName("updated full name").build();

        // The fetched account info is delivered directly to the observer - AccountTrackerService
        // can't be fed from Java without going through AccountManagerFacade.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileDataCache.onExtendedAccountInfoUpdated(updatedAccount);
                    assertEquals(
                            updatedAccount.getFullName(),
                            mProfileDataCache.getById(accountId).getFullName());
                });

        assertAccountsUpdatedCount(0);
        assertProfileDataUpdatedCount(1);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void
            testExtendedAccountInfoUpdateShouldNotifyOnlyAboutUpdatedAccount_withIdentityManagerSourceOfTruth() {
        addAccounts(mLargeAccountsList);
        createProfileDataCache();

        final AccountInfo account = mLargeAccountsList.get(0);
        final CoreAccountId accountId = account.getId();
        final AccountInfo updatedAccount =
                new AccountInfo.Builder(account).fullName("updated full name").build();

        // The fetched account info is delivered directly to the observer - AccountTrackerService
        // can't be fed from Java without going through AccountManagerFacade.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileDataCache.onExtendedAccountInfoUpdated(updatedAccount);
                    assertEquals(
                            updatedAccount.getFullName(),
                            mProfileDataCache.getById(accountId).getFullName());
                });

        assertAccountsUpdatedCount(0);
        assertProfileDataUpdatedCount(1);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void
            testRemoveAccountShouldNotifyAboutAccountsListAndRemovedAccountChange_withAmfSourceOfTruth() {
        addAccounts(mLargeAccountsList);
        createProfileDataCache();

        mSigninTestRule.removeAccount(mLargeAccountsList.get(LARGE_ACCOUNTS_LIST_SIZE - 1).getId());
        waitForCachedAccounts(mLargeAccountsList.subList(0, LARGE_ACCOUNTS_LIST_SIZE - 1));

        assertAccountsUpdatedCount(1);
        assertProfileDataUpdatedCount(LARGE_ACCOUNTS_LIST_SIZE - 1);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void testSigninShouldNotNotifyAboutAccountsListOrSignedinAccount_withAmfSourceOfTruth() {
        addAccounts(mLargeAccountsList);
        createProfileDataCache();

        var primaryAccount = mLargeAccountsList.get(0);
        mSigninTestRule.signin(primaryAccount);

        assertAccountsUpdatedCount(0);
        assertProfileDataUpdatedCount(0);
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void
            testSigninShouldNotNotifyAboutAccountsListOrSignedinAccount_withIdentityManagerSourceOfTruth() {
        addAccounts(mLargeAccountsList);
        createProfileDataCache();

        var primaryAccount = mLargeAccountsList.get(0);
        mSigninTestRule.signin(primaryAccount);

        assertAccountsUpdatedCount(0);
        assertProfileDataUpdatedCount(0);
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
    public void
            testSignoutShouldNotNotifyAboutAccountsListOrSignedOutAccount_withAmfSourceOfTruth() {
        addAccounts(mLargeAccountsList);
        mSigninTestRule.signin(mLargeAccountsList.get(0));

        createProfileDataCache();
        mSigninTestRule.signOut();
        ThreadUtils.runOnUiThreadBlocking(() -> assertFalse(mIdentityManager.hasPrimaryAccount()));

        assertAccountsUpdatedCount(0);
        assertProfileDataUpdatedCount(0);
    }

    /** Creates a list of distinct accounts, see {@link #createAccount(int)}. */
    private static List<AccountInfo> createAccountsList(int size) {
        List<AccountInfo> accounts = new ArrayList<>();
        for (int i = 0; i < size; i++) {
            accounts.add(createAccount(i));
        }
        return accounts;
    }

    /** Creates an account with an avatar image and all the capabilities known. */
    private static AccountInfo createAccount(int index) {
        final String email = "account" + index + "@gmail.com";
        Map<String, Boolean> capabilities = new HashMap<>();
        // Known capabilities prevent the native AccountFetcherService from fetching them
        // asynchronously. Such a fetch would update the account info at an arbitrary point in time
        // and make the number of observer calls non-deterministic.
        // TODO(crbug.com/569918576): Remove setting capabilities when AccountCapabilitiesFetcher
        // implementation is fixed.
        for (String capabilityName :
                AccountCapabilitiesConstants.SUPPORTED_ACCOUNT_CAPABILITY_NAMES) {
            capabilities.put(capabilityName, false);
        }
        return new AccountInfo.Builder(email, FakeAccountManagerFacade.toGaiaId(email))
                .fullName("Account" + index + " Full")
                .givenName("Account" + index + " Given")
                .accountImage(TestAccounts.ACCOUNT1.getAccountImage())
                .accountCapabilities(new AccountCapabilities(capabilities))
                .build();
    }

    private void addAccounts(List<AccountInfo> accounts) {
        for (AccountInfo account : accounts) {
            mSigninTestRule.addAccount(account);
        }
    }

    private void createProfileDataCache() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileDataCache =
                            ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                                    ContextUtils.getApplicationContext(), mIdentityManager);
                    mProfileDataCache.addObserver(mObserver);
                });
    }

    /** Waits until the {@link ProfileDataCache#getAccounts()} contains exactly the given ones. */
    private void waitForCachedAccounts(List<AccountInfo> expectedAccounts) {
        Set<CoreAccountId> expectedIds = new HashSet<>();
        for (AccountInfo account : expectedAccounts) {
            expectedIds.add(account.getId());
        }
        CriteriaHelper.pollUiThread(
                () -> {
                    var accounts = mProfileDataCache.getAccounts();
                    Criteria.checkThat(accounts.isFulfilled(), is(true));
                    Criteria.checkThat(accounts.getResult().size(), is(expectedIds.size()));
                    for (DisplayableProfileData profileData : accounts.getResult()) {
                        Criteria.checkThat(expectedIds, hasItem(profileData.getAccountId()));
                    }
                    return true;
                },
                "The cached accounts don't match " + expectedIds);
    }

    private void assertAccountsUpdatedCount(int expected) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertEquals(
                                "Unexpected number of onAccountsUpdated() calls",
                                expected,
                                mObserver.mAccountsUpdatedCount));
    }

    private void assertProfileDataUpdatedCount(int expected) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertEquals(
                                "Unexpected number of onProfileDataUpdated() calls",
                                expected,
                                mObserver.mProfileDataUpdatedCount));
    }
}
