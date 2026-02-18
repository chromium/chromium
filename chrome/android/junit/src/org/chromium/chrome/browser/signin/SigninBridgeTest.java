// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
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
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;

/** JUnit tests for the class {@link SigninBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SigninBridgeTest {
    private static final GURL CONTINUE_URL = new GURL("https://test-continue-url.com");
    private final FakeIdentityManager mIdentityManager = new FakeIdentityManager();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Tab mTabMock;

    @Mock private Profile mProfileMock;

    @Mock private WindowAndroid mWindowAndroidMock;

    @Mock private SigninManager mSigninManagerMock;

    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsJniMock;

    @Mock private BottomSheetController mBottomSheetControllerMock;

    @Mock
    private SigninBridge.AccountPickerBottomSheetCoordinatorFactory
            mAccountPickerBottomSheetCoordinatorFactoryMock;

    @Before
    public void setUp() {
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetControllerMock);
        Context context = ApplicationProvider.getApplicationContext();

        lenient().when(mWindowAndroidMock.getContext()).thenReturn(new WeakReference<>(context));
        lenient().when(mTabMock.getProfile()).thenReturn(mProfileMock);
        lenient().when(mTabMock.getWindowAndroid()).thenReturn(mWindowAndroidMock);
        lenient().when(mTabMock.isUserInteractable()).thenReturn(true);
        lenient().when(mProfileMock.getOriginalProfile()).thenReturn(mProfileMock);

        IdentityServicesProvider.setSigninManagerForTesting(mSigninManagerMock);
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManager);
        SigninMetricsUtilsJni.setInstanceForTesting(mSigninMetricsUtilsJniMock);
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
                mTabMock,
                CONTINUE_URL,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT1.getId());
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(TestAccounts.ACCOUNT1.getId()));
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenTabNotInteractable() {
        //  Reset default values configured in `setUp`.
        Mockito.reset(mTabMock);
        when(mTabMock.getWindowAndroid()).thenReturn(mWindowAndroidMock);
        when(mTabMock.isUserInteractable()).thenReturn(false);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                CONTINUE_URL,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT1.getId());
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(TestAccounts.ACCOUNT1.getId()));
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenSigninNotAllowed() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                CONTINUE_URL,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT1.getId());
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.SUPPRESSED_SIGNIN_NOT_ALLOWED,
                        SigninAccessPoint.WEB_SIGNIN);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(TestAccounts.ACCOUNT1.getId()));
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedWhenNoAccountsOnDevice() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                CONTINUE_URL,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT1.getId());
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.SUPPRESSED_NO_ACCOUNTS,
                        SigninAccessPoint.WEB_SIGNIN);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(TestAccounts.ACCOUNT1.getId()));
    }

    @Test
    @SmallTest
    public void testAccountPickerSuppressedIfDismissLimitReached() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT,
                        SigninBridge.ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock, CONTINUE_URL, mAccountPickerBottomSheetCoordinatorFactoryMock, null);

        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.SUPPRESSED_CONSECUTIVE_DISMISSALS,
                        SigninAccessPoint.WEB_SIGNIN);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock, never())
                .create(any(), any(), any(), any(), any(), any(), any(), anyInt(), eq(null));
    }

    @Test
    @SmallTest
    public void testAccountPickerHasNoLimitIfAccountIsSpecified() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.WEB_SIGNIN_ACCOUNT_PICKER_ACTIVE_DISMISSAL_COUNT,
                        SigninBridge.ACCOUNT_PICKER_BOTTOM_SHEET_DISMISS_LIMIT);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                CONTINUE_URL,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT2.getId());

        verify(mSigninMetricsUtilsJniMock, never())
                .logAccountConsistencyPromoAction(
                        eq(AccountConsistencyPromoAction.SUPPRESSED_CONSECUTIVE_DISMISSALS),
                        anyInt());
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock)
                .create(
                        eq(mWindowAndroidMock),
                        any(),
                        any(),
                        eq(mBottomSheetControllerMock),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(TestAccounts.ACCOUNT2.getId()));
    }

    @Test
    @SmallTest
    public void testAccountPickerShown() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock,
                CONTINUE_URL,
                mAccountPickerBottomSheetCoordinatorFactoryMock,
                TestAccounts.ACCOUNT1.getId());
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock)
                .create(
                        eq(mWindowAndroidMock),
                        any(),
                        any(),
                        eq(mBottomSheetControllerMock),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(TestAccounts.ACCOUNT1.getId()));
    }

    @Test
    @SmallTest
    public void testAccountPickerShownWithNoSelectedAccountId() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        SigninBridge.openAccountPickerBottomSheet(
                mTabMock, CONTINUE_URL, mAccountPickerBottomSheetCoordinatorFactoryMock, null);
        verify(mAccountPickerBottomSheetCoordinatorFactoryMock)
                .create(
                        eq(mWindowAndroidMock),
                        any(),
                        any(),
                        eq(mBottomSheetControllerMock),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        isNull());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(SigninFeatures.ENABLE_ADD_SESSION_REDIRECT)
    public void testBottomSheetInvokedAfterAddAccountFlow() {
        ArgumentCaptor<WindowAndroid.IntentCallback> intentCaptor =
                ArgumentCaptor.forClass(WindowAndroid.IntentCallback.class);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        when(mWindowAndroidMock.showIntent(any(Intent.class), intentCaptor.capture(), any()))
                .thenReturn(true);
        FakeAccountManagerFacade fakeAccountManagerFacade =
                (FakeAccountManagerFacade) AccountManagerFacadeProvider.getInstance();
        AccountManagerFacadeProvider.setInstanceForTests(fakeAccountManagerFacade);
        fakeAccountManagerFacade.setAddAccountFlowResult(TestAccounts.ACCOUNT2);

        SigninBridge.startAddAccountFlow(
                mTabMock,
                TestAccounts.ACCOUNT2.getEmail(),
                CONTINUE_URL,
                mAccountPickerBottomSheetCoordinatorFactoryMock);

        mIdentityManager.addOrUpdateExtendedAccountInfo(TestAccounts.ACCOUNT2);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT2);
        intentCaptor.getValue().onIntentCompleted(Activity.RESULT_OK, null);

        verify(mAccountPickerBottomSheetCoordinatorFactoryMock)
                .create(
                        eq(mWindowAndroidMock),
                        any(),
                        any(),
                        eq(mBottomSheetControllerMock),
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        eq(TestAccounts.ACCOUNT2.getId()));
    }
}
