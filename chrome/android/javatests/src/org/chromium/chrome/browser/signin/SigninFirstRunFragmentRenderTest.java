// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.accounts.Account;
import android.support.test.runner.lifecycle.Stage;

import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunPageDelegate;
import org.chromium.chrome.browser.firstrun.PolicyLoadListener;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninFirstRunFragmentTest.CustomSigninFirstRunFragment;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Render tests for the class {@link SigninFirstRunFragment}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninFirstRunFragmentRenderTest {
    private static final String TEST_EMAIL1 = "test.account1@gmail.com";
    private static final String CHILD_EMAIL = "child.account@gmail.com";

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            new FakeAccountManagerFacade() {
                @Override
                public void checkChildAccountStatus(
                        Account account, ChildAccountStatusListener listener) {
                    listener.onStatusReady(account.name.equals(CHILD_EMAIL)
                                    ? ChildAccountStatus.REGULAR_CHILD
                                    : ChildAccountStatus.NOT_CHILD);
                }
            };

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final RenderTestRule mRenderTestRule = RenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade, new FakeAccountInfoService());

    @Rule
    public final ChromeTabbedActivityTestRule mChromeActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Mock
    private ExternalAuthUtils mExternalAuthUtilsMock;

    @Mock
    private FirstRunPageDelegate mFirstRunPageDelegateMock;

    @Mock
    private PolicyLoadListener mPolicyLoadListenerMock;

    @Mock
    private SigninManager mSigninManagerMock;

    private CustomSigninFirstRunFragment mFragment;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        });
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @Before
    public void setUp() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        SigninCheckerProvider.setForTests(mock(SigninChecker.class));
        when(mPolicyLoadListenerMock.get()).thenReturn(false);
        when(mFirstRunPageDelegateMock.getPolicyLoadListener()).thenReturn(mPolicyLoadListenerMock);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mFragment = new CustomSigninFirstRunFragment();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        mFragment.setPageDelegate(mFirstRunPageDelegateMock);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testFragmentWithAccount(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccountWithNameAndAvatar(TEST_EMAIL1);

        launchActivityWithFragment();

        CriteriaHelper.pollUiThread(() -> {
            return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
        });
        mRenderTestRule.render(mFragment.getView(), "signin_first_run_fragment_with_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testFragmentWithAccountOnManagedDevice(boolean nightModeEnabled)
            throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccountWithNameAndAvatar(TEST_EMAIL1);

        launchActivityWithFragment();

        CriteriaHelper.pollUiThread(() -> {
            return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
        });
        mRenderTestRule.render(
                mFragment.getView(), "signin_first_run_fragment_with_account_managed");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testFragmentWithAccountWhenSigninIsDisabledByPolicy(boolean nightModeEnabled)
            throws IOException {
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(IdentityServicesProvider.get().getSigninManager(
                         Profile.getLastUsedRegularProfile()))
                    .thenReturn(mSigninManagerMock);
        });
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccountWithNameAndAvatar(TEST_EMAIL1);

        launchActivityWithFragment();

        CriteriaHelper.pollUiThread(() -> {
            return !mFragment.getView().findViewById(R.id.signin_fre_selected_account).isShown();
        });
        mRenderTestRule.render(
                mFragment.getView(), "signin_first_run_fragment_when_signin_disabled_by_policy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testFragmentWithoutAccount(boolean nightModeEnabled) throws IOException {
        launchActivityWithFragment();

        mRenderTestRule.render(mFragment.getView(), "signin_first_run_fragment_without_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testFragmentWithoutAccountOnManagedDevice(boolean nightModeEnabled)
            throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        launchActivityWithFragment();

        mRenderTestRule.render(
                mFragment.getView(), "signin_first_run_fragment_without_account_managed");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testFragmentWithChildAccount(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccountWithNameAndAvatar(CHILD_EMAIL);

        launchActivityWithFragment();

        CriteriaHelper.pollUiThread(() -> {
            return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
        });
        mRenderTestRule.render(mFragment.getView(), "signin_first_run_fragment_with_child_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testFragmentWhenCannotUseGooglePlayService(boolean nightModeEnabled)
            throws IOException {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });

        launchActivityWithFragment();

        mRenderTestRule.render(
                mFragment.getView(), "signin_first_run_fragment_signin_not_supported");
    }

    private void launchActivityWithFragment() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mChromeActivityTestRule.getActivity()
                    .getSupportFragmentManager()
                    .beginTransaction()
                    .add(android.R.id.content, mFragment)
                    .commit();
        });
        ApplicationTestUtils.waitForActivityState(
                mChromeActivityTestRule.getActivity(), Stage.RESUMED);
    }
}
