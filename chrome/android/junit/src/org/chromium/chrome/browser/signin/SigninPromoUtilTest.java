// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.Mockito.ignoreStubs;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import androidx.collection.ArraySet;

import com.google.common.collect.ImmutableSet;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;

import java.util.Arrays;
import java.util.Collections;
import java.util.Set;

/** Tests for {@link SigninPromoUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SigninPromoUtilTest {
    private SigninPreferencesManager mPreferencesManager;

    @Before
    public void setUp() {
        mPreferencesManager = Mockito.mock(SigninPreferencesManager.class);
        // Return default value if last account names are requested
        when(mPreferencesManager.getSigninPromoLastAccountNames()).thenReturn(null);
    }

    @After
    public void tearDown() {
        verifyNoMoreInteractions(ignoreStubs(mPreferencesManager));
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
        when(mPreferencesManager.getSigninPromoLastShownVersion()).thenReturn(0);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferencesManager, 42, false, false, ImmutableSet.of("test@gmail.com")));
        verify(mPreferencesManager).setSigninPromoLastShownVersion(42);
    }

    @Test
    public void whenSignedInShouldReturnFalse() {
        when(mPreferencesManager.getSigninPromoLastShownVersion()).thenReturn(38);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferencesManager, 42, true, false, ImmutableSet.of("test@gmail.com")));
    }

    @Test
    public void whenWasSignedInShouldReturnFalse() {
        when(mPreferencesManager.getSigninPromoLastShownVersion()).thenReturn(38);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferencesManager, 42, false, true, ImmutableSet.of("test@gmail.com")));
    }

    @Test
    public void whenVersionDifferenceTooSmallShouldReturnFalse() {
        when(mPreferencesManager.getSigninPromoLastShownVersion()).thenReturn(41);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferencesManager, 42, false, false, ImmutableSet.of("test@gmail.com")));
    }

    @Test
    public void whenNoAccountsShouldReturnFalse() {
        when(mPreferencesManager.getSigninPromoLastShownVersion()).thenReturn(38);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferencesManager, 42, false, false, Collections.emptySet()));
    }

    @Test
    public void whenNoAccountListStoredShouldReturnTrue() {
        when(mPreferencesManager.getSigninPromoLastShownVersion()).thenReturn(40);
        // Old implementation hasn't been storing account list
        when(mPreferencesManager.getSigninPromoLastAccountNames()).thenReturn(null);
        Assert.assertTrue(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferencesManager, 42, false, false, ImmutableSet.of("test@gmail.com")));
    }

    @Test
    public void whenHasNewAccountShouldReturnTrue() {
        when(mPreferencesManager.getSigninPromoLastShownVersion()).thenReturn(40);
        when(mPreferencesManager.getSigninPromoLastAccountNames())
                .thenReturn(ImmutableSet.of("test@gmail.com"));
        Assert.assertTrue(SigninPromoUtil.shouldLaunchSigninPromo(mPreferencesManager, 42, false,
                false, ImmutableSet.of("test@gmail.com", "test2@gmail.com")));
    }

    @Test
    public void whenAccountListUnchangedShouldReturnFalse() {
        when(mPreferencesManager.getSigninPromoLastShownVersion()).thenReturn(40);
        when(mPreferencesManager.getSigninPromoLastAccountNames())
                .thenReturn(ImmutableSet.of("test@gmail.com"));
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferencesManager, 42, false, false, ImmutableSet.of("test@gmail.com")));
    }

    @Test
    public void whenNoNewAccountsShouldReturnFalse() {
        when(mPreferencesManager.getSigninPromoLastShownVersion()).thenReturn(40);
        when(mPreferencesManager.getSigninPromoLastAccountNames())
                .thenReturn(ImmutableSet.of("test@gmail.com", "test2@gmail.com"));
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(
                mPreferencesManager, 42, false, false, ImmutableSet.of("test2@gmail.com")));
    }
}
