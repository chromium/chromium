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
import org.chromium.chrome.browser.signin.services.ProfileDownloader.PendingProfileDownloads;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.ProfileDataSource.ProfileData;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Unit tests for {@link ProfileDownloader}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ProfileDownloaderTest {
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
    private ProfileDataSource.Observer mObserverMock;

    @Captor
    private ArgumentCaptor<ProfileData> mProfileDataCaptor;

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        when(mIdentityServicesProviderMock.getAccountTrackerService(mProfileMock))
                .thenReturn(mAccountTrackerServiceMock);
        when(mIdentityServicesProviderMock.getIdentityManager(mProfileMock))
                .thenReturn(mIdentityManagerMock);
    }

    @After
    public void tearDown() {
        ProfileDownloader.resetForTests();
    }

    @Test
    public void testFetchingAccountInfoWhenAccountsAreNotSeeded() {
        when(mAccountTrackerServiceMock.checkAndSeedSystemAccounts()).thenReturn(false);

        ProfileDownloader.get().addObserver(mObserverMock);
        ProfileDownloader.get().startFetchingAccountInfoFor(ACCOUNT_EMAIL);
        final PendingProfileDownloads pendingProfileDownloads =
                ProfileDownloader.PendingProfileDownloads.get();

        verify(mAccountTrackerServiceMock).addSystemAccountsSeededListener(pendingProfileDownloads);
        verify(mIdentityManagerMock, never()).forceRefreshOfExtendedAccountInfo(any());
        verify(mObserverMock, never()).onProfileDataUpdated(any());
    }

    @Test
    public void testFetchingAccountInfoWhenAccountsAreSeededAndNoAccountInfoAvailable() {
        when(mAccountTrackerServiceMock.checkAndSeedSystemAccounts()).thenReturn(true);

        ProfileDownloader.get().addObserver(mObserverMock);
        ProfileDownloader.get().startFetchingAccountInfoFor(ACCOUNT_EMAIL);
        final PendingProfileDownloads pendingProfileDownloads =
                ProfileDownloader.PendingProfileDownloads.get();

        verify(mAccountTrackerServiceMock).addSystemAccountsSeededListener(pendingProfileDownloads);
        verify(mIdentityManagerMock)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(ACCOUNT_EMAIL);
        verify(mIdentityManagerMock, never()).forceRefreshOfExtendedAccountInfo(any());
        verify(mObserverMock, never()).onProfileDataUpdated(any());
    }

    @Test
    public void testFetchingAccountInfoWhenAccountsAreSeededAndAccountInfoHasNoImage() {
        when(mAccountTrackerServiceMock.checkAndSeedSystemAccounts()).thenReturn(true);
        final AccountInfo accountInfo = new AccountInfo(new CoreAccountId("gaia-id-test"),
                ACCOUNT_EMAIL, "gaia-id-test", "full name", "given name", null);
        when(mIdentityManagerMock.findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                     ACCOUNT_EMAIL))
                .thenReturn(accountInfo);

        ProfileDownloader.get().addObserver(mObserverMock);
        ProfileDownloader.get().startFetchingAccountInfoFor(ACCOUNT_EMAIL);
        final PendingProfileDownloads pendingProfileDownloads =
                ProfileDownloader.PendingProfileDownloads.get();
        verify(mAccountTrackerServiceMock).addSystemAccountsSeededListener(pendingProfileDownloads);
        verify(mIdentityManagerMock)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(ACCOUNT_EMAIL);
        verify(mIdentityManagerMock).forceRefreshOfExtendedAccountInfo(accountInfo.getId());
        verify(mObserverMock, never()).onProfileDataUpdated(any());
    }

    @Test
    public void testFetchingAccountInfoWhenAccountsAreSeededAndAccountInfoHasImage() {
        when(mAccountTrackerServiceMock.checkAndSeedSystemAccounts()).thenReturn(true);
        final AccountInfo accountInfo = new AccountInfo(new CoreAccountId("gaia-id-test"),
                ACCOUNT_EMAIL, "gaia-id-test", "full name", "given name", mock(Bitmap.class));
        when(mIdentityManagerMock.findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                     ACCOUNT_EMAIL))
                .thenReturn(accountInfo);

        ProfileDownloader.get().addObserver(mObserverMock);
        ProfileDownloader.get().startFetchingAccountInfoFor(ACCOUNT_EMAIL);
        final PendingProfileDownloads pendingProfileDownloads =
                ProfileDownloader.PendingProfileDownloads.get();
        verify(mAccountTrackerServiceMock).addSystemAccountsSeededListener(pendingProfileDownloads);
        verify(mIdentityManagerMock)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(ACCOUNT_EMAIL);
        verify(mIdentityManagerMock, never()).forceRefreshOfExtendedAccountInfo(any());
        verify(mObserverMock).onProfileDataUpdated(mProfileDataCaptor.capture());
        final ProfileData profileData = mProfileDataCaptor.getValue();
        Assert.assertEquals(ACCOUNT_EMAIL, profileData.getAccountEmail());
        Assert.assertEquals(accountInfo.getFullName(), profileData.getFullName());
        Assert.assertEquals(accountInfo.getGivenName(), profileData.getGivenName());
        Assert.assertEquals(accountInfo.getAccountImage(), profileData.getAvatar());
    }
}
