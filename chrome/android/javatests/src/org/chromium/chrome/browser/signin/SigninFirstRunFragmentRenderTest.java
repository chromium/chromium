// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.transit.ViewElement.displayingAtLeastOption;
import static org.chromium.base.test.transit.ViewFinder.waitForView;

import android.content.res.Configuration;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.firstrun.FirstRunPageDelegate;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.SigninFirstRunFragmentTest.CustomSigninFirstRunFragment;
import org.chromium.chrome.browser.signin.services.SigninChecker;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Render tests for the class {@link SigninFirstRunFragment}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
@DoNotBatch(reason = "Relies on global state")
public class SigninFirstRunFragmentRenderTest {
    /** Parameter provider for night mode state and device orientation. */
    public static class NightModeAndOrientationParameterProvider implements ParameterProvider {
        private static final List<ParameterSet> sParams =
                Arrays.asList(
                        new ParameterSet()
                                .value(/* firstArg= */ false, Configuration.ORIENTATION_PORTRAIT)
                                .name("NightModeDisabled_Portrait"),
                        new ParameterSet()
                                .value(/* firstArg= */ false, Configuration.ORIENTATION_LANDSCAPE)
                                .name("NightModeDisabled_Landscape"),
                        new ParameterSet()
                                .value(/* firstArg= */ true, Configuration.ORIENTATION_PORTRAIT)
                                .name("NightModeEnabled_Portrait"),
                        new ParameterSet()
                                .value(/* firstArg= */ true, Configuration.ORIENTATION_LANDSCAPE)
                                .name("NightModeEnabled_Landscape"));

        @Override
        public Iterable<ParameterSet> getParameters() {
            return sParams;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(3)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_FIRST_RUN)
                    .build();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Mock private ProfileProvider mProfileProviderMock;
    @Mock private ExternalAuthUtils mExternalAuthUtilsMock;
    @Mock private FirstRunPageDelegate mFirstRunPageDelegateMock;
    @Mock private PolicyLoadListener mPolicyLoadListenerMock;
    @Mock private SyncService mSyncService;
    @Mock private SigninChecker mSigninCheckerMock;
    @Mock private PrivacyPreferencesManagerImpl mPrivacyPreferencesManagerMock;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private PrefService mPrefService;

