// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.accounts.Account;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

/**
 * JUnit tests for {@link ForcedSigninProcessor}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {CustomShadowAsyncTask.class})
public class ForcedSigninProcessorTest {
    private static final Account CHILD_ACCOUNT =
            AccountUtils.createAccountFromName("child.account@gmail.com");

    private final FakeAccountManagerFacade mFakeFacade = new FakeAccountManagerFacade(null) {
        @Override
        public void checkChildAccountStatus(Account account, ChildAccountStatusListener listener) {
            listener.onStatusReady(account.equals(CHILD_ACCOUNT) ? ChildAccountStatus.REGULAR_CHILD
                                                                 : ChildAccountStatus.NOT_CHILD);
        }
    };

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeFacade);

    @Mock
    private Profile mProfileMock;

    @Mock
    private ProfileSyncService mProfileSyncServiceMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private IdentityManager mIdentityManagerMock;

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mProfileMock);
        ProfileSyncService.overrideForTests(mProfileSyncServiceMock);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));

        when(IdentityServicesProvider.get().getIdentityManager(mProfileMock))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount()).thenReturn(false);

        when(IdentityServicesProvider.get().getSigninManager(mProfileMock))
                .thenReturn(mSigninManagerMock);
        doAnswer(invocation -> {
            SignInCallback callback = invocation.getArgument(2);
            callback.onSignInComplete();
            return null;
        })
                .when(mSigninManagerMock)
                .signinAndEnableSync(anyInt(), any(CoreAccountInfo.class), notNull());
    }

    @Test
    public void testStartWhenMoreThanOneAccountsOnDevice() {
        mAccountManagerTestRule.addAccount(CHILD_ACCOUNT);
        mAccountManagerTestRule.addAccount("adult.account@gmail.com");

        ForcedSigninProcessor.start();
        verify(mSigninManagerMock, never()).onFirstRunCheckDone();
    }

    @Test
    public void testStartWhenAdultAccountOnDevice() {
        mAccountManagerTestRule.addAccount("adult.account@gmail.com");

        ForcedSigninProcessor.start();
        verify(mSigninManagerMock, never()).onFirstRunCheckDone();
    }

    @Test
    public void testStartWhenSigninNotAllowed() {
        mAccountManagerTestRule.addAccount(CHILD_ACCOUNT);
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(false);

        ForcedSigninProcessor.start();
        verify(mSigninManagerMock).onFirstRunCheckDone();
        verify(mSigninManagerMock, never())
                .signinAndEnableSync(anyInt(), any(CoreAccountInfo.class), any());
        verify(mProfileSyncServiceMock, never())
                .setFirstSetupComplete(SyncFirstSetupCompleteSource.BASIC_FLOW);
    }

    @Test
    public void testStartWhenSigninAllowed() {
        CoreAccountInfo childAccount = mAccountManagerTestRule.addAccount(CHILD_ACCOUNT);
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(true);

        ForcedSigninProcessor.start();
        verify(mSigninManagerMock).onFirstRunCheckDone();
        verify(mSigninManagerMock)
                .signinAndEnableSync(
                        eq(SigninAccessPoint.FORCED_SIGNIN), eq(childAccount), notNull());
        verify(mProfileSyncServiceMock)
                .setFirstSetupComplete(SyncFirstSetupCompleteSource.BASIC_FLOW);
    }
}
