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

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistoryOptInCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests {@link SigninAndHistoryOptInActivityLauncherImpl}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SigninAndHistoryOptInActivityLauncherImplTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule(BlankUiTestActivity.class);

    @Mock private Context mContextMock;
    @Mock private IdentityServicesProvider mIdentityProviderMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private Profile mProfileMock;

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mIdentityProviderMock);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        mActivityTestRule.launchActivity(null);
    }

    @Test
    @MediumTest
    public void testLaunchActivityIfAllowedWhenSigninIsAllowed() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(true);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistoryOptInActivityLauncherImpl.get()
                            .launchActivityIfAllowed(
                                    mContextMock,
                                    mProfileMock,
                                    SigninAndHistoryOptInCoordinator.NoAccountSigninMode
                                            .BOTTOM_SHEET,
                                    SigninAndHistoryOptInCoordinator.HistoryOptInMode.OPTIONAL,
                                    SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                });
        verify(mContextMock).startActivity(notNull());
    }

    @Test
    @MediumTest
    public void testLaunchActivityIfAllowedWhenSigninIsNotAllowed() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(false);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistoryOptInActivityLauncherImpl.get()
                            .launchActivityIfAllowed(
                                    mContextMock,
                                    mProfileMock,
                                    SigninAndHistoryOptInCoordinator.NoAccountSigninMode
                                            .BOTTOM_SHEET,
                                    SigninAndHistoryOptInCoordinator.HistoryOptInMode.OPTIONAL,
                                    SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                });

        verify(mContextMock, never()).startActivity(notNull());
    }

    @Test
    @MediumTest
    // TODO(https://crbug.com/1520783): Update this test when the error UI will be implemented.
    public void testLaunchActivityIfAllowedWhenSigninIsDisabledByPolicy() {
        when(mSigninManagerMock.isSigninAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        HistogramWatcher watchSigninDisabledToastShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninDisabledNotificationShown",
                        SigninAccessPoint.NTP_SIGNED_OUT_ICON);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SigninAndHistoryOptInActivityLauncherImpl.get()
                            .launchActivityIfAllowed(
                                    mActivityTestRule.getActivity(),
                                    mProfileMock,
                                    SigninAndHistoryOptInCoordinator.NoAccountSigninMode
                                            .BOTTOM_SHEET,
                                    SigninAndHistoryOptInCoordinator.HistoryOptInMode.OPTIONAL,
                                    SigninAccessPoint.NTP_SIGNED_OUT_ICON);
                });

        onView(withText(R.string.managed_by_your_organization))
                .inRoot(
                        withDecorView(
                                not(mActivityTestRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
        watchSigninDisabledToastShownHistogram.assertExpected();
    }
}
