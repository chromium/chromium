// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.AccountInfoService.PendingAccountInfoFetch;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Unit tests for {@link AccountInfoService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountInfoServiceTest {
    private static final String ACCOUNT_EMAIL = "test@gmail.com";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Profile mProfileMock;

    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock
    private AccountTrackerService mAccountTrackerServiceMock;

    @Mock
    private IdentityManager mIdentityManagerMock;

    @Mock
    private AccountInfoService.Observer mObserverMock;

    @Captor
    private ArgumentCaptor<AccountInfo> mAccountInfoCaptor;

    private final AccountInfo mAccountInfoWithAvatar =
            new AccountInfo(new CoreAccountId("gaia-id-test"), ACCOUNT_EMAIL, "gaia-id-test",
                    "full name", "given name", mock(Bitmap.class));

    private AccountInfoService mService;

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        when(mIdentityServicesProviderMock.getAccountTrackerService(mProfileMock))
                .thenReturn(mAccountTrackerServiceMock);
        when(mIdentityServicesProviderMock.getIdentityManager(mProfileMock))
                .thenReturn(mIdentityManagerMock);
        mService = AccountInfoService.get(mIdentityManagerMock);
    }

    @After
    public void tearDown() {
        AccountInfoService.resetForTests();
    }

    @Test
    public void testServiceIsAttachedToIdentityManager() {
        verify(mIdentityManagerMock).addObserver(mService);

        mService.destroy();
        verify(mIdentityManagerMock).removeObserver(mService);
    }

    @Test
    public void testObserverIsNotifiedWhenAdded() {
        mService.addObserver(mObserverMock);

        mService.onExtendedAccountInfoUpdated(mAccountInfoWithAvatar);
        verify(mObserverMock).onAccountInfoUpdated(mAccountInfoWithAvatar);
    }

    @Test
    public void testObserverIsNotNotifiedAfterRemoval() {
        mService.addObserver(mObserverMock);
        mService.removeObserver(mObserverMock);

        mService.onExtendedAccountInfoUpdated(mAccountInfoWithAvatar);
        verify(mObserverMock, never()).onAccountInfoUpdated(mAccountInfoWithAvatar);
    }

    @Test
    public void testFetchingAccountInfoWhenAccountsAreNotSeeded() {
        when(mAccountTrackerServiceMock.checkAndSeedSystemAccounts()).thenReturn(false);

        mService.startFetchingAccountInfoFor(ACCOUNT_EMAIL, mObserverMock);
        final PendingAccountInfoFetch pendingAccountInfoFetch = PendingAccountInfoFetch.get();

        verify(mAccountTrackerServiceMock).addSystemAccountsSeededListener(pendingAccountInfoFetch);
        verify(mIdentityManagerMock, never()).forceRefreshOfExtendedAccountInfo(any());
        verify(mObserverMock, never()).onAccountInfoUpdated(any());
    }

    @Test
    public void testFetchingAccountInfoWhenAccountsAreSeededAndNoAccountInfoAvailable() {
        when(mAccountTrackerServiceMock.checkAndSeedSystemAccounts()).thenReturn(true);

        mService.startFetchingAccountInfoFor(ACCOUNT_EMAIL, mObserverMock);
        final PendingAccountInfoFetch pendingAccountInfoFetch = PendingAccountInfoFetch.get();

        verify(mAccountTrackerServiceMock).addSystemAccountsSeededListener(pendingAccountInfoFetch);
        verify(mIdentityManagerMock)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(ACCOUNT_EMAIL);
        verify(mIdentityManagerMock, never()).forceRefreshOfExtendedAccountInfo(any());
        verify(mObserverMock, never()).onAccountInfoUpdated(any());
    }

    @Test
    public void testFetchingAccountInfoWhenAccountsAreSeededAndAccountInfoHasNoImage() {
        when(mAccountTrackerServiceMock.checkAndSeedSystemAccounts()).thenReturn(true);
        final AccountInfo accountInfo = new AccountInfo(new CoreAccountId("gaia-id-test"),
                ACCOUNT_EMAIL, "gaia-id-test", "full name", "given name", null);
        when(mIdentityManagerMock.findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                     ACCOUNT_EMAIL))
                .thenReturn(accountInfo);

        mService.startFetchingAccountInfoFor(ACCOUNT_EMAIL, mObserverMock);
        final PendingAccountInfoFetch pendingAccountInfoFetch = PendingAccountInfoFetch.get();
        verify(mAccountTrackerServiceMock).addSystemAccountsSeededListener(pendingAccountInfoFetch);
        verify(mIdentityManagerMock)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(ACCOUNT_EMAIL);
        verify(mIdentityManagerMock).forceRefreshOfExtendedAccountInfo(accountInfo.getId());
        verify(mObserverMock, never()).onAccountInfoUpdated(any());
    }

    @Test
    public void testFetchingAccountInfoWhenAccountsAreSeededAndAccountInfoHasImage() {
        when(mAccountTrackerServiceMock.checkAndSeedSystemAccounts()).thenReturn(true);
        when(mIdentityManagerMock.findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                     ACCOUNT_EMAIL))
                .thenReturn(mAccountInfoWithAvatar);

        mService.startFetchingAccountInfoFor(ACCOUNT_EMAIL, mObserverMock);
        final PendingAccountInfoFetch pendingAccountInfoFetch = PendingAccountInfoFetch.get();
        verify(mAccountTrackerServiceMock).addSystemAccountsSeededListener(pendingAccountInfoFetch);
        verify(mIdentityManagerMock)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(ACCOUNT_EMAIL);
        verify(mIdentityManagerMock, never()).forceRefreshOfExtendedAccountInfo(any());
        verify(mObserverMock).onAccountInfoUpdated(mAccountInfoCaptor.capture());
        final AccountInfo result = mAccountInfoCaptor.getValue();
        Assert.assertEquals(mAccountInfoWithAvatar.getEmail(), result.getEmail());
        Assert.assertEquals(mAccountInfoWithAvatar.getFullName(), result.getFullName());
        Assert.assertEquals(mAccountInfoWithAvatar.getGivenName(), result.getGivenName());
        Assert.assertEquals(mAccountInfoWithAvatar.getAccountImage(), result.getAccountImage());
    }
}