    private CustomSigninFirstRunFragment mFragment;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeAndOrientationParameterProvider.class)
    public void setupNightModeAndDeviceOrientation(boolean nightModeEnabled, int orientation) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppCompatDelegate.setDefaultNightMode(
                            nightModeEnabled
                                    ? AppCompatDelegate.MODE_NIGHT_YES
                                    : AppCompatDelegate.MODE_NIGHT_NO);
                });
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        mRenderTestRule.setVariantPrefix(
                orientation == Configuration.ORIENTATION_PORTRAIT ? "Portrait" : "Landscape");
    }

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mActivityTestRule.launchActivity(null);
        OneshotSupplierImpl<ProfileProvider> profileSupplier =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            OneshotSupplierImpl<ProfileProvider> supplier =
                                    new OneshotSupplierImpl<>();
                            when(mProfileProviderMock.getOriginalProfile())
                                    .thenReturn(ProfileManager.getLastUsedRegularProfile());
                            supplier.set(mProfileProviderMock);
                            return supplier;
                        });
        when(mFirstRunPageDelegateMock.getProfileProviderSupplier()).thenReturn(profileSupplier);

        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        SigninCheckerProvider.setForTests(mSigninCheckerMock);
        when(mPolicyLoadListenerMock.get()).thenReturn(false);
        when(mFirstRunPageDelegateMock.getPolicyLoadListener()).thenReturn(mPolicyLoadListenerMock);
        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);
        when(mUserPrefsJni.get(any())).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.SIGNIN_ALLOWED)).thenReturn(true);
        mFragment = new CustomSigninFirstRunFragment();
        mFragment.setPageDelegate(mFirstRunPageDelegateMock);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Promise<Void> nativeSideIsInitialized = new Promise<>();
                    nativeSideIsInitialized.fulfill(null);
                    when(mFirstRunPageDelegateMock.getNativeInitializationPromise())
                            .thenReturn(nativeSideIsInitialized);

                    OneshotSupplierImpl<Boolean> childAccountStatusListener =
                            new OneshotSupplierImpl<>();
                    childAccountStatusListener.set(false);
                    when(mFirstRunPageDelegateMock.getChildAccountStatusSupplier())
                            .thenReturn(childAccountStatusListener);
                });
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testFragmentRotationToLandscapeWithAccount() throws IOException {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        launchActivityWithFragment(Configuration.ORIENTATION_PORTRAIT);

        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_landscape");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentRotationToPortraitWithAccount_Legacy() throws IOException {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        launchActivityWithFragment(Configuration.ORIENTATION_LANDSCAPE);

        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_PORTRAIT);
        ViewUtils.onViewWaiting(
                allOf(withId(R.id.account_text_secondary), isCompletelyDisplayed()));
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_portrait_legacy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentRotationToPortraitWithAccount() throws IOException {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        launchActivityWithFragment(Configuration.ORIENTATION_LANDSCAPE);

        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_PORTRAIT);
        ViewUtils.onViewWaiting(
                allOf(withId(R.id.account_text_secondary), isCompletelyDisplayed()));
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_portrait");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @DisableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWithAccount_Legacy(boolean nightModeEnabled, int orientation)
            throws IOException {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_legacy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @EnableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWithAccount(boolean nightModeEnabled, int orientation)
            throws IOException {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithSupervisedAccount(boolean nightModeEnabled, int orientation)
            throws IOException {
        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_supervised_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @DisableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWithAccountOnManagedDevice_Legacy(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_managed_legacy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @EnableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWithAccountOnManagedDevice(boolean nightModeEnabled, int orientation)
            throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_managed");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @DisableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWithAccountOnManagedDevice_doesNotApplyFREStringVariations_Legacy(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_managed_and_string_variation_legacy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @EnableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWithAccountOnManagedDevice_doesNotApplyFREStringVariations(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_managed_and_string_variation");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithAccountWhenSigninIsDisabledByPolicy(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPrefService.getBoolean(Pref.SIGNIN_ALLOWED)).thenReturn(false);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_signin_disabled_by_policy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithAccountWhenSigninIsDisabledByPolicy_doesNotApplyFREStringVariation(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPrefService.getBoolean(Pref.SIGNIN_ALLOWED)).thenReturn(false);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_signin_disabled_by_policy_and_string_variation");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @DisableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWithoutAccount_Legacy(boolean nightModeEnabled, int orientation)
            throws IOException {
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_without_account_legacy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @EnableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWithoutAccount(boolean nightModeEnabled, int orientation)
            throws IOException {
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_without_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @DisableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWithoutAccountOnManagedDevice_Legacy(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_without_account_managed_legacy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @EnableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWithoutAccountOnManagedDevice(boolean nightModeEnabled, int orientation)
            throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_without_account_managed");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithChildAccount(boolean nightModeEnabled, int orientation)
            throws IOException {
        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_child_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @DisableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWithChildAccount_doesNotApplyFREStringVariation_Legacy(
            boolean nightModeEnabled, int orientation) throws IOException {
        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_child_account_and_string_variation_legacy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @EnableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWithChildAccount_doesNotApplyFREStringVariation(
            boolean nightModeEnabled, int orientation) throws IOException {
        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_child_account_and_string_variation");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWhenCannotUseGooglePlayService(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_signin_not_supported");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @DisableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWhenMetricsReportingIsDisabledByPolicy_Legacy(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);

        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_metrics_reporting_is_disabled_by_policy_legacy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @EnableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWhenMetricsReportingIsDisabledByPolicy(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);

        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_metrics_reporting_is_disabled_by_policy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @DisableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWhenMetricsReportingIsDisabledByPolicyWithAccount_Legacy(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);

        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);

        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_metrics_reporting_is_disabled_by_policy_with_account_legacy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @EnableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragmentWhenMetricsReportingIsDisabledByPolicyWithAccount(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);

        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);

        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_metrics_reporting_is_disabled_by_policy_with_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWhenMetricsReportingIsDisabledByPolicyWithChildAccount(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);

        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);

        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> mFragment.getView().findViewById(R.id.account_text_secondary).isShown());
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_metrics_reporting_is_disabled_by_policy_with_child_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @DisableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragment_WelcomeToChrome_EasierAcrossDevices_Legacy(
            boolean nightModeEnabled, int orientation) throws IOException {
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_welcome_to_chrome_easier_across_devices_legacy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    @EnableFeatures(SigninFeatures.FRE_SIGN_IN_ALTERNATIVE_SECONDARY_BUTTON_TEXT)
    public void testFragment_WelcomeToChrome_EasierAcrossDevices(
            boolean nightModeEnabled, int orientation) throws IOException {
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_welcome_to_chrome_easier_across_devices");
    }

    private void launchActivityWithFragment(int orientation) {
        ActivityTestUtils.rotateActivityToOrientation(mActivityTestRule.getActivity(), orientation);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, mFragment)
                            .commit();
                    // Set background color to the content view for better screenshot readability,
                    // especially in dark mode.
                    mActivityTestRule
                            .getActivity()
                            .findViewById(android.R.id.content)
                            .setBackgroundColor(
                                    SemanticColorUtils.getDefaultBgColor(
                                            mActivityTestRule.getActivity()));
                });
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
        // Parts of SigninFirstRunFragment are initialized asynchronously, so ensure the load
        // spinner is not displayed before grabbing a screenshot.
        //
        // In landscape in smaller screens, the continue button may be outside the screen bounds.
        int minDisplayedPercentage = orientation == Configuration.ORIENTATION_LANDSCAPE ? 0 : 51;
        waitForView(
                withId(R.id.signin_fre_continue_button),
                displayingAtLeastOption(minDisplayedPercentage));

        onView(withId(R.id.fre_native_and_policy_load_progress_spinner))
                .check(matches(not(isDisplayed())));
    }
}
