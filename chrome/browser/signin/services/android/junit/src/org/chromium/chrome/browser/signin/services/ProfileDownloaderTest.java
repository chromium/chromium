// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;

import androidx.annotation.Px;

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
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.ProfileDownloader.PendingProfileDownloads;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.ProfileDataSource.ProfileData;

/**
 * Unit tests for {@link ProfileDownloader}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ProfileDownloaderTest {
    private static final String ACCOUNT_EMAIL = "test@gmail.com";
    private static final @Px int IMAGE_SIZE = 64;

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private ProfileDownloader.Natives mProfileDownloaderNativeMock;

    @Mock
    private Profile mProfileMock;

    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock
    private AccountTrackerService mAccountTrackerServiceMock;

    @Captor
    private ArgumentCaptor<ProfileData> mProfileDataCaptor;

    @Before
    public void setUp() {
        mocker.mock(ProfileDownloaderJni.TEST_HOOKS, mProfileDownloaderNativeMock);
        Profile.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        when(mIdentityServicesProviderMock.getAccountTrackerService(mProfileMock))
                .thenReturn(mAccountTrackerServiceMock);
    }

    @Test
    public void testFetchingAccountInfoWhenAccountsAreNotSeeded() {
        when(mAccountTrackerServiceMock.checkAndSeedSystemAccounts()).thenReturn(false);
        ProfileDownloader.get().startFetchingAccountInfoFor(ACCOUNT_EMAIL, IMAGE_SIZE);
        final PendingProfileDownloads pendingProfileDownloads =
                ProfileDownloader.PendingProfileDownloads.get();
        verify(mAccountTrackerServiceMock).addSystemAccountsSeededListener(pendingProfileDownloads);
        verify(mProfileDownloaderNativeMock, never())
                .startFetchingAccountInfoFor(any(), anyString(), anyInt(), anyBoolean());
        pendingProfileDownloads.onSystemAccountsSeedingComplete();
        verify(mProfileDownloaderNativeMock)
                .startFetchingAccountInfoFor(mProfileMock, ACCOUNT_EMAIL, IMAGE_SIZE, true);
    }

    @Test
    public void testFetchingAccountInfoWhenAccountsAreSeeded() {
        when(mAccountTrackerServiceMock.checkAndSeedSystemAccounts()).thenReturn(true);
        ProfileDownloader.get().startFetchingAccountInfoFor(ACCOUNT_EMAIL, IMAGE_SIZE);
        final PendingProfileDownloads pendingProfileDownloads =
                ProfileDownloader.PendingProfileDownloads.get();
        verify(mAccountTrackerServiceMock, never())
                .addSystemAccountsSeededListener(pendingProfileDownloads);
        verify(mProfileDownloaderNativeMock)
                .startFetchingAccountInfoFor(mProfileMock, ACCOUNT_EMAIL, IMAGE_SIZE, true);
    }

    @Test
    public void testOnProfileDownloadSuccess() {
        final String fullName = "Full name";
        final String givenName = "Given name";
        final Bitmap avatar = mock(Bitmap.class);
        final ProfileDataSource.Observer observer = mock(ProfileDataSource.Observer.class);

        ProfileDownloader.get().addObserver(observer);
        ProfileDownloader.onProfileDownloadSuccess(ACCOUNT_EMAIL, fullName, givenName, avatar);

        verify(observer).onProfileDataUpdated(mProfileDataCaptor.capture());
        final ProfileData profileData = mProfileDataCaptor.getValue();
        Assert.assertEquals(ACCOUNT_EMAIL, profileData.getAccountEmail());
        Assert.assertEquals(fullName, profileData.getFullName());
        Assert.assertEquals(givenName, profileData.getGivenName());
        Assert.assertEquals(avatar, profileData.getAvatar());
    }
}
