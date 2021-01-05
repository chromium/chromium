// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.MockChangeEventChecker;

/**
 * Unit tests for {@link SigninHelper}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninHelperTest {
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private final MockChangeEventChecker mEventChecker = new MockChangeEventChecker();

    @After
    public void tearDown() {
        SigninPreferencesManager.getInstance().clearAccountsStateSharedPrefsForTesting();
    }

    @Test
    public void testSimpleAccountRename() {
        mEventChecker.insertRenameEvent("A", "B");
        SigninHelper.updateAccountRenameData(mEventChecker, "A");
        Assert.assertEquals("B", getNewSignedInAccountName());
        checkLastAccountChangedEventIndex(0);
    }

    @Test
    public void testNotSignedInAccountRename() {
        mEventChecker.insertRenameEvent("B", "C");
        SigninHelper.updateAccountRenameData(mEventChecker, "A");
        Assert.assertNull(getNewSignedInAccountName());
        checkLastAccountChangedEventIndex(0);
    }

    @Test
    public void testSimpleAccountRenameTwice() {
        mEventChecker.insertRenameEvent("A", "B");
        SigninHelper.updateAccountRenameData(mEventChecker, "A");
        Assert.assertEquals("B", getNewSignedInAccountName());
        checkLastAccountChangedEventIndex(0);
        mEventChecker.insertRenameEvent("B", "C");
        SigninHelper.updateAccountRenameData(mEventChecker, "B");
        Assert.assertEquals("C", getNewSignedInAccountName());
        checkLastAccountChangedEventIndex(0);
    }

    @Test
    public void testNotSignedInAccountRename2() {
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        SigninHelper.updateAccountRenameData(mEventChecker, "A");
        Assert.assertNull(getNewSignedInAccountName());
        checkLastAccountChangedEventIndex(0);
    }

    @Test
    public void testChainedAccountRename2() {
        mEventChecker.insertRenameEvent("Z", "Y"); // Unrelated.
        mEventChecker.insertRenameEvent("A", "B");
        mEventChecker.insertRenameEvent("Y", "X"); // Unrelated.
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        SigninHelper.updateAccountRenameData(mEventChecker, "A");
        Assert.assertEquals("D", getNewSignedInAccountName());
        checkLastAccountChangedEventIndex(0);
    }

    @Test
    public void testLoopedAccountRename() {
        mEventChecker.insertRenameEvent("Z", "Y"); // Unrelated.
        mEventChecker.insertRenameEvent("A", "B");
        mEventChecker.insertRenameEvent("Y", "X"); // Unrelated.
        mEventChecker.insertRenameEvent("B", "C");
        mEventChecker.insertRenameEvent("C", "D");
        mEventChecker.insertRenameEvent("D", "A"); // Looped.
        mAccountManagerTestRule.addAccount("D");
        SigninHelper.updateAccountRenameData(mEventChecker, "A");
        Assert.assertEquals("D", getNewSignedInAccountName());
        checkLastAccountChangedEventIndex(1);
    }

    private String getNewSignedInAccountName() {
        return SigninPreferencesManager.getInstance().getNewSignedInAccountName();
    }

    private void checkLastAccountChangedEventIndex(int expectedEventIndex) {
        Assert.assertEquals(expectedEventIndex,
                SigninPreferencesManager.getInstance().getLastAccountChangedEventIndex());
    }
}
