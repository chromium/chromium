// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.signinbottomsheet;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
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
    private BottomSheetContent mBottomSheetContentMock;

    @Mock
    private WindowAndroid mWindowAndroidMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private IdentityManager mIdentityManagerMock;

    @Mock
    private Profile mProfileMock;

    @Mock
    private AccountPickerBottomSheetCoordinator mAccountPickerBottomSheetCoordinatorMock;

    private SigninBottomSheetCoordinator mSigninCoordinator;
    private CoreAccountInfo mCoreAccountInfo;

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getSigninManager(mProfileMock))
                .thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);
        mCoreAccountInfo = mAccountManagerTestRule.addAccount(TEST_EMAIL);
        mSigninCoordinator = new SigninBottomSheetCoordinator(
                mWindowAndroidMock, mBottomSheetControllerMock, mProfileMock);
    }

    @Test
    public void testSignInCompleted() {
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "ContentSuggestions.Feed.SignInFromFeedAction.SignInSuccessful", true);
        doAnswer(invocation -> {
            SigninManager.SignInCallback callback = invocation.getArgument(1);
            callback.onSignInComplete();
            return null;
        })
                .when(mSigninManagerMock)
                .signin(eq(AccountUtils.createAccountFromName(TEST_EMAIL)), any());
        mSigninCoordinator.signIn(TEST_EMAIL, error -> {});
        verify(mSigninManagerMock)
                .signin(eq(AccountUtils.createAccountFromName(TEST_EMAIL)), any());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testSignInAborted() {
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "ContentSuggestions.Feed.SignInFromFeedAction.SignInSuccessful", false);
        doAnswer(invocation -> {
            SigninManager.SignInCallback callback = invocation.getArgument(1);
            callback.onSignInAborted();
            return null;
        })
                .when(mSigninManagerMock)
                .signin(eq(AccountUtils.createAccountFromName(TEST_EMAIL)), any());
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
                .signin(eq(AccountUtils.createAccountFromName(TEST_EMAIL)), any());
    }
}
