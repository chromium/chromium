// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.childaccounts;

import static org.mockito.Mockito.verify;

import android.accounts.Account;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

/**
 * Unit tests for {@link ChildAccountService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ChildAccountServiceTest {
    private static final Account CHILD_ACCOUNT =
            AccountUtils.createAccountFromName("child.account@gmail.com");

    private final FakeAccountManagerFacade mFakeFacade = new FakeAccountManagerFacade(null) {
        @Override
        public void checkChildAccountStatus(Account account, ChildAccountStatusListener listener) {
            listener.onStatusReady(account.name.startsWith("child")
                            ? ChildAccountStatus.REGULAR_CHILD
                            : ChildAccountStatus.NOT_CHILD);
        }
    };

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeFacade);

    @Mock
    private ChildAccountStatusListener mListenerMock;

    @Test
    public void testChildAccountStatusWhenNoAccountsOnDevice() {
        ChildAccountService.checkChildAccountStatus(mListenerMock);
        verify(mListenerMock).onStatusReady(ChildAccountStatus.NOT_CHILD);
    }

    @Test
    public void testChildAccountStatusWhenTwoChildAccountsOnDevice() {
        // For product reason, child account cannot share device, so as long
        // as more than one account detected on device, the child account status
        // on device should be NOT_CHILD.
        mAccountManagerTestRule.addAccount(CHILD_ACCOUNT);
        mAccountManagerTestRule.addAccount("child.account2@gmail.com");
        ChildAccountService.checkChildAccountStatus(mListenerMock);
        verify(mListenerMock).onStatusReady(ChildAccountStatus.NOT_CHILD);
    }

    @Test
    public void testChildAccountStatusWhenOneChildAndOneAdultAccountsOnDevice() {
        mAccountManagerTestRule.addAccount(CHILD_ACCOUNT);
        mAccountManagerTestRule.addAccount("adult.account@gmail.com");
        ChildAccountService.checkChildAccountStatus(mListenerMock);
        verify(mListenerMock).onStatusReady(ChildAccountStatus.NOT_CHILD);
    }

    @Test
    public void testChildAccountStatusWhenTwoAdultAccountsOnDevice() {
        mAccountManagerTestRule.addAccount("adult.account1@gmail.com");
        mAccountManagerTestRule.addAccount("adult.account2@gmail.com");
        ChildAccountService.checkChildAccountStatus(mListenerMock);
        verify(mListenerMock).onStatusReady(ChildAccountStatus.NOT_CHILD);
    }

    @Test
    public void testChildAccountStatusWhenOnlyOneAdultAccountOnDevice() {
        mAccountManagerTestRule.addAccount("adult.account1@gmail.com");
        ChildAccountService.checkChildAccountStatus(mListenerMock);
        verify(mListenerMock).onStatusReady(ChildAccountStatus.NOT_CHILD);
    }

    @Test
    public void testChildAccountStatusWhenOnlyOneChildAccountOnDevice() {
        mAccountManagerTestRule.addAccount(CHILD_ACCOUNT);
        ChildAccountService.checkChildAccountStatus(mListenerMock);
        verify(mListenerMock).onStatusReady(ChildAccountStatus.REGULAR_CHILD);
    }
}