// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;

/** JUnit tests for the class {@link SigninBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {SigninBridgeTest.ShadowBottomSheetControllerProvider.class})
public class SigninBridgeTest {
    /** The shadow of BottomSheetControllerProvider. */
    @Implements(BottomSheetControllerProvider.class)
    static class ShadowBottomSheetControllerProvider {
        private static BottomSheetController sBottomSheetController;

        @Implementation
        public static BottomSheetController from(WindowAndroid windowAndroid) {
            return sBottomSheetController;
        }

        private static void setBottomSheetController(BottomSheetController controller) {
            sBottomSheetController = controller;
        }
    }

    private static final String CONTINUE_URL = "https://test-continue-url.com";

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Tab mTabMock;

    @Mock private Profile mProfileMock;

    @Mock private WindowAndroid mWindowAndroidMock;

    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock private SigninManager mSigninManagerMock;

    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsJniMock;

    @Mock private BottomSheetController mBottomSheetControllerMock;

    @Mock
    private SigninBridge.AccountPickerBottomSheetCoordinatorFactory
            mAccountPickerBottomSheetCoordinatorFactoryMock;

    @Before
    public void setUp() {
        ShadowBottomSheetControllerProvider.setBottomSheetController(mBottomSheetControllerMock);

        lenient().when(mTabMock.getProfile()).thenReturn(mProfileMock);
        lenient().when(mTabMock.getWindowAndroid()).thenReturn(mWindowAndroidMock);
        lenient().when(mTabMock.isUserInteractable()).thenReturn(true);

        lenient().when(mProfileMock.getOriginalProfile()).thenReturn(mProfileMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        lenient()
                .when(mIdentityServicesProviderMock.getSigninManager(mProfileMock))
                .thenReturn(mSigninManagerMock);
        mJniMocker.mock(SigninMetricsUtilsJni.TEST_HOOKS, mSigninMetricsUtilsJniMock);
    }

    @After
    public void tearDown() {
        SigninPreferencesManager.getInstance().clearWebSigninAccountPickerActiveDismissalCount();
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenNoWindow() {
        //  Reset default values configured in `setUp`.
        Mockito.reset(mTabMock);
        when(mTabMock.getWindowAndroid()).thenReturn(null);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock, CONTINUE_URL, mAccountPickerBottomSheetCoordinatorFactoryMock);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(any(), any(), any(), any(), any(), anyInt());
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenTabNotInteractable() {
        //  Reset default values configured in `setUp`.
        Mockito.reset(mTabMock);
        when(mTabMock.getWindowAndroid()).thenReturn(mWindowAndroidMock);
        when(mTabMock.isUserInteractable()).thenReturn(false);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock, CONTINUE_URL, mAccountPickerBottomSheetCoordinatorFactoryMock);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(any(), any(), any(), any(), any(), anyInt());
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenSigninNotAllowed() {
        when(mSigninManagerMock.isSyncOptInAllowed()).thenReturn(false);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock, CONTINUE_URL, mAccountPickerBottomSheetCoordinatorFactoryMock);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED,
                        SigninAccessPoint.WEB_SIGNIN);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(any(), any(), any(), any(), any(), anyInt());
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenNoAccountsOnDevice() {
        when(mSigninManagerMock.isSyncOptInAllowed()).thenReturn(true);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock, CONTINUE_URL, mAccountPickerBottomSheetCoordinatorFactoryMock);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.SUPPRESSED_NO_ACCOUNTS,
                        SigninAccessPoint.WEB_SIGNIN);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(any(), any(), any(), any(), any(), anyInt());
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedIfDismissLimitReached() {
        when(mSigninManagerMock.isSyncOptInAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount("account@test.com");
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT,
                        SigninBridge.ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock, CONTINUE_URL, mAccountPickerBottomSheetCoordinatorFactoryMock);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.SUPPRESSED_CONSECUTIVE_DISMISSALS,
                        SigninAccessPoint.WEB_SIGNIN);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(any(), any(), any(), any(), any(), anyInt());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testAccountPickerShown() {
        when(mSigninManagerMock.isSyncOptInAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount("account@test.com");

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock, CONTINUE_URL, mAccountPickerBottomSheetCoordinatorFactoryMock);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock)
                .create(
                        eq(mWindowAndroidMock),
                        eq(mBottomSheetControllerMock),
                        any(),
                        any(),
                        any(),
                        anyInt());
    }
}
