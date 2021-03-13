// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.metrics.UmaRecorder;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.ui.base.WindowAndroid;

/**
 * JUnit tests for the class {@link SigninBridge}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {SigninBridgeTest.ShadowChromeFeatureList.class})
public class SigninBridgeTest {
    private static final String CONTINUE_URL = "https://test-continue-url.com";

    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        private static final String PARAM_NAME = "consecutive_active_dismissal_limit";
        static final int PARAM_VALUE = 3;

        @Implementation
        public static int getFieldTrialParamByFeatureAsInt(
                String featureName, String paramName, int defaultValue) {
            Assert.assertEquals(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY_VAR, featureName);
            Assert.assertEquals(PARAM_NAME, paramName);
            Assert.assertEquals(Integer.MAX_VALUE, defaultValue);
            return PARAM_VALUE;
        }
    }

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock
    private Profile mProfileMock;

    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private WindowAndroid mWindowAndroidMock;

    @Mock
    private UmaRecorder mUmaRecorderMock;

    @Before
    public void setUp() {
        initMocks(this);
        UmaRecorderHolder.setNonNativeDelegate(mUmaRecorderMock);
        Profile.setLastUsedProfileForTesting(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        when(mIdentityServicesProviderMock.getSigninManager(mProfileMock))
                .thenReturn(mSigninManagerMock);
    }

    @After
    public void tearDown() {
        SigninPreferencesManager.getInstance().clearAccountPickerBottomSheetActiveDismissalCount();
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenSigninNotAllowed() {
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(false);
        SigninBridge.openAccountPickerBottomSheet(mWindowAndroidMock, CONTINUE_URL);
        checkHistogramRecording(AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED);
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenNoAccountsOnDevice() {
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(true);
        SigninBridge.openAccountPickerBottomSheet(mWindowAndroidMock, CONTINUE_URL);
        checkHistogramRecording(AccountConsistencyPromoAction.SUPPRESSED_NO_ACCOUNTS);
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedIfDismissLimitReached() {
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount("account@test.com");
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.ACCOUNT_PICKER_BOTTOM_SHEET_ACTIVE_DISMISSAL_COUNT,
                ShadowChromeFeatureList.PARAM_VALUE);
        SigninBridge.openAccountPickerBottomSheet(mWindowAndroidMock, CONTINUE_URL);
        checkHistogramRecording(AccountConsistencyPromoAction.SUPPRESSED_CONSECUTIVE_DISMISSALS);
    }

    private void checkHistogramRecording(@AccountConsistencyPromoAction int action) {
        verify(mUmaRecorderMock)
                .recordLinearHistogram("Signin.AccountConsistencyPromoAction", action, 1,
                        AccountConsistencyPromoAction.MAX, AccountConsistencyPromoAction.MAX + 1);
    }
}
