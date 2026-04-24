// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.util.Arrays;
import java.util.Collection;

/** Unit tests for {@link ProfileDataCache} */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class ProfileDataCacheUnitTest {

    // TODO(crbug.com/493130564) - Remove the data source parameterization after
    // MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS launch. Test should be reverted to use
    // BaseRobolectricTestRule after that.
    @Rule(order = Rule.DEFAULT_ORDER - 1)
    public final BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Parameters(name = "{index}_isIdentityManagerSourceOfAccounts={0}")
    public static Collection parameters() {
        return Arrays.asList(false, true);
    }

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(
                    spy(new FakeAccountManagerFacade()), spy(new FakeIdentityManager()));

    @Mock private ProfileDataCache.Observer mObserverMock;

    private ProfileDataCache mProfileDataCache;

    @Parameter(0)
    public boolean mIsIdentityManagerSourceOfAccounts;

    @Before
    public void setUp() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
                mIsIdentityManagerSourceOfAccounts);
        mProfileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mAccountManagerTestRule.getIdentityManager());
    }

    @Test
    public void accountInfoIsUpdatedWithOnlyFullName() {
        final String fullName = "full name1";
        final CoreAccountId accountId = TestAccounts.TEST_ACCOUNT_NO_NAME.getId();
        final String accountEmail = TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail();

        Assert.assertFalse(mProfileDataCache.hasProfileDataForTesting(accountId));

        mProfileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertTrue(mProfileDataCache.hasProfileDataForTesting(accountId));
        Assert.assertEquals(accountEmail, mProfileDataCache.getById(accountId).getAccountEmail());
        Assert.assertEquals(accountId, mProfileDataCache.getById(accountId).getAccountId());
        Assert.assertNull(mProfileDataCache.getById(accountId).getFullName());

        mAccountManagerTestRule.addAccount(
                new AccountInfo.Builder(TestAccounts.TEST_ACCOUNT_NO_NAME)
                        .fullName(fullName)
                        .build());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertTrue(mProfileDataCache.hasProfileDataForTesting(accountId));
        Assert.assertEquals(1, mProfileDataCache.getAccounts().getResult().size());
        Assert.assertEquals(accountEmail, mProfileDataCache.getById(accountId).getAccountEmail());
        Assert.assertEquals(accountId, mProfileDataCache.getById(accountId).getAccountId());
        Assert.assertEquals(fullName, mProfileDataCache.getById(accountId).getFullName());
        Assert.assertNull(mProfileDataCache.getById(accountId).getGivenName());
    }

    @Test
    public void accountInfoIsUpdatedWithOnlyGivenName() {
        final String givenName = "given name1";
        final CoreAccountId accountId = TestAccounts.TEST_ACCOUNT_NO_NAME.getId();
        final String accountEmail = TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail();

        Assert.assertFalse(mProfileDataCache.hasProfileDataForTesting(accountId));

        mProfileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertTrue(mProfileDataCache.hasProfileDataForTesting(accountId));
        Assert.assertEquals(accountEmail, mProfileDataCache.getById(accountId).getAccountEmail());
        Assert.assertEquals(accountId, mProfileDataCache.getById(accountId).getAccountId());
        Assert.assertNull(mProfileDataCache.getById(accountId).getGivenName());

        mAccountManagerTestRule.addAccount(
                new AccountInfo.Builder(TestAccounts.TEST_ACCOUNT_NO_NAME)
                        .givenName(givenName)
                        .build());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertTrue(mProfileDataCache.hasProfileDataForTesting(accountId));
        Assert.assertEquals(1, mProfileDataCache.getAccounts().getResult().size());
        Assert.assertEquals(accountEmail, mProfileDataCache.getById(accountId).getAccountEmail());
        Assert.assertEquals(accountId, mProfileDataCache.getById(accountId).getAccountId());
        Assert.assertEquals(givenName, mProfileDataCache.getById(accountId).getGivenName());
        Assert.assertNull(mProfileDataCache.getById(accountId).getFullName());
    }

    @Test
    public void accountInfoIsUpdatedWithOnlyBadgeConfig() {
        mProfileDataCache.setBadge(
                TestAccounts.TEST_ACCOUNT_NO_NAME.getId(),
                BadgeConfig.create(R.drawable.ic_error)
                        .withDefaultSizeChildAccountConfig()
                        .build(RuntimeEnvironment.application.getApplicationContext()));
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertFalse(
                mProfileDataCache.hasProfileDataForTesting(
                        TestAccounts.TEST_ACCOUNT_NO_NAME.getId()));
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());

        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertTrue(
                mProfileDataCache.hasProfileDataForTesting(
                        TestAccounts.TEST_ACCOUNT_NO_NAME.getId()));
        Assert.assertFalse(mProfileDataCache.getAccounts().getResult().isEmpty());
        Assert.assertEquals(
                TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail(),
                mProfileDataCache.getAccounts().getResult().get(0).getAccountEmail());
        Assert.assertEquals(
                TestAccounts.TEST_ACCOUNT_NO_NAME.getId(),
                mProfileDataCache.getAccounts().getResult().get(0).getAccountId());
    }

    @Test
    public void cacheShouldBePopulatedOnInitialization() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        RobolectricUtil.runAllBackgroundAndUi();
        var profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mAccountManagerTestRule.getIdentityManager());
        Assert.assertTrue(profileDataCache.getAccounts().isFulfilled());
        Assert.assertEquals(1, profileDataCache.getAccounts().getResult().size());
    }

    @Test
    public void cacheShouldBePopulatedOnCoreAccountsUpdate() {
        mAccountManagerTestRule.blockExtendedAccountInfoUpdate();
        mProfileDataCache.addObserver(mObserverMock);

        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());

        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(1, mProfileDataCache.getAccounts().getResult().size());
        Assert.assertEquals(
                TestAccounts.ACCOUNT1.getEmail(),
                mProfileDataCache.getAccounts().getResult().get(0).getAccountEmail());
        Assert.assertEquals(
                TestAccounts.ACCOUNT1.getId(),
                mProfileDataCache.getAccounts().getResult().get(0).getAccountId());
    }

    @Test
    public void cacheShouldBePopulatedOnIdentityManagerAccountsUpdate() {
        mAccountManagerTestRule.blockGetAccountsUpdate();
        mProfileDataCache.addObserver(mObserverMock);

        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());

        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(1, mProfileDataCache.getAccounts().getResult().size());
        Assert.assertEquals(
                TestAccounts.ACCOUNT1.getEmail(),
                mProfileDataCache.getAccounts().getResult().get(0).getAccountEmail());
        Assert.assertEquals(
                TestAccounts.ACCOUNT1.getId(),
                mProfileDataCache.getAccounts().getResult().get(0).getAccountId());
    }

    @Test
    public void cacheShouldNotBePopulatedOnInitializationWhenGetAccountsFails() {
        mAccountManagerTestRule.blockGetAccountsUpdate();
        var profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mAccountManagerTestRule.getIdentityManager());
        Assert.assertFalse(profileDataCache.getAccounts().isFulfilled());
    }

    @Test
    public void cacheShouldBePopulatedOnAccountWithoutDisplayableInfoOnCoreAccountsUpdate() {
        mAccountManagerTestRule.blockExtendedAccountInfoUpdate();
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertTrue(
                mProfileDataCache.hasProfileDataForTesting(
                        TestAccounts.TEST_ACCOUNT_NO_NAME.getId()));
    }

    @Test
    public void
            cacheShouldBePopulatedOnAccountWithoutDisplayableInfoOnIdentityManagerAccountsUpdate() {
        mAccountManagerTestRule.blockGetAccountsUpdate();
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertTrue(
                mProfileDataCache.hasProfileDataForTesting(
                        TestAccounts.TEST_ACCOUNT_NO_NAME.getId()));
    }

    @Test
    public void
            cacheShouldBePopulatedOnAccountWithoutDisplayableInfoWithCustomBadgeOnCoreAccountsUpdate() {
        mAccountManagerTestRule.blockExtendedAccountInfoUpdate();
        mProfileDataCache.setBadge(
                TestAccounts.TEST_ACCOUNT_NO_NAME.getId(),
                BadgeConfig.create(R.drawable.ic_error)
                        .withDefaultSizeChildAccountConfig()
                        .build(RuntimeEnvironment.application.getApplicationContext()));
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertTrue(
                mProfileDataCache.hasProfileDataForTesting(
                        TestAccounts.TEST_ACCOUNT_NO_NAME.getId()));
    }

    @Test
    public void
            cacheShouldBePopulatedOnAccountWithoutDisplayableInfoWithCustomBadgeOnIdentityManagerAccountsUpdate() {
        mAccountManagerTestRule.blockGetAccountsUpdate();
        mProfileDataCache.setBadge(
                TestAccounts.TEST_ACCOUNT_NO_NAME.getId(),
                BadgeConfig.create(R.drawable.ic_error)
                        .withDefaultSizeChildAccountConfig()
                        .build(RuntimeEnvironment.application.getApplicationContext()));
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertTrue(
                mProfileDataCache.hasProfileDataForTesting(
                        TestAccounts.TEST_ACCOUNT_NO_NAME.getId()));
    }

    @Test
    public void getAccountsShouldReturnAccountsInGivenOrder() {
        mProfileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        mAccountManagerTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        RobolectricUtil.runAllBackgroundAndUi();

        var profileData = mProfileDataCache.getAccounts().getResult();
        Assert.assertEquals(3, profileData.size());
        Assert.assertEquals(TestAccounts.ACCOUNT2.getEmail(), profileData.get(0).getAccountEmail());
        Assert.assertEquals(TestAccounts.ACCOUNT2.getId(), profileData.get(0).getAccountId());
        Assert.assertEquals(
                TestAccounts.CHILD_ACCOUNT.getEmail(), profileData.get(1).getAccountEmail());
        Assert.assertEquals(TestAccounts.CHILD_ACCOUNT.getId(), profileData.get(1).getAccountId());
        Assert.assertEquals(TestAccounts.ACCOUNT1.getEmail(), profileData.get(2).getAccountEmail());
        Assert.assertEquals(TestAccounts.ACCOUNT1.getId(), profileData.get(2).getAccountId());
    }

    @Test
    public void cacheShouldBeUpdatedWhenAccountIsRemoved() {
        mProfileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(2, mProfileDataCache.getAccounts().getResult().size());
        Assert.assertEquals(
                TestAccounts.ACCOUNT1.getEmail(),
                mProfileDataCache.getById(TestAccounts.ACCOUNT1.getId()).getAccountEmail());
        Assert.assertEquals(
                TestAccounts.ACCOUNT1.getId(),
                mProfileDataCache.getById(TestAccounts.ACCOUNT1.getId()).getAccountId());
        Assert.assertEquals(
                TestAccounts.ACCOUNT2.getEmail(),
                mProfileDataCache.getById(TestAccounts.ACCOUNT2.getId()).getAccountEmail());
        Assert.assertEquals(
                TestAccounts.ACCOUNT2.getId(),
                mProfileDataCache.getById(TestAccounts.ACCOUNT2.getId()).getAccountId());

        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(1, mProfileDataCache.getAccounts().getResult().size());
        Assert.assertEquals(
                TestAccounts.ACCOUNT2.getEmail(),
                mProfileDataCache.getAccounts().getResult().get(0).getAccountEmail());
        Assert.assertEquals(
                TestAccounts.ACCOUNT2.getId(),
                mProfileDataCache.getAccounts().getResult().get(0).getAccountId());
        CoreAccountId accountId = TestAccounts.ACCOUNT1.getId();
        Assert.assertThrows(
                IllegalArgumentException.class, () -> mProfileDataCache.getById(accountId));
        Assert.assertEquals(
                TestAccounts.ACCOUNT2.getEmail(),
                mProfileDataCache.getById(TestAccounts.ACCOUNT2.getId()).getAccountEmail());
        Assert.assertEquals(
                TestAccounts.ACCOUNT2.getId(),
                mProfileDataCache.getById(TestAccounts.ACCOUNT2.getId()).getAccountId());
    }

    @Test
    public void givenCachedAccountWhenGetByIdThenReturnCachedAccountWithoutFallback() {
        var accountInfo = TestAccounts.ACCOUNT1;
        mProfileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.addAccount(accountInfo);
        RobolectricUtil.runAllBackgroundAndUi();

        var expectedIdentityManagerCalls = mIsIdentityManagerSourceOfAccounts ? 2 : 0;
        var expectedAccountManagerFacadeCalls = mIsIdentityManagerSourceOfAccounts ? 0 : 2;

        verify(
                        mAccountManagerTestRule.getAccountManagerFacade(),
                        times(expectedAccountManagerFacadeCalls))
                .getAccounts();
        verify(mAccountManagerTestRule.getIdentityManager(), times(expectedIdentityManagerCalls))
                .getExtendedAccountInfoForAccountsWithRefreshToken();
        DisplayableProfileData cachedProfileData = mProfileDataCache.getById(accountInfo.getId());
        verify(
                        mAccountManagerTestRule.getAccountManagerFacade(),
                        times(expectedAccountManagerFacadeCalls))
                .getAccounts();
        verify(mAccountManagerTestRule.getIdentityManager(), times(expectedIdentityManagerCalls))
                .getExtendedAccountInfoForAccountsWithRefreshToken();
        Assert.assertEquals(accountInfo.getEmail(), cachedProfileData.getAccountEmail());
        Assert.assertEquals(accountInfo.getId(), cachedProfileData.getAccountId());
        Assert.assertEquals(accountInfo.getFullName(), cachedProfileData.getFullName());
        Assert.assertEquals(accountInfo.getGivenName(), cachedProfileData.getGivenName());
    }

    @Test(expected = IllegalArgumentException.class)
    public void givenUnknownAccountIdWhenGetByIdThenShouldThrowException() {
        mProfileDataCache.getById(TestAccounts.ACCOUNT1.getId());
    }

    @Test
    public void testOnProfileDataUpdatedIsEmittedIfAccountsAreNotReadyDuringInitialization() {
        var updateBlocker = mAccountManagerTestRule.blockGetAccountsUpdate();
        mAccountManagerTestRule.blockExtendedAccountInfoUpdate();
        var profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mAccountManagerTestRule.getIdentityManager());
        profileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        RobolectricUtil.runAllBackgroundAndUi();
        updateBlocker.close();
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mObserverMock).onAccountsUpdated(any());
        verify(mObserverMock).onProfileDataUpdated(any());
    }

    @Test
    public void testObserverIsExecutedOnAccountsManagerAccountsUpdate() {
        mAccountManagerTestRule.blockExtendedAccountInfoUpdate();
        mProfileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        RobolectricUtil.runAllBackgroundAndUi();
        var accounts = mProfileDataCache.getAccounts().getResult();
        Assert.assertEquals(2, accounts.size());
        Assert.assertEquals(TestAccounts.ACCOUNT1.getEmail(), accounts.get(0).getAccountEmail());
        Assert.assertEquals(TestAccounts.ACCOUNT1.getId(), accounts.get(0).getAccountId());
        Assert.assertEquals(TestAccounts.ACCOUNT2.getEmail(), accounts.get(1).getAccountEmail());
        Assert.assertEquals(TestAccounts.ACCOUNT2.getId(), accounts.get(1).getAccountId());
        verify(mObserverMock).onAccountsUpdated(accounts);
        // TODO(crbug.com/485130949): onProfileDataUpdated should be never called after
        // onAccountsUpdated is called. (Blocked by crbug.com/480239119)
        verify(mObserverMock).onProfileDataUpdated(accounts.get(0));
        verify(mObserverMock).onProfileDataUpdated(accounts.get(1));
    }

    @Test
    public void testObserverIsExecutedOnIdentityManagerAccountsUpdate() {
        mAccountManagerTestRule.blockGetAccountsUpdate();
        RobolectricUtil.runAllBackgroundAndUi();
        mProfileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mObserverMock, never()).onAccountsUpdated(any());
        verify(mObserverMock).onProfileDataUpdated(any());
    }

    @Test
    public void testUpdateShouldFallbackToPrimaryAccountInfoIfAccountsAreNotReady() {
        mAccountManagerTestRule.blockGetAccountsUpdate();
        mAccountManagerTestRule.blockExtendedAccountInfoUpdate();
        mAccountManagerTestRule
                .getIdentityManager()
                .setPrimaryAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        // Create a new ProfileDataCache to ensure that the accounts are not ready.
        var profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mAccountManagerTestRule.getIdentityManager());
        var accounts = profileDataCache.getAccounts().getResult();
        var profileData = profileDataCache.getById(TestAccounts.TEST_ACCOUNT_NO_NAME.getId());

        Assert.assertEquals(1, accounts.size());
        Assert.assertEquals(
                TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail(), profileData.getAccountEmail());
        Assert.assertEquals(TestAccounts.TEST_ACCOUNT_NO_NAME.getId(), profileData.getAccountId());
    }

    @Test
    public void testUpdateShouldPutInCacheBothPrimaryAndCoreAccounts() {
        var updateBlocker = mAccountManagerTestRule.blockGetAccountsUpdate();
        mAccountManagerTestRule.blockExtendedAccountInfoUpdate();
        // Create a new ProfileDataCache to ensure that the accounts are not ready.
        mProfileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mAccountManagerTestRule.getIdentityManager());

        updateBlocker.close();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT2);
        Assert.assertFalse(mProfileDataCache.getAccounts().isFulfilled());

        // getById should trigger the cache population
        Assert.assertEquals(
                TestAccounts.ACCOUNT1.getEmail(),
                mProfileDataCache.getById(TestAccounts.ACCOUNT1.getId()).getAccountEmail());
        Assert.assertEquals(
                TestAccounts.ACCOUNT1.getId(),
                mProfileDataCache.getById(TestAccounts.ACCOUNT1.getId()).getAccountId());
        Assert.assertEquals(
                TestAccounts.ACCOUNT2.getEmail(),
                mProfileDataCache.getById(TestAccounts.ACCOUNT2.getId()).getAccountEmail());
        Assert.assertEquals(
                TestAccounts.ACCOUNT2.getId(),
                mProfileDataCache.getById(TestAccounts.ACCOUNT2.getId()).getAccountId());
        Assert.assertEquals(2, mProfileDataCache.getAccounts().getResult().size());
    }

    // TODO(crbug.com/494569985): Remove after MakeIdentityManagerSourceOfAccounts flag cleanup
    @Test
    public void testUpdateProfileDataWithoutDisplayableInfo_Legacy() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS, false);
        mProfileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mAccountManagerTestRule.getIdentityManager());
        mProfileDataCache.addObserver(mObserverMock);

        AccountInfo accountWithoutDisplayableInfo =
                new AccountInfo.Builder(
                                TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail(),
                                TestAccounts.TEST_ACCOUNT_NO_NAME.getGaiaId())
                        .build();
        mAccountManagerTestRule.addAccount(accountWithoutDisplayableInfo);
        RobolectricUtil.runAllBackgroundAndUi();

        var profileData = mProfileDataCache.getById(accountWithoutDisplayableInfo.getId());
        Assert.assertEquals(
                accountWithoutDisplayableInfo.getEmail(), profileData.getAccountEmail());
        Assert.assertEquals(accountWithoutDisplayableInfo.getId(), profileData.getAccountId());
        Assert.assertNull(profileData.getFullName());
        Assert.assertNull(profileData.getGivenName());
    }

    // TODO(crbug.com/494569985): Remove after MakeIdentityManagerSourceOfAccounts flag cleanup
    @Test
    public void testUpdateProfileDataWithDisplayableInfo_Legacy() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS, false);
        mProfileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mAccountManagerTestRule.getIdentityManager());
        mProfileDataCache.addObserver(mObserverMock);

        AccountInfo accountWithDisplayableInfo =
                new AccountInfo.Builder(
                                TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail(),
                                TestAccounts.TEST_ACCOUNT_NO_NAME.getGaiaId())
                        .accountImage(TestAccounts.ACCOUNT1.getAccountImage())
                        .build();
        mAccountManagerTestRule.addAccount(accountWithDisplayableInfo);
        RobolectricUtil.runAllBackgroundAndUi();

        var profileData = mProfileDataCache.getById(accountWithDisplayableInfo.getId());
        Assert.assertEquals(accountWithDisplayableInfo.getEmail(), profileData.getAccountEmail());
        Assert.assertEquals("", profileData.getFullName());
        Assert.assertEquals("", profileData.getGivenName());
    }

    // TODO(crbug.com/494569985): Remove after MakeIdentityManagerSourceOfAccounts flag cleanup
    @Test
    public void testUpdateProfileDataWithBadgeConfig_Legacy() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS, false);
        mProfileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mAccountManagerTestRule.getIdentityManager());
        mProfileDataCache.addObserver(mObserverMock);

        AccountInfo accountWithDisplayableInfo =
                new AccountInfo.Builder(
                                TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail(),
                                TestAccounts.TEST_ACCOUNT_NO_NAME.getGaiaId())
                        .build();
        mProfileDataCache.setBadge(
                accountWithDisplayableInfo.getId(),
                BadgeConfig.create(R.drawable.ic_error)
                        .withDefaultSizeChildAccountConfig()
                        .build(RuntimeEnvironment.application.getApplicationContext()));
        mAccountManagerTestRule.addAccount(accountWithDisplayableInfo);
        RobolectricUtil.runAllBackgroundAndUi();

        var profileData = mProfileDataCache.getById(accountWithDisplayableInfo.getId());
        Assert.assertEquals(accountWithDisplayableInfo.getEmail(), profileData.getAccountEmail());
        Assert.assertEquals(accountWithDisplayableInfo.getId(), profileData.getAccountId());
        Assert.assertEquals("", profileData.getFullName());
        Assert.assertEquals("", profileData.getGivenName());
    }

    @Test
    public void testWhenAccountIsRemovedThenShouldNotNotifyOnProfileDataUpdated() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mProfileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());
        verify(mObserverMock, never()).onProfileDataUpdated(any());
    }

    @Test
    public void testAccountIdMatchesSourceAccountInfo() {
        mProfileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        RobolectricUtil.runAllBackgroundAndUi();

        // Check getAccounts()
        var accounts = mProfileDataCache.getAccounts().getResult();
        Assert.assertEquals(2, accounts.size());
        Assert.assertEquals(TestAccounts.ACCOUNT1.getId(), accounts.get(0).getAccountId());
        Assert.assertEquals(TestAccounts.ACCOUNT2.getId(), accounts.get(1).getAccountId());

        // Check getById()
        Assert.assertEquals(
                TestAccounts.ACCOUNT1.getId(),
                mProfileDataCache.getById(TestAccounts.ACCOUNT1.getId()).getAccountId());
        Assert.assertEquals(
                TestAccounts.ACCOUNT2.getId(),
                mProfileDataCache.getById(TestAccounts.ACCOUNT2.getId()).getAccountId());
    }
}
