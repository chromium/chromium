// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.metrics.UmaRecorder;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class tests the method {@link SigninUtils#openAccountPickerBottomSheet}
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninUtilsAccountPickerTest {
    private static final String CONTINUE_URL = "https://test-continue-url.com";

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

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenSigninNotAllowed() {
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(false);
        SigninUtils.openAccountPickerBottomSheet(mWindowAndroidMock, CONTINUE_URL);
        checkHistogramRecording(AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED);
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenNoAccountsOnDevice() {
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(true);
        SigninUtils.openAccountPickerBottomSheet(mWindowAndroidMock, CONTINUE_URL);
        checkHistogramRecording(AccountConsistencyPromoAction.SUPPRESSED_NO_ACCOUNTS);
    }

    private void checkHistogramRecording(@AccountConsistencyPromoAction int action) {
        verify(mUmaRecorderMock)
                .recordLinearHistogram("Signin.AccountConsistencyPromoAction", action, 1,
                        AccountConsistencyPromoAction.MAX, AccountConsistencyPromoAction.MAX + 1);
    }
}
