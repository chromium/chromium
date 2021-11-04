// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.support.test.runner.lifecycle.Stage;

import androidx.annotation.IdRes;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.signin.SigninFirstRunFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/**
 * Integration tests for the first run experience with sign-in and sync decoupled.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
public class FirstRunActivitySigninAndSyncTest {
    private static final String TEST_EMAIL = "test.account@gmail.com";
    private static final String CHILD_EMAIL = "child.account@gmail.com";

    // Disable animations to reduce flakiness.
    @ClassRule
    public static final DisableAnimationsTestRule sNoAnimationsRule =
            new DisableAnimationsTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final BaseActivityTestRule<FirstRunActivity> mFirstRunActivityRule =
            new BaseActivityTestRule<>(FirstRunActivity.class);

    @Mock
    private ExternalAuthUtils mExternalAuthUtilsMock;

    @Mock
    private DataReductionProxySettings mDataReductionProxySettingsMock;

    @Before
    public void setUp() {
        when(mDataReductionProxySettingsMock.isDataReductionProxyManaged()).thenReturn(false);
        when(mDataReductionProxySettingsMock.isDataReductionProxyFREPromoAllowed())
                .thenReturn(true);
        DataReductionProxySettings.setInstanceForTesting(mDataReductionProxySettingsMock);
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
    }

    @Test
    @MediumTest
    public void dismissButtonClickSkipsSyncConsentPageWhenNoAccountsAreOnDevice() {
        launchFirstRunActivity();
        onView(withId(R.id.signin_fre_selected_account)).check(matches(not(isDisplayed())));

        clickButton(R.id.signin_fre_dismiss_button);

        ensureCurrentPageIs(DataReductionProxyFirstRunFragment.class);
    }

    @Test
    @MediumTest
    public void dismissButtonClickSkipsSyncConsentPageWhenOneAccountIsOnDevice() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_dismiss_button);

        ensureCurrentPageIs(DataReductionProxyFirstRunFragment.class);
    }

    @Test
    @MediumTest
    public void continueButtonClickShowsSyncConsentPage() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_continue_button);

        ensureCurrentPageIs(SyncConsentFirstRunFragment.class);
    }

    @Test
    @MediumTest
    public void continueButtonClickShowsSyncConsentPageWithChildAccount() {
        mAccountManagerTestRule.addAccount(CHILD_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_continue_button);

        ensureCurrentPageIs(SyncConsentFirstRunFragment.class);
    }

    @Test
    @MediumTest
    public void continueButtonClickSkipsSyncConsentPageWhenCannotUseGooglePlayServices() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));

        clickButton(R.id.signin_fre_continue_button);

        ensureCurrentPageIs(DataReductionProxyFirstRunFragment.class);
    }

    @Test
    @MediumTest
    public void acceptingSyncShowsDataReductionPage() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        ensureCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickButton(R.id.positive_button);

        ensureCurrentPageIs(DataReductionProxyFirstRunFragment.class);
    }

    @Test
    @MediumTest
    public void refusingSyncShowsDataReductionPage() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        ensureCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickButton(R.id.negative_button);

        ensureCurrentPageIs(DataReductionProxyFirstRunFragment.class);
    }

    private void clickButton(@IdRes int buttonId) {
        // This helps to reduce flakiness on some marshmallow bots in comparison with
        // espresso click.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mFirstRunActivityRule.getActivity().findViewById(buttonId).performClick();
        });
    }

    private <T extends FirstRunFragment> void ensureCurrentPageIs(Class<T> fragmentClass) {
        CriteriaHelper.pollUiThread(() -> {
            return fragmentClass.isInstance(
                    mFirstRunActivityRule.getActivity().getCurrentFragmentForTesting());
        }, fragmentClass.getName() + " should be the current page");
    }

    private void launchFirstRunActivity() {
        final Intent intent =
                new Intent(ContextUtils.getApplicationContext(), FirstRunActivity.class);
        mFirstRunActivityRule.launchActivity(intent);
        ApplicationTestUtils.waitForActivityState(
                mFirstRunActivityRule.getActivity(), Stage.RESUMED);
        CriteriaHelper.pollUiThread(
                mFirstRunActivityRule.getActivity()::isNativeSideIsInitializedForTest);
    }
}
