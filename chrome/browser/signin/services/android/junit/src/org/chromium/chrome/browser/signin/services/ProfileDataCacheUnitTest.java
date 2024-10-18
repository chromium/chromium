// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import org.junit.After;
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
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManagerJni;

/** Unit tests for {@link ProfileDataCache} */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class ProfileDataCacheUnitTest {
    private static final long NATIVE_IDENTITY_MANAGER = 10001L;
    private static final AccountInfo ACCOUNT =
            new AccountInfo.Builder("test@gmail.com", "gaia-id").build();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public final JniMocker mocker = new JniMocker();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private IdentityManager.Natives mIdentityManagerNativeMock;

    @Mock private ProfileDataCache.Observer mObserverMock;

    private final IdentityManager mIdentityManager =
            IdentityManager.create(NATIVE_IDENTITY_MANAGER, null /* OAuth2TokenService */);

    private ProfileDataCache mProfileDataCache;

    @Before
    public void setUp() {
        mocker.mock(IdentityManagerJni.TEST_HOOKS, mIdentityManagerNativeMock);
        mProfileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(
                        RuntimeEnvironment.application.getApplicationContext());

        // Add an observer for IdentityManager::onExtendedAccountInfoUpdated.
        mAccountManagerTestRule.observeIdentityManager(mIdentityManager);
    }

    @After
    public void tearDown() {
        AccountInfoServiceProvider.resetForTests();
    }

    @Test
    public void accountInfoIsUpdatedWithOnlyFullName() {
        final String fullName = "full name1";
        final AccountInfo accountWithFullName =
                new AccountInfo.Builder(ACCOUNT).fullName(fullName).build();
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertFalse(mProfileDataCache.hasProfileDataForTesting(ACCOUNT.getEmail()));
        Assert.assertNull(
                mProfileDataCache.getProfileDataOrDefault(ACCOUNT.getEmail()).getFullName());

        mIdentityManager.onExtendedAccountInfoUpdated(accountWithFullName);

        Assert.assertTrue(mProfileDataCache.hasProfileDataForTesting(ACCOUNT.getEmail()));
        Assert.assertEquals(
                fullName,
                mProfileDataCache.getProfileDataOrDefault(ACCOUNT.getEmail()).getFullName());
    }

    @Test
    public void accountInfoIsUpdatedWithOnlyGivenName() {
        final String givenName = "given name1";
        final AccountInfo accountWithGivenName =
                new AccountInfo.Builder(ACCOUNT).givenName(givenName).build();
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertFalse(mProfileDataCache.hasProfileDataForTesting(ACCOUNT.getEmail()));
        Assert.assertNull(
                mProfileDataCache.getProfileDataOrDefault(ACCOUNT.getEmail()).getGivenName());

        mIdentityManager.onExtendedAccountInfoUpdated(accountWithGivenName);

        Assert.assertTrue(mProfileDataCache.hasProfileDataForTesting(ACCOUNT.getEmail()));
        Assert.assertEquals(
                givenName,
                mProfileDataCache.getProfileDataOrDefault(ACCOUNT.getEmail()).getGivenName());
    }

    @Test
    public void accountInfoIsUpdatedWithOnlyBadgeConfig() {
        mProfileDataCache.setBadge(R.drawable.ic_sync_badge_error_20dp);
        mProfileDataCache.addObserver(mObserverMock);
        Assert.assertFalse(mProfileDataCache.hasProfileDataForTesting(ACCOUNT.getEmail()));

        mIdentityManager.onExtendedAccountInfoUpdated(ACCOUNT);

        Assert.assertTrue(mProfileDataCache.hasProfileDataForTesting(ACCOUNT.getEmail()));
    }
}
