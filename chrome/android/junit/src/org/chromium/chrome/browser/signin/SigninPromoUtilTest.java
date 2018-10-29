// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.Mockito.ignoreStubs;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.support.v4.util.ArraySet;

import com.google.common.collect.ImmutableSet;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;

import java.util.Arrays;
import java.util.Set;

/** Tests for {@link SigninPromoUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SigninPromoUtilTest {
    private ChromePreferenceManager mPreferenceManager;

    @Before
    public void setUp() {
        mPreferenceManager = Mockito.mock(ChromePreferenceManager.class);
        // Return default value if last account names are requested
        when(mPreferenceManager.getSigninPromoLastAccountNames()).thenReturn(null);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(ignoreStubs(mPreferenceManager));
    }

    /**
     * Returns account list supplier that throws unchecked exception, causing the test to fail. This
     * is helpful to ensure that other signals for {@link SigninPromoUtil#shouldLaunchSigninPromo}
     * are checked before getting account list (as account list may not be ready yet).
     */
    private Supplier<Set<String>> shouldNotTryToGetAccounts() {
        return () -> {
            throw new RuntimeException("Should not try to get accounts!");
        };
    }

    /**
     * Creates a {@link Supplier} that returns a list of accounts provided to this method.
     * @param accountNames The account names to return from {@link Supplier}
     */
    private Supplier<Set<String>> accountsSupplier(String... accountNames) {
        return () -> new ArraySet<>(Arrays.asList(accountNames));
    }

    @Test
    public void whenNoLastShownVersionShouldReturnFalseAndSaveVersion() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(0);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, false, shouldNotTryToGetAccounts()));
        verify(mPreferenceManager).setSigninPromoLastShownVersion(42);
    }

    @Test
    public void whenSignedInShouldReturnFalse() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(38);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, true, false, shouldNotTryToGetAccounts()));
    }

    @Test
    public void whenWasSignedInShouldReturnFalse() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(38);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, true, shouldNotTryToGetAccounts()));
    }

    @Test
    public void whenVersionDifferenceTooSmallShouldReturnFalse() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(41);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, false, shouldNotTryToGetAccounts()));
    }

    @Test
    public void whenNoAccountsShouldReturnFalse() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(38);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, false, accountsSupplier()));
    }

    @Test
    public void whenNoAccountListStoredShouldReturnTrue() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(40);
        // Old implementation hasn't been storing account list
        when(mPreferenceManager.getSigninPromoLastAccountNames()).thenReturn(null);
        Assert.assertTrue(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, false, accountsSupplier("test@gmail.com")));
    }

    @Test
    public void whenHasNewAccountShouldReturnTrue() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(40);
        when(mPreferenceManager.getSigninPromoLastAccountNames())
                .thenReturn(ImmutableSet.of("test@gmail.com"));
        Assert.assertTrue(SigninPromoUtil.shouldLaunchSigninPromo(mPreferenceManager, 42, false,
                false, accountsSupplier("test@gmail.com", "test2@gmail.com")));
    }

    @Test
    public void whenAccountListUnchangedShouldReturnFalse() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(40);
        when(mPreferenceManager.getSigninPromoLastAccountNames())
                .thenReturn(ImmutableSet.of("test@gmail.com"));
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, false, accountsSupplier("test@gmail.com")));
    }

    @Test
    public void whenNoNewAccountsShouldReturnFalse() {
        when(mPreferenceManager.getSigninPromoLastShownVersion()).thenReturn(40);
        when(mPreferenceManager.getSigninPromoLastAccountNames())
                .thenReturn(ImmutableSet.of("test@gmail.com", "test2@gmail.com"));
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferenceManager, 42, false, false, accountsSupplier("test2@gmail.com")));
    }
}
