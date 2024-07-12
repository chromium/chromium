// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests {@link SigninAndHistorySyncActivityLauncherImpl}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
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
    public void testLaunchActivityIfAllowedWhenSigninIsAllowed() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchActivityIfAllowed(
                                    mContextMock,
                                    mProfileMock,
                                    BOTTOM_SHEET_STRINGS,
                                    SigninAndHistorySyncCoordinator.NoAccountSigninMode
                                            .BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                            .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE,
                                    SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                });

        verify(mContextMock).startActivity(notNull());
    }

    @Test
    @MediumTest
    public void testLaunchActivityIfAllowedWhenHistorySyncIsAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchActivityIfAllowed(
                                    mContextMock,
                                    mProfileMock,
                                    BOTTOM_SHEET_STRINGS,
                                    SigninAndHistorySyncCoordinator.NoAccountSigninMode
                                            .BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                            .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.HistoryOptInMode.REQUIRED,
                                    SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                });

        verify(mContextMock).startActivity(notNull());
    }

    @Test
    @MediumTest
    public void testLaunchActivityForHistorySyncDedicatedFlowWhenSigninIsAllowed() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchActivityForHistorySyncDedicatedFlow(
                                    mContextMock,
                                    mProfileMock,
                                    BOTTOM_SHEET_STRINGS,
                                    SigninAndHistorySyncCoordinator.NoAccountSigninMode
                                            .BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                            .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                    SigninAccessPoint.RECENT_TABS);
                });

        verify(mContextMock).startActivity(notNull());
    }

    @Test
    @MediumTest
    public void testLaunchActivityIfAllowedWhenSigninIsNotPossible() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(anyInt())).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchActivityIfAllowed(
                                    mContextMock,
                                    mProfileMock,
                                    BOTTOM_SHEET_STRINGS,
                                    SigninAndHistorySyncCoordinator.NoAccountSigninMode
                                            .BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                            .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.HistoryOptInMode.REQUIRED,
                                    SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                });

        verify(mContextMock, never()).startActivity(notNull());
    }

    @Test
    @MediumTest
    public void testLaunchActivityIfAllowedWhenSigninAndHistorySyncAreNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchActivityIfAllowed(
                                    mContextMock,
                                    mProfileMock,
                                    BOTTOM_SHEET_STRINGS,
                                    SigninAndHistorySyncCoordinator.NoAccountSigninMode
                                            .BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                            .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.HistoryOptInMode.REQUIRED,
                                    SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                });

        verify(mContextMock, never()).startActivity(notNull());
    }

    @Test
    @MediumTest
    public void testLaunchActivityIfAllowedWhenSigninIsNotAllowedAndHistorySyncIsSuppressed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchActivityIfAllowed(
                                    mContextMock,
                                    mProfileMock,
                                    BOTTOM_SHEET_STRINGS,
                                    SigninAndHistorySyncCoordinator.NoAccountSigninMode
                                            .BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                            .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE,
                                    SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                });

        verify(mContextMock, never()).startActivity(notNull());
    }

    @Test
    @MediumTest
    public void testLaunchActivityForHistorySyncDedicatedFlowWhenSigninIsNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchActivityForHistorySyncDedicatedFlow(
                                    mContextMock,
                                    mProfileMock,
                                    BOTTOM_SHEET_STRINGS,
                                    SigninAndHistorySyncCoordinator.NoAccountSigninMode
                                            .BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                            .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                    SigninAccessPoint.RECENT_TABS);
                });

        verify(mContextMock, never()).startActivity(notNull());
    }

    @Test
    @MediumTest
    // TODO(crbug.com/41493758): Update this test when the error UI will be implemented.
    public void testLaunchActivityIfAllowedWhenSigninIsDisabledByPolicy() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        HistogramWatcher watchSigninDisabledToastShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninDisabledNotificationShown",
                        SigninAccessPoint.NTP_SIGNED_OUT_ICON);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchActivityIfAllowed(
                                    mActivityTestRule.getActivity(),
                                    mProfileMock,
                                    BOTTOM_SHEET_STRINGS,
                                    SigninAndHistorySyncCoordinator.NoAccountSigninMode
                                            .BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                            .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE,
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
    @DisabledTest(message = "https://crbug.com/352314425")
    public void testLaunchActivityForHistorySyncDedicatedFlowWhenSigninIsDisabledByPolicy() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        HistogramWatcher watchSigninDisabledToastShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninDisabledNotificationShown", SigninAccessPoint.RECENT_TABS);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchActivityForHistorySyncDedicatedFlow(
                                    mActivityTestRule.getActivity(),
                                    mProfileMock,
                                    BOTTOM_SHEET_STRINGS,
                                    SigninAndHistorySyncCoordinator.NoAccountSigninMode
                                            .BOTTOM_SHEET,
                                    SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                            .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                    SigninAccessPoint.RECENT_TABS);
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
    public void testLaunchUpgradePromoActivityIfAllowed() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchUpgradePromoActivityIfAllowed(mContextMock, mProfileMock);
                });

        verify(mContextMock).startActivity(notNull());
    }

    @Test
    @MediumTest
    public void testLaunchUpgradePromoActivityIfAllowedWhenSigninNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(false);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchUpgradePromoActivityIfAllowed(mContextMock, mProfileMock);
                });

       verify(mContextMock, never()).startActivity(notNull());
    }

    @Test
    @MediumTest
    public void testLaunchUpgradePromoActivityIfAllowedWhenAlreadySignedIn() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(false);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchUpgradePromoActivityIfAllowed(mContextMock, mProfileMock);
                });

        verify(mContextMock).startActivity(notNull());
    }

    @Test
    @MediumTest
    public void testLaunchUpgradePromoActivityIfAllowedWhenSignedInAndHistorySyncNotAllowed() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.shouldSuppressHistorySync()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchUpgradePromoActivityIfAllowed(mContextMock, mProfileMock);
                });

        verify(mContextMock, never()).startActivity(notNull());
    }

    @Test
    @MediumTest
    public void testLaunchUpgradePromoActivityIfAllowedWhenSignedInAndHistorySyncDeclinedOften() {
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(eq(ConsentLevel.SIGNIN))).thenReturn(true);
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mHistorySyncHelperMock.isDeclinedOften()).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .launchUpgradePromoActivityIfAllowed(mContextMock, mProfileMock);
                });

        verify(mContextMock, never()).startActivity(notNull());
    }
}
