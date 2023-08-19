// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;

/**
 * JUnit tests for the class {@link SigninBridge}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninBridgeTest {
    private static final String CONTINUE_URL = "https://test-continue-url.com";

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private Profile mProfileMock;

    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private WindowAndroid mWindowAndroidMock;

    @Mock
    private SigninMetricsUtils.Natives mSigninMetricsUtilsJniMock;

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        when(mIdentityServicesProviderMock.getSigninManager(mProfileMock))
                .thenReturn(mSigninManagerMock);
        mJniMocker.mock(SigninMetricsUtilsJni.TEST_HOOKS, mSigninMetricsUtilsJniMock);
    }

    @After
    public void tearDown() {
        SigninPreferencesManager.getInstance().clearWebSigninAccountPickerActiveDismissalCount();
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenSigninNotAllowed() {
        when(mSigninManagerMock.isSyncOptInAllowed()).thenReturn(false);
        SigninBridge.openAccountPickerBottomSheet(mWindowAndroidMock, CONTINUE_URL);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED,
                        SigninAccessPoint.WEB_SIGNIN);
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenNoAccountsOnDevice() {
        when(mSigninManagerMock.isSyncOptInAllowed()).thenReturn(true);
        SigninBridge.openAccountPickerBottomSheet(mWindowAndroidMock, CONTINUE_URL);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.SUPPRESSED_NO_ACCOUNTS,
                        SigninAccessPoint.WEB_SIGNIN);
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedIfDismissLimitReached() {
        when(mSigninManagerMock.isSyncOptInAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount("account@test.com");
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT,
                SigninBridge.ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT);
        SigninBridge.openAccountPickerBottomSheet(mWindowAndroidMock, CONTINUE_URL);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.SUPPRESSED_CONSECUTIVE_DISMISSALS,
                        SigninAccessPoint.WEB_SIGNIN);
    }
}
