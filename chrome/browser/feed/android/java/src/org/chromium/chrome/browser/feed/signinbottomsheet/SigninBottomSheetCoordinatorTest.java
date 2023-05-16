// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.signinbottomsheet;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.base.WindowAndroid;

/** Tests for SigninBottomSheetCoordinator */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class SigninBottomSheetCoordinatorTest {
    private static final String TEST_EMAIL = "test.account@gmail.com";
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private BottomSheetController mBottomSheetControllerMock;

    @Mock
    private WindowAndroid mWindowAndroidMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private Profile mProfileMock;

    @Mock
    private AccountPickerBottomSheetCoordinator mAccountPickerBottomSheetCoordinatorMock;

    @Mock
    private Runnable mOnSigninSuccessCallbackMock;

    private SigninBottomSheetCoordinator mSigninCoordinator;

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getSigninManager(mProfileMock))
                .thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        mSigninCoordinator = new SigninBottomSheetCoordinator(mWindowAndroidMock, null,
                mBottomSheetControllerMock, mProfileMock, null, null,
                SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO);
    }

    @Test
    public void testSigninCompleted_callsSigninManagerAndUpdatesHistogram() {
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "ContentSuggestions.Feed.SignInFromFeedAction.SignInSuccessful", true);
        doAnswer(invocation -> {
            SigninManager.SignInCallback callback = invocation.getArgument(2);
            callback.onSignInComplete();
            return null;
        })
                .when(mSigninManagerMock)
                .signin(eq(AccountUtils.createAccountFromName(TEST_EMAIL)),
                        eq(SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO), any());
        mSigninCoordinator.signIn(TEST_EMAIL, error -> {});
        verify(mSigninManagerMock)
                .signin(eq(AccountUtils.createAccountFromName(TEST_EMAIL)),
                        eq(SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO),
                        any(SigninManager.SignInCallback.class));
        histogramWatcher.assertExpected();
    }

    @Test
    public void testSigninAborted_doesNotUpdateHistogram() {
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "ContentSuggestions.Feed.SignInFromFeedAction.SignInSuccessful", false);
        doAnswer(invocation -> {
            SigninManager.SignInCallback callback = invocation.getArgument(2);
            callback.onSignInAborted();
            return null;
        })
                .when(mSigninManagerMock)
                .signin(eq(AccountUtils.createAccountFromName(TEST_EMAIL)),
                        eq(SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO), any());
        mSigninCoordinator.setAccountPickerBottomSheetCoordinator(
                mAccountPickerBottomSheetCoordinatorMock);
        mSigninCoordinator.signIn(TEST_EMAIL, error -> {});
        histogramWatcher.assertExpected();
    }

    @Test
    public void testSignInNotAllowed() {
        HistogramWatcher watchSigninDisabledToastShownHistogram =
                HistogramWatcher.newSingleRecordWatcher("Signin.SigninDisabledNotificationShown",
                        SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        mSigninCoordinator.setToastOverrideForTesting();
        mSigninCoordinator.signIn(TEST_EMAIL, error -> {});
        verify(mSigninManagerMock, never())
                .signin(eq(AccountUtils.createAccountFromName(TEST_EMAIL)), anyInt(), any());
        watchSigninDisabledToastShownHistogram.assertExpected();
    }

    @Test
    public void testSigninCompleted_callSigninSuccessCallback() {
        SigninBottomSheetCoordinator coordinator = new SigninBottomSheetCoordinator(
                mWindowAndroidMock, null, mBottomSheetControllerMock, mProfileMock, null,
                mOnSigninSuccessCallbackMock, SigninAccessPoint.NTP_FEED_BOTTOM_PROMO);
        doAnswer(invocation -> {
            SigninManager.SignInCallback callback = invocation.getArgument(2);
            callback.onSignInComplete();
            return null;
        })
                .when(mSigninManagerMock)
                .signin(eq(AccountUtils.createAccountFromName(TEST_EMAIL)),
                        eq(SigninAccessPoint.NTP_FEED_BOTTOM_PROMO), any());
        coordinator.signIn(TEST_EMAIL, error -> {});
        verify(mOnSigninSuccessCallbackMock, times(1)).run();
    }

    @Test
    public void testSigninAborted_doesNotCallSigninSuccessCallback() {
        SigninBottomSheetCoordinator coordinator = new SigninBottomSheetCoordinator(
                mWindowAndroidMock, null, mBottomSheetControllerMock, mProfileMock, null,
                mOnSigninSuccessCallbackMock, SigninAccessPoint.NTP_FEED_BOTTOM_PROMO);
        doAnswer(invocation -> {
            SigninManager.SignInCallback callback = invocation.getArgument(2);
            callback.onSignInAborted();
            return null;
        })
                .when(mSigninManagerMock)
                .signin(eq(AccountUtils.createAccountFromName(TEST_EMAIL)),
                        eq(SigninAccessPoint.NTP_FEED_BOTTOM_PROMO), any());
        coordinator.setAccountPickerBottomSheetCoordinator(
                mAccountPickerBottomSheetCoordinatorMock);
        coordinator.signIn(TEST_EMAIL, error -> {});
        verify(mOnSigninSuccessCallbackMock, times(0)).run();
    }
}
