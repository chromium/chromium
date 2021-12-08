// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin;

import static org.mockito.Mockito.when;

import android.content.res.Configuration;
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
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
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
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Render tests for the class {@link SigninFirstRunFragment}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninFirstRunFragmentRenderTest {
    /** Parameter provider for night mode state and device orientation. */
    public static class NightModeAndOrientationParameterProvider implements ParameterProvider {
        private static List<ParameterSet> sParams = Arrays.asList(
                new ParameterSet()
                        .value(/*nightModeEnabled=*/false, Configuration.ORIENTATION_PORTRAIT)
                        .name("NightModeDisabled_Portrait"),
                new ParameterSet()
                        .value(/*nightModeEnabled=*/false, Configuration.ORIENTATION_LANDSCAPE)
                        .name("NightModeDisabled_Landscape"),
                new ParameterSet()
                        .value(/*nightModeEnabled=*/true, Configuration.ORIENTATION_PORTRAIT)
                        .name("NightModeEnabled_Portrait"),
                new ParameterSet()
                        .value(/*nightModeEnabled=*/true, Configuration.ORIENTATION_LANDSCAPE)
                        .name("NightModeEnabled_Landscape"));
        @Override
        public Iterable<ParameterSet> getParameters() {
            return sParams;
        }
    }

    private static final String TEST_EMAIL1 = "test.account1@gmail.com";
    private static final String CHILD_ACCOUNT_NAME =
            AccountManagerTestRule.generateChildEmail("account@gmail.com");

    // Disable animations to reduce flakiness.
    @Rule
    public final DisableAnimationsTestRule mNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final RenderTestRule mRenderTestRule = RenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

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
    @Mock
    private SigninChecker mSigninCheckerMock;
    @Mock
    private IdentityManager mIdentityManagerMock;
    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;

    private CustomSigninFirstRunFragment mFragment;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeAndOrientationParameterProvider.class)
    public void setupNightModeAndDeviceOrientation(boolean nightModeEnabled, int orientation) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        });
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(
                orientation == Configuration.ORIENTATION_PORTRAIT ? "Portrait" : "Landscape");
    }

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @Before
    public void setUp() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        SigninCheckerProvider.setForTests(mSigninCheckerMock);
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
    public void testFragmentRotationToLandscapeWithAccount() throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);
        launchActivityWithFragment(Configuration.ORIENTATION_PORTRAIT);

        ActivityTestUtils.rotateActivityToOrientation(
                mChromeActivityTestRule.getActivity(), Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> {
            return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
        });
        mRenderTestRule.render(
                mFragment.getView(), "signin_first_run_fragment_with_account_landscape");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testFragmentRotationToPortraitWithAccount() throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);
        launchActivityWithFragment(Configuration.ORIENTATION_LANDSCAPE);

        ActivityTestUtils.rotateActivityToOrientation(
                mChromeActivityTestRule.getActivity(), Configuration.ORIENTATION_PORTRAIT);
        CriteriaHelper.pollUiThread(() -> {
            return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
        });
        mRenderTestRule.render(
                mFragment.getView(), "signin_first_run_fragment_with_account_portrait");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithAccount(boolean nightModeEnabled, int orientation)
            throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(() -> {
            return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
        });
        mRenderTestRule.render(mFragment.getView(), "signin_first_run_fragment_with_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithAccountOnManagedDevice(boolean nightModeEnabled, int orientation)
            throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(() -> {
            return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
        });
        mRenderTestRule.render(
                mFragment.getView(), "signin_first_run_fragment_with_account_managed");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithAccountWhenSigninIsDisabledByPolicy(
            boolean nightModeEnabled, int orientation) throws IOException {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(IdentityServicesProvider.get().getSigninManager(
                         Profile.getLastUsedRegularProfile()))
                    .thenReturn(mSigninManagerMock);
            when(IdentityServicesProvider.get().getIdentityManager(
                         Profile.getLastUsedRegularProfile()))
                    .thenReturn(mIdentityManagerMock);
        });
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(() -> {
            return !mFragment.getView().findViewById(R.id.signin_fre_selected_account).isShown();
        });
        mRenderTestRule.render(
                mFragment.getView(), "signin_first_run_fragment_when_signin_disabled_by_policy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithoutAccount(boolean nightModeEnabled, int orientation)
            throws IOException {
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(mFragment.getView(), "signin_first_run_fragment_without_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithoutAccountOnManagedDevice(boolean nightModeEnabled, int orientation)
            throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mFragment.getView(), "signin_first_run_fragment_without_account_managed");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithChildAccount(boolean nightModeEnabled, int orientation)
            throws IOException {
        mAccountManagerTestRule.addAccount(CHILD_ACCOUNT_NAME);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(() -> {
            return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
        });
        mRenderTestRule.render(mFragment.getView(), "signin_first_run_fragment_with_child_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWhenCannotUseGooglePlayService(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mFragment.getView(), "signin_first_run_fragment_signin_not_supported");
    }

    private void launchActivityWithFragment(int orientation) {
        ActivityTestUtils.rotateActivityToOrientation(
                mChromeActivityTestRule.getActivity(), orientation);
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
