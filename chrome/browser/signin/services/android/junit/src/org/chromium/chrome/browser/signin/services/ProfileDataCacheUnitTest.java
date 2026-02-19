// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.never;
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
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.TestAccounts;

/** Unit tests for {@link ProfileDataCache} */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class ProfileDataCacheUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private ProfileDataCache.Observer mObserverMock;

    private ProfileDataCache mProfileDataCache;

    @Before
    public void setUp() {
        mProfileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mAccountManagerTestRule.getIdentityManager());
    }

    @Test
    public void accountInfoIsUpdatedWithOnlyFullName() {
        final String fullName = "full name1";
        final AccountInfo accountWithFullName =
                new AccountInfo.Builder(TestAccounts.TEST_ACCOUNT_NO_NAME)
                        .fullName(fullName)
                        .build();
        final String accountEmail = accountWithFullName.getEmail();
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertFalse(mProfileDataCache.hasProfileDataForTesting(accountEmail));
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());
        Assert.assertNull(mProfileDataCache.getProfileDataOrDefault(accountEmail).getFullName());

        mAccountManagerTestRule.addAccount(accountWithFullName);

        Assert.assertTrue(mProfileDataCache.getAccounts().isFulfilled());
        Assert.assertEquals(1, mProfileDataCache.getAccounts().getResult().size());
        Assert.assertTrue(mProfileDataCache.hasProfileDataForTesting(accountEmail));
        Assert.assertEquals(
                fullName, mProfileDataCache.getProfileDataOrDefault(accountEmail).getFullName());
    }

    @Test
    public void accountInfoIsUpdatedWithOnlyGivenName() {
        final String givenName = "given name1";
        final AccountInfo accountWithGivenName =
                new AccountInfo.Builder(TestAccounts.TEST_ACCOUNT_NO_NAME)
                        .givenName(givenName)
                        .build();
        final String accountEmail = accountWithGivenName.getEmail();
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());
        Assert.assertFalse(mProfileDataCache.hasProfileDataForTesting(accountEmail));
        Assert.assertNull(mProfileDataCache.getProfileDataOrDefault(accountEmail).getGivenName());

        mAccountManagerTestRule.addAccount(accountWithGivenName);

        Assert.assertTrue(mProfileDataCache.hasProfileDataForTesting(accountEmail));
        Assert.assertEquals(
                accountEmail, mProfileDataCache.getAccounts().getResult().get(0).getAccountEmail());
        Assert.assertEquals(
                givenName, mProfileDataCache.getAccounts().getResult().get(0).getGivenName());
        Assert.assertEquals(
                givenName, mProfileDataCache.getProfileDataOrDefault(accountEmail).getGivenName());
    }

    @Test
    public void accountInfoIsUpdatedWithOnlyBadgeConfig() {
        mProfileDataCache.setBadge(
                TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail(),
                ProfileDataCache.createDefaultSizeChildAccountBadgeConfig(
                        RuntimeEnvironment.application.getApplicationContext(),
                        R.drawable.ic_sync_badge_error_20dp));
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertFalse(
                mProfileDataCache.hasProfileDataForTesting(
                        TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail()));
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());

        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);

        Assert.assertTrue(
                mProfileDataCache.hasProfileDataForTesting(
                        TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail()));
        Assert.assertFalse(mProfileDataCache.getAccounts().getResult().isEmpty());
        Assert.assertEquals(
                TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail(),
                mProfileDataCache.getAccounts().getResult().get(0).getAccountEmail());
    }

    @Test
    public void cacheShouldBePopulatedOnInitialization() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
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

        Assert.assertEquals(1, mProfileDataCache.getAccounts().getResult().size());
        Assert.assertEquals(
                TestAccounts.ACCOUNT1.getEmail(),
                mProfileDataCache.getAccounts().getResult().get(0).getAccountEmail());
    }

    @Test
    public void cacheShouldBePopulatedOnIdentityManagerAccountsUpdate() {
        mAccountManagerTestRule.blockGetAccountsUpdate(false);
        mProfileDataCache.addObserver(mObserverMock);

        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());

        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        Assert.assertEquals(1, mProfileDataCache.getAccounts().getResult().size());
        Assert.assertEquals(
                TestAccounts.ACCOUNT1.getEmail(),
                mProfileDataCache.getAccounts().getResult().get(0).getAccountEmail());
    }

    @Test
    public void cacheShouldNotBePopulatedOnInitializationWhenGetAccountsFails() {
        mAccountManagerTestRule.blockGetAccountsUpdate(false);
        var profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mAccountManagerTestRule.getIdentityManager());
        Assert.assertFalse(profileDataCache.getAccounts().isFulfilled());
    }

    @Test
    public void cacheShouldBeNotPopulatedOnAccountWithoutDisplayableInfoOnCoreAccountsUpdate() {
        mAccountManagerTestRule.blockExtendedAccountInfoUpdate();
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());
    }

    @Test
    public void
            cacheShouldBeNotPopulatedOnAccountWithoutDisplayableInfoOnIdentityManagerAccountsUpdate() {
        mAccountManagerTestRule.blockGetAccountsUpdate(false);
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());
    }

    @Test
    public void
            cacheShouldBePopulatedOnAccountWithoutDisplayableInfoWithCustomBadgeOnCoreAccountsUpdate() {
        mAccountManagerTestRule.blockExtendedAccountInfoUpdate();
        mProfileDataCache.setBadge(
                TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail(),
                ProfileDataCache.createDefaultSizeChildAccountBadgeConfig(
                        RuntimeEnvironment.application.getApplicationContext(),
                        R.drawable.ic_sync_badge_error_20dp));
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        Assert.assertFalse(mProfileDataCache.getAccounts().getResult().isEmpty());
    }

    @Test
    public void
            cacheShouldBePopulatedOnAccountWithoutDisplayableInfoWithCustomBadgeOnIdentityManagerAccountsUpdate() {
        mAccountManagerTestRule.blockGetAccountsUpdate(false);
        mProfileDataCache.setBadge(
                TestAccounts.TEST_ACCOUNT_NO_NAME.getEmail(),
                ProfileDataCache.createDefaultSizeChildAccountBadgeConfig(
                        RuntimeEnvironment.application.getApplicationContext(),
                        R.drawable.ic_sync_badge_error_20dp));
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertTrue(mProfileDataCache.getAccounts().getResult().isEmpty());
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        Assert.assertFalse(mProfileDataCache.getAccounts().getResult().isEmpty());
    }

    @Test
    public void testOnProfileDataUpdatedIsEmittedIfAccountsAreNotReadyDuringInitialization() {
        var updateBlocker = mAccountManagerTestRule.blockGetAccountsUpdate(false);
        mAccountManagerTestRule.blockExtendedAccountInfoUpdate();
        var profileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext(),
                        mAccountManagerTestRule.getIdentityManager());
        profileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        updateBlocker.close();
        verify(mObserverMock).onAccountsUpdated(any());
        verify(mObserverMock).onProfileDataUpdated(any());
    }

    @Test
    public void testObserverIsExecutedOnAccountsManagerAccountsUpdate() {
        mAccountManagerTestRule.blockExtendedAccountInfoUpdate();
        mProfileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        var accounts = mProfileDataCache.getAccounts().getResult();
        Assert.assertEquals(2, accounts.size());
        Assert.assertEquals(TestAccounts.ACCOUNT1.getEmail(), accounts.get(0).getAccountEmail());
        Assert.assertEquals(TestAccounts.ACCOUNT2.getEmail(), accounts.get(1).getAccountEmail());
        verify(mObserverMock).onAccountsUpdated(accounts);
        // TODO(crbug.com/485130949): onProfileDataUpdated should be never called after
        // onAccountsUpdated is called. (Blocked by crbug.com/480239119)
        verify(mObserverMock, times(2)).onProfileDataUpdated(TestAccounts.ACCOUNT1.getEmail());
        verify(mObserverMock).onProfileDataUpdated(TestAccounts.ACCOUNT2.getEmail());
    }

    @Test
    public void testObserverIsExecutedOnIdentityManagerAccountsUpdate() {
        mAccountManagerTestRule.blockGetAccountsUpdate(false);
        mProfileDataCache.addObserver(mObserverMock);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        verify(mObserverMock, never()).onAccountsUpdated(any());
        verify(mObserverMock).onProfileDataUpdated(any());
    }
}
