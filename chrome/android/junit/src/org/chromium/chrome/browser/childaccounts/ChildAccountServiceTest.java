// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.childaccounts;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.accounts.Account;
import android.app.Activity;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.Collections;
import java.util.List;

/**
 * Unit tests for {@link ChildAccountService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ChildAccountServiceTest {
    private static final Account CHILD_ACCOUNT1 =
            AccountUtils.createAccountFromName("child.account1@gmail.com");
    private static final Account CHILD_ACCOUNT2 =
            AccountUtils.createAccountFromName("child.account2@gmail.com");
    private static final Account ADULT_ACCOUNT1 =
            AccountUtils.createAccountFromName("adult.account1@gmail.com");
    private static final Account ADULT_ACCOUNT2 =
            AccountUtils.createAccountFromName("adult.account2@gmail.com");
    private static final long FAKE_NATIVE_CALLBACK = 1000L;

    private final FakeAccountManagerFacade mFakeFacade = spy(new FakeAccountManagerFacade() {
        @Override
        public void checkChildAccountStatus(Account account, ChildAccountStatusListener listener) {
            listener.onStatusReady(account.name.startsWith("child")
                            ? ChildAccountStatus.REGULAR_CHILD
                            : ChildAccountStatus.NOT_CHILD);
        }
    });

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeFacade);

    @Mock
    private ChildAccountStatusListener mListenerMock;

    @Mock
    private ChildAccountService.Natives mNativeMock;

    @Mock
    private WindowAndroid mWindowAndroidMock;

    @Before
    public void setUp() {
        mocker.mock(ChildAccountServiceJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    public void testChildAccountStatusWhenNoAccountsOnDevice() {
        ChildAccountService.checkChildAccountStatus(Collections.emptyList(), mListenerMock);
        verify(mListenerMock).onStatusReady(ChildAccountStatus.NOT_CHILD);
    }

    @Test
    public void testChildAccountStatusWhenTwoChildAccountsOnDevice() {
        // For product reason, child account cannot share device, so as long
        // as more than one account detected on device, the child account status
        // on device should be NOT_CHILD.
        ChildAccountService.checkChildAccountStatus(
                List.of(CHILD_ACCOUNT1, CHILD_ACCOUNT2), mListenerMock);
        verify(mListenerMock).onStatusReady(ChildAccountStatus.NOT_CHILD);
    }

    @Test
    public void testChildAccountStatusWhenOneChildAndOneAdultAccountsOnDevice() {
        ChildAccountService.checkChildAccountStatus(
                List.of(CHILD_ACCOUNT1, ADULT_ACCOUNT1), mListenerMock);
        verify(mListenerMock).onStatusReady(ChildAccountStatus.NOT_CHILD);
    }

    @Test
    public void testChildAccountStatusWhenTwoAdultAccountsOnDevice() {
        ChildAccountService.checkChildAccountStatus(
                List.of(ADULT_ACCOUNT1, ADULT_ACCOUNT2), mListenerMock);
        verify(mListenerMock).onStatusReady(ChildAccountStatus.NOT_CHILD);
    }

    @Test
    public void testChildAccountStatusWhenOnlyOneAdultAccountOnDevice() {
        ChildAccountService.checkChildAccountStatus(List.of(ADULT_ACCOUNT1), mListenerMock);
        verify(mListenerMock).onStatusReady(ChildAccountStatus.NOT_CHILD);
    }

    @Test
    public void testChildAccountStatusWhenOnlyOneChildAccountOnDevice() {
        ChildAccountService.checkChildAccountStatus(List.of(CHILD_ACCOUNT1), mListenerMock);
        verify(mListenerMock).onStatusReady(ChildAccountStatus.REGULAR_CHILD);
    }

    @Test
    public void testReauthenticateChildAccountWhenActivityIsNull() {
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(null));
        ChildAccountService.reauthenticateChildAccount(
                mWindowAndroidMock, CHILD_ACCOUNT1.name, FAKE_NATIVE_CALLBACK);
        verify(mNativeMock).onReauthenticationFailed(FAKE_NATIVE_CALLBACK);
    }

    @Test
    public void testReauthenticateChildAccountWhenReauthenticationSucceeded() {
        final Activity activity = mock(Activity.class);
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(activity));
        doAnswer(invocation -> {
            Account account = invocation.getArgument(0);
            Assert.assertEquals(CHILD_ACCOUNT1.name, account.name);
            Callback<Boolean> callback = invocation.getArgument(2);
            callback.onResult(true);
            return null;
        })
                .when(mFakeFacade)
                .updateCredentials(any(Account.class), eq(activity), any());

        ChildAccountService.reauthenticateChildAccount(
                mWindowAndroidMock, CHILD_ACCOUNT1.name, FAKE_NATIVE_CALLBACK);
        verify(mNativeMock, never()).onReauthenticationFailed(anyLong());
    }

    @Test
    public void testReauthenticateChildAccountWhenReauthenticationFailed() {
        final Activity activity = mock(Activity.class);
        when(mWindowAndroidMock.getActivity()).thenReturn(new WeakReference<>(activity));
        doAnswer(invocation -> {
            Account account = invocation.getArgument(0);
            Assert.assertEquals(CHILD_ACCOUNT1.name, account.name);
            Callback<Boolean> callback = invocation.getArgument(2);
            callback.onResult(false);
            return null;
        })
                .when(mFakeFacade)
                .updateCredentials(any(Account.class), eq(activity), any());

        ChildAccountService.reauthenticateChildAccount(
                mWindowAndroidMock, CHILD_ACCOUNT1.name, FAKE_NATIVE_CALLBACK);
        verify(mNativeMock).onReauthenticationFailed(FAKE_NATIVE_CALLBACK);
    }
}