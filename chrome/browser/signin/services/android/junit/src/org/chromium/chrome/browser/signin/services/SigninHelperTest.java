// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.accounts.Account;
import android.content.Context;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.signin.AccountUtils;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Unit tests for {@link SigninHelper}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninHelperTest {
    private static final class MockChangeEventChecker
            implements SigninHelper.AccountChangeEventChecker {
        private final Map<String, String> mEvents = new HashMap<>();

        @Override
        public String getNewNameOfRenamedAccount(Context context, String accountEmail) {
            return mEvents.get(accountEmail);
        }

        void insertRenameEvent(String from, String to) {
            mEvents.put(from, to);
        }
    }

    private final MockChangeEventChecker mEventChecker = new MockChangeEventChecker();

    @Test
    public void newNameIsValidWhenTheRenamedAccountIsPresent() {
        mEventChecker.insertRenameEvent("A", "B");

        Assert.assertEquals(
                "B", SigninHelper.getNewNameOfRenamedAccount(mEventChecker, "A", getAccounts("B")));
    }

    @Test
    public void newNameIsNullWhenTheOldAccountIsNotRenamed() {
        mEventChecker.insertRenameEvent("B", "C");

        Assert.assertNull(
                SigninHelper.getNewNameOfRenamedAccount(mEventChecker, "A", getAccounts("D")));
    }

    @Test
    public void newNameIsNullWhenTheRenamedAccountIsNotPresent() {
        mEventChecker.insertRenameEvent("B", "C");

        Assert.assertNull(
                SigninHelper.getNewNameOfRenamedAccount(mEventChecker, "B", getAccounts("D")));
    }

    @Test
    public void newNameIsValidWhenTheOldAccountIsRenamedTwice() {
        mEventChecker.insertRenameEvent("A", "B");
        mEventChecker.insertRenameEvent("B", "C");

        Assert.assertEquals(
                "C", SigninHelper.getNewNameOfRenamedAccount(mEventChecker, "A", getAccounts("C")));
    }

    @Test
    public void newNameIsValidWhenTheOldAccountIsRenamedMultipleTimes() {
        // A -> B -> C
        mEventChecker.insertRenameEvent("Z", "Y"); // Unrelated.
        mEventChecker.insertRenameEvent("A", "B");
        mEventChecker.insertRenameEvent("Y", "X"); // Unrelated.
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");

        Assert.assertEquals(
                "D", SigninHelper.getNewNameOfRenamedAccount(mEventChecker, "A", getAccounts("D")));
    }

    @Test
    public void newNameIsValidWhenTheOldAccountIsRenamedInCycle() {
        // A -> B -> C -> D -> A
        mEventChecker.insertRenameEvent("Z", "Y"); // Unrelated.
        mEventChecker.insertRenameEvent("A", "B");
        mEventChecker.insertRenameEvent("Y", "X"); // Unrelated.
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        mEventChecker.insertRenameEvent("D", "A"); // Looped.

        Assert.assertEquals("D",
                SigninHelper.getNewNameOfRenamedAccount(mEventChecker, "A", getAccounts("D", "X")));
    }

    private List<Account> getAccounts(String... names) {
        final List<Account> accounts = new ArrayList<>();
        for (String name : names) {
            accounts.add(AccountUtils.createAccountFromName(name));
        }
        return accounts;
    }
}
