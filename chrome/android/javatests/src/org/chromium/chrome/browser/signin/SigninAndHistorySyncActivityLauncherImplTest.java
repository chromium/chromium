// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.FullscreenSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests {@link SigninAndHistorySyncActivityLauncherImpl}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
public class SigninAndHistorySyncActivityLauncherImplTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule(BlankUiTestActivity.class);

    private static final AccountPickerBottomSheetStrings BOTTOM_SHEET_STRINGS =
            new AccountPickerBottomSheetStrings.Builder(
                            R.string.signin_account_picker_bottom_sheet_title)
                    .build();
    private static final FullscreenSigninAndHistorySyncConfig CONFIG =
            new FullscreenSigninAndHistorySyncConfig.Builder().build();

    @Mock private Context mContextMock;
    @Mock private IdentityServicesProvider mIdentityProviderMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private Profile mProfileMock;
    @Mock private HistorySyncHelper mHistorySyncHelperMock;

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mIdentityProviderMock);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        mActivityTestRule.launchActivity(null);
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelperMock);
    }

    @Test
    @MediumTest
    public void testCreateBottomSheetSigninIntentOrShowErrorWhenSigninIsAllowed() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            BOTTOM_SHEET_STRINGS,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.NONE)
                                    .build();
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            config,
                                            SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateBottomSheetSigninIntentOrShowErrorWithSpecifiedAccountId() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            BOTTOM_SHEET_STRINGS,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.NONE)
                                    .selectedCoreAccountId(TestAccounts.ACCOUNT1.getId())
                                    .build();
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            config,
                                            SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateBottomSheetSigninIntentOrShowErrorWhenHistorySyncIsAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            BOTTOM_SHEET_STRINGS,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.REQUIRED)
                                    .build();
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            config,
                                            SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testLaunchActivityForHistorySyncRequiredFlowWhenSigninIsAllowed() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            BOTTOM_SHEET_STRINGS,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.REQUIRED)
                                    .build();
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            config,
                                            SigninAccessPoint.RECENT_TABS);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateBottomSheetSigninIntentOrShowErrorWhenSigninIsNotPossible() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(anyInt())).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            BOTTOM_SHEET_STRINGS,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.REQUIRED)
                                    .build();
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            config,
                                            SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                    assertNull(intent);
                });
        // TODO(crbug.com/376251506): Verify that error UI is shown.
    }

    @Test
    @MediumTest
    public void
            testCreateBottomSheetSigninIntentOrShowErrorWhenSigninAndHistorySyncAreNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            BOTTOM_SHEET_STRINGS,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.REQUIRED)
                                    .build();
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            config,
                                            SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                    assertNull(intent);
                });
        // TODO(crbug.com/376251506): Verify that error UI is shown.
    }

    @Test
    @MediumTest
    public void
            testCreateBottomSheetSigninIntentOrShowErrorWhenSigninIsNotAllowedAndHistorySyncIsSuppressed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            BOTTOM_SHEET_STRINGS,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.NONE)
                                    .build();
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            config,
                                            SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                    assertNull(intent);
                });
        // TODO(crbug.com/376251506): Verify that error UI is shown.
    }

    @Test
    @MediumTest
    public void testLaunchActivityForHistorySyncRequiredFlowWhenSigninIsNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            BOTTOM_SHEET_STRINGS,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.REQUIRED)
                                    .build();
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createBottomSheetSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            config,
                                            SigninAccessPoint.RECENT_TABS);
                    assertNull(intent);
                });
        // TODO(crbug.com/376251506): Verify that error UI is shown.
    }

    @Test
    @MediumTest
    // TODO(crbug.com/41493758): Update this test when the error UI will be implemented.
    public void testCreateBottomSheetSigninIntentOrShowErrorWhenSigninIsDisabledByPolicy() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        HistogramWatcher watchSigninDisabledToastShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninDisabledNotificationShown",
                        SigninAccessPoint.NTP_SIGNED_OUT_ICON);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            BOTTOM_SHEET_STRINGS,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.NONE)
                                    .build();
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .createBottomSheetSigninIntentOrShowError(
                                    mActivityTestRule.getActivity(),
                                    mProfileMock,
                                    config,
                                    SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                });

        onView(withText(R.string.managed_by_your_organization))
                .inRoot(
                        withDecorView(
                                not(mActivityTestRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
        watchSigninDisabledToastShownHistogram.assertExpected();
    }

    @Test
    @MediumTest
    // TODO(crbug.com/41493758): Update this test when the error UI will be implemented.
    public void testLaunchActivityForHistorySyncRequiredFlowWhenSigninIsDisabledByPolicy() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        HistogramWatcher watchSigninDisabledToastShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninDisabledNotificationShown", SigninAccessPoint.RECENT_TABS);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetSigninAndHistorySyncConfig config =
                            new BottomSheetSigninAndHistorySyncConfig.Builder(
                                            BOTTOM_SHEET_STRINGS,
                                            NoAccountSigninMode.BOTTOM_SHEET,
                                            WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                            HistorySyncConfig.OptInMode.REQUIRED)
                                    .build();
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .createBottomSheetSigninIntentOrShowError(
                                    mActivityTestRule.getActivity(),
                                    mProfileMock,
                                    config,
                                    SigninAccessPoint.RECENT_TABS);
                });

        onView(withText(R.string.managed_by_your_organization))
                .inRoot(withDecorView(allOf(withId(R.id.toast_text))))
                .check(matches(isDisplayed()));
        watchSigninDisabledToastShownHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntent() {
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntent(
                                            mContextMock,
                                            mProfileMock,
                                            CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentWhenSigninNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(false);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntent(
                                            mContextMock,
                                            mProfileMock,
                                            CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentWhenAlreadySignedIn() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntent(
                                            mContextMock,
                                            mProfileMock,
                                            CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentWhenSignedInAndHistorySyncNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntent(
                                            mContextMock,
                                            mProfileMock,
                                            CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentWhenSignedInAndHistorySyncDeclinedOften() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntent(
                                            mContextMock,
                                            mProfileMock,
                                            CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentOrShowError() {
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentOrShowErrorWhenSigninNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(false);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNull(intent);
                });
        // TODO(crbug.com/376251506): Verify that error UI is shown.
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentOrShowErrorWhenAlreadySignedIn() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNotNull(intent);
                });
    }

    @Test
    @MediumTest
    public void testCreateFullscreenSigninIntentOrShowErrorWhenSignedInAndHistorySyncNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNull(intent);
                });
        // TODO(crbug.com/376251506): Verify that error UI is shown.
    }

    @Test
    @MediumTest
    public void
            testCreateFullscreenSigninIntentOrShowErrorWhenSignedInAndHistorySyncDeclinedOften() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    @Nullable
                    Intent intent =
                            SigninAndHistorySyncActivityLauncherImpl.get()
                                    .createFullscreenSigninIntentOrShowError(
                                            mContextMock,
                                            mProfileMock,
                                            CONFIG,
                                            SigninAccessPoint.SIGNIN_PROMO);
                    assertNull(intent);
                });
        // TODO(crbug.com/376251506): Verify that error UI is shown.
    }
}
