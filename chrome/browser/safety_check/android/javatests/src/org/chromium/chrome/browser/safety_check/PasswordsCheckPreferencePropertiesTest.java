// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordCheckResult;
import org.chromium.chrome.browser.safety_check.PasswordsCheckPreferenceProperties.PasswordsState;

import java.util.Arrays;
import java.util.Collection;

/** Unit tests for {@link PasswordsCheckPreferenceProperties}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasswordsCheckPreferencePropertiesTest {
    @Parameters
    public static Collection<Object[]> testCases() {
        return Arrays.asList(
                new Object[][] {
                    // Password check result: 10 credentials, 0 breached.
                    // Expected result: PasswordsState.SAFE.
                    {new PasswordCheckResult(10, 0), PasswordsState.SAFE},
                    // Password check result: 0 credentials, 0 breached.
                    // Expected result: PasswordsState.NO_PASSWORDS.
                    {new PasswordCheckResult(0, 0), PasswordsState.NO_PASSWORDS},
                    // Password check result: 10 credentials, 1 breached.
                    // Expected result: PasswordsState.COMPROMISED_EXIST.
                    {new PasswordCheckResult(10, 1), PasswordsState.COMPROMISED_EXIST},
                    // Password check result: some unknown error set.
                    // Expected result: PasswordsState.ERROR.
                    {
                        new PasswordCheckResult(
                                new Exception("Simulate that password check throws an exception")),
                        PasswordsState.ERROR
                    },
                });
    }

    private final PasswordCheckResult mPasswordCheckResult;
    private final @PasswordsState int mExpectedPasswordsState;

    public PasswordsCheckPreferencePropertiesTest(
            PasswordCheckResult passwordCheckResult, @PasswordsState int expectedPasswordsState) {
        mPasswordCheckResult = passwordCheckResult;
        mExpectedPasswordsState = expectedPasswordsState;
    }

    @Test
    public void testPasswordsStateFromPasswordCheckResult() {
        Assert.assertEquals(
                mExpectedPasswordsState,
                PasswordsCheckPreferenceProperties.passwordsStateFromPasswordCheckResult(
                        mPasswordCheckResult));
    }
}
