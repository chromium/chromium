// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.accounts.Account;
import android.app.Activity;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.WebSigninBridge;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * This class tests the {@link AccountPickerDelegate}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountPickerDelegateTest {
    private static final String CONTINUE_URL = "https://test-continue-url.com";

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock
    private WebSigninBridge.Factory mWebSigninBridgeFactoryMock;

    @Mock
    private WebSigninBridge mWebSigninBridgeMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private Profile mProfileMock;

    @Mock
    private WindowAndroid mWindowAndroidMock;

    @Mock(answer = Answers.RETURNS_DEEP_STUBS)
    private ChromeActivity mChromeActivityMock;

    @Captor
    private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    private AccountPickerDelegate mDelegate;

    private final IdentityManager mIdentityManager =
            new IdentityManager(/* nativeIdentityManager= */ 0, /* OAuth2TokenService= */ null);

    @Before
    public void setUp() {
        initMocks(this);
        Profile.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(any())).thenReturn(mIdentityManager);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);

        when(mWindowAndroidMock.getActivity())
                .thenReturn(new WeakReference<Activity>(mChromeActivityMock));
        mDelegate = new AccountPickerDelegate(
                mWindowAndroidMock, mWebSigninBridgeFactoryMock, CONTINUE_URL);
        when(mWebSigninBridgeFactoryMock.create(eq(mProfileMock), any(), eq(mDelegate)))
                .thenReturn(mWebSigninBridgeMock);
    }

    @After
    public void tearDown() {
        mDelegate.onDismiss();
    }

    @Test
    public void testSignInSucceeded() {
        Account account =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        CoreAccountInfo coreAccountInfo = mAccountManagerTestRule.toCoreAccountInfo(account.name);
        mDelegate.signIn(coreAccountInfo, error -> {});
        InOrder calledInOrder = inOrder(mWebSigninBridgeFactoryMock, mSigninManagerMock);
        calledInOrder.verify(mWebSigninBridgeFactoryMock)
                .create(mProfileMock, coreAccountInfo, mDelegate);
        calledInOrder.verify(mSigninManagerMock)
                .signinAndEnableSync(eq(SigninAccessPoint.WEB_SIGNIN), eq(coreAccountInfo), any());
        mDelegate.onSigninSucceded();
        verify(mChromeActivityMock.getActivityTab()).loadUrl(mLoadUrlParamsCaptor.capture());
        LoadUrlParams loadUrlParams = mLoadUrlParamsCaptor.getValue();
        Assert.assertEquals("Continue url does not match!", CONTINUE_URL, loadUrlParams.getUrl());
    }

    @Test
    public void testSignInAborted() {
        Account account =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        CoreAccountInfo coreAccountInfo = mAccountManagerTestRule.toCoreAccountInfo(account.name);
        doAnswer(invocation -> {
            SigninManager.SignInCallback callback = invocation.getArgument(2);
            callback.onSignInAborted();
            return null;
        })
                .when(mSigninManagerMock)
                .signinAndEnableSync(eq(SigninAccessPoint.WEB_SIGNIN), eq(coreAccountInfo), any());
        mDelegate.signIn(coreAccountInfo, error -> {});
        verify(mWebSigninBridgeMock).destroy();
    }

    @Test
    public void testSignInFailedWithConnectionError() {
        Account account =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        CoreAccountInfo coreAccountInfo = mAccountManagerTestRule.toCoreAccountInfo(account.name);
        Callback<GoogleServiceAuthError> mockCallback = mock(Callback.class);
        GoogleServiceAuthError error = new GoogleServiceAuthError(State.CONNECTION_FAILED);
        mDelegate.signIn(coreAccountInfo, mockCallback);
        mDelegate.onSigninFailed(error);
        verify(mockCallback).onResult(error);
        verify(mWebSigninBridgeMock).destroy();
    }
}
