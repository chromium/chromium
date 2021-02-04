// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import androidx.fragment.app.FragmentActivity;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * This class tests the {@link AccountPickerDelegateImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountPickerDelegateImplTest {
    private static final String CONTINUE_URL = "https://test-continue-url.com";

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade(null));

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Mock
    private WebSigninBridge.Factory mWebSigninBridgeFactoryMock;

    @Mock
    private WebSigninBridge mWebSigninBridgeMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private IdentityManager mIdentityManagerMock;

    @Mock
    private Profile mProfileMock;

    @Mock
    private WindowAndroid mWindowAndroidMock;

    @Mock
    private Tab mTabMock;

    @Captor
    private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    private FragmentActivity mActivity;

    private AccountPickerDelegateImpl mDelegate;

    private CoreAccountInfo mCoreAccountInfo;

    @Before
    public void setUp() {
        initMocks(this);
        mActivity = Robolectric.setupActivity(FragmentActivity.class);
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(mActivity));

        Profile.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);

        mCoreAccountInfo =
                mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);

        mDelegate = new AccountPickerDelegateImpl(
                mWindowAndroidMock, mTabMock, mWebSigninBridgeFactoryMock, CONTINUE_URL);
        when(mWebSigninBridgeFactoryMock.create(eq(mProfileMock), any(), eq(mDelegate)))
                .thenReturn(mWebSigninBridgeMock);
    }

    @After
    public void tearDown() {
        mDelegate.onDismiss();
    }

    @Test
    public void testSignInSucceeded() {
        mDelegate.signIn(mCoreAccountInfo, error -> {});
        InOrder calledInOrder = inOrder(mWebSigninBridgeFactoryMock, mSigninManagerMock);
        calledInOrder.verify(mWebSigninBridgeFactoryMock)
                .create(mProfileMock, mCoreAccountInfo, mDelegate);
        calledInOrder.verify(mSigninManagerMock).signin(eq(mCoreAccountInfo), any());
        mDelegate.onSigninSucceeded();
        verify(mTabMock).loadUrl(mLoadUrlParamsCaptor.capture());
        LoadUrlParams loadUrlParams = mLoadUrlParamsCaptor.getValue();
        Assert.assertEquals("Continue url does not match!", CONTINUE_URL, loadUrlParams.getUrl());
    }

    @Test
    public void testSignInAborted() {
        doAnswer(invocation -> {
            SigninManager.SignInCallback callback = invocation.getArgument(1);
            callback.onSignInAborted();
            return null;
        })
                .when(mSigninManagerMock)
                .signin(eq(mCoreAccountInfo), any());
        mDelegate.signIn(mCoreAccountInfo, error -> {});
        verify(mWebSigninBridgeMock).destroy();
    }

    @Test
    public void testSigninTriggersSignoutIfAlreadySignedIn() {
        // In case an error is fired because cookies are taking longer to generate than usual,
        // if user retries the sign-in from the error screen, we need to sign out the user
        // first before signing in again.
        mDelegate.signIn(mCoreAccountInfo, error -> {});
        when(mIdentityManagerMock.getPrimaryAccountInfo(anyInt())).thenReturn(mCoreAccountInfo);

        mDelegate.signIn(mCoreAccountInfo, error -> {});
        InOrder calledInOrder = inOrder(mWebSigninBridgeMock, mSigninManagerMock,
                mWebSigninBridgeFactoryMock, mSigninManagerMock);
        calledInOrder.verify(mWebSigninBridgeMock).destroy();
        calledInOrder.verify(mSigninManagerMock).signOut(anyInt());
        calledInOrder.verify(mWebSigninBridgeFactoryMock)
                .create(mProfileMock, mCoreAccountInfo, mDelegate);
        calledInOrder.verify(mSigninManagerMock).signin(eq(mCoreAccountInfo), any());
    }

    @Test
    public void testSignInFailedWithConnectionError() {
        Callback<GoogleServiceAuthError> mockCallback = mock(Callback.class);
        GoogleServiceAuthError error = new GoogleServiceAuthError(State.CONNECTION_FAILED);
        mDelegate.signIn(mCoreAccountInfo, mockCallback);
        mDelegate.onSigninFailed(error);
        verify(mockCallback).onResult(error);
        // WebSigninBridge should be kept alive in case cookies are taking longer to
        // generate than usual
        verify(mWebSigninBridgeMock, never()).destroy();
    }

    @Test
    public void testUpdateCredentials() {
        Callback<Boolean> callback = (isSuccess) -> {};
        mDelegate.updateCredentials(AccountManagerTestRule.TEST_ACCOUNT_EMAIL, callback);
        verify(mFakeAccountManagerFacade)
                .updateCredentials(AccountUtils.createAccountFromName(
                                           AccountManagerTestRule.TEST_ACCOUNT_EMAIL),
                        mActivity, callback);
    }
}
