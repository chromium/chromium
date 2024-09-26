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
import static org.mockito.Mockito.when;

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
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.firstrun.FirstRunPageDelegate;
import org.chromium.chrome.browser.firstrun.PolicyLoadListener;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.SigninFirstRunFragmentTest.CustomSigninFirstRunFragment;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninChecker;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
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
public class SigninFirstRunFragmentRenderTest extends BlankUiTestActivityTestCase {
    /** Parameter provider for night mode state and device orientation. */
    public static class NightModeAndOrientationParameterProvider implements ParameterProvider {
        private static List<ParameterSet> sParams =
                Arrays.asList(
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled= */ false,
                                        Configuration.ORIENTATION_PORTRAIT)
                                .name("NightModeDisabled_Portrait"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled= */ false,
                                        Configuration.ORIENTATION_LANDSCAPE)
                                .name("NightModeDisabled_Landscape"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled= */ true,
                                        Configuration.ORIENTATION_PORTRAIT)
                                .name("NightModeEnabled_Portrait"),
                        new ParameterSet()
                                .value(
                                        /* nightModeEnabled= */ true,
                                        Configuration.ORIENTATION_LANDSCAPE)
                                .name("NightModeEnabled_Landscape"));

        @Override
        public Iterable<ParameterSet> getParameters() {
            return sParams;
        }
    }

    private static final String TEST_EMAIL1 = "test.account1@gmail.com";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_FIRST_RUN)
                    .build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private Profile mProfileMock;
    @Mock private ProfileProvider mProfileProviderMock;
    @Mock private ExternalAuthUtils mExternalAuthUtilsMock;
    @Mock private FirstRunPageDelegate mFirstRunPageDelegateMock;
    @Mock private PolicyLoadListener mPolicyLoadListenerMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private SyncService mSyncService;
    @Mock private SigninChecker mSigninCheckerMock;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock private PrivacyPreferencesManagerImpl mPrivacyPreferencesManagerMock;

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
        OneshotSupplierImpl<ProfileProvider> profileSupplier =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            OneshotSupplierImpl<ProfileProvider> supplier =
                                    new OneshotSupplierImpl<>();
                            when(mProfileProviderMock.getOriginalProfile())
                                    .thenReturn(mProfileMock);
                            supplier.set(mProfileProviderMock);
                            return supplier;
                        });
        when(mFirstRunPageDelegateMock.getProfileProviderSupplier()).thenReturn(profileSupplier);

        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(IdentityServicesProvider.get().getSigninManager(mProfileMock))
                            .thenReturn(mSigninManagerMock);
                    when(IdentityServicesProvider.get().getIdentityManager(mProfileMock))
                            .thenReturn(mIdentityManagerMock);
                });
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        SigninCheckerProvider.setForTests(mSigninCheckerMock);
        when(mPolicyLoadListenerMock.get()).thenReturn(false);
        when(mFirstRunPageDelegateMock.getPolicyLoadListener()).thenReturn(mPolicyLoadListenerMock);
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
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFragmentRotationToLandscapeWithAccount() throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);
        launchActivityWithFragment(Configuration.ORIENTATION_PORTRAIT);

        ActivityTestUtils.rotateActivityToOrientation(
                getActivity(), Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_landscape");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFragmentRotationToLandscapeWithAccount_replaceSyncWithSigninPromosEnabled()
            throws IOException {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        launchActivityWithFragment(Configuration.ORIENTATION_PORTRAIT);

        ActivityTestUtils.rotateActivityToOrientation(
                getActivity(), Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_landscape_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFragmentRotationToPortraitWithAccount() throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);
        launchActivityWithFragment(Configuration.ORIENTATION_LANDSCAPE);

        ActivityTestUtils.rotateActivityToOrientation(
                getActivity(), Configuration.ORIENTATION_PORTRAIT);
        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_portrait");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFragmentRotationToPortraitWithAccount_replaceSyncWithSigninPromosEnabled()
            throws IOException {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        launchActivityWithFragment(Configuration.ORIENTATION_LANDSCAPE);

        ActivityTestUtils.rotateActivityToOrientation(
                getActivity(), Configuration.ORIENTATION_PORTRAIT);
        ViewUtils.onViewWaiting(
                allOf(withId(R.id.account_text_secondary), isCompletelyDisplayed()));
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_portrait_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithAccount(boolean nightModeEnabled, int orientation)
            throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithAccount_replaceSyncWithSigninPromosEnabled(
            boolean nightModeEnabled, int orientation) throws IOException {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithAccountOnManagedDevice(boolean nightModeEnabled, int orientation)
            throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_managed");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithAccountOnManagedDevice_replaceSyncWithSigninPromosEnabled(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_managed_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithAccountOnManagedDevice_doesNotApplyFREStringVariations(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_managed_and_string_variation");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void
            testFragmentWithAccountOnManagedDevice_doesNotApplyFREStringVariations_replaceSyncWithSigninPromosEnabled(
                    boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_account_managed_and_string_variation_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithAccountWhenSigninIsDisabledByPolicy(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_signin_disabled_by_policy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void
            testFragmentWithAccountWhenSigninIsDisabledByPolicy_replaceSyncWithSigninPromosEnabled(
                    boolean nightModeEnabled, int orientation) throws IOException {
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_signin_disabled_by_policy_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithAccountWhenSigninIsDisabledByPolicy_doesNotApplyFREStringVariation(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_signin_disabled_by_policy_and_string_variation");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void
            testFragmentWithAccountWhenSigninIsDisabledByPolicy_doesNotApplyFREStringVariation_replaceSyncWithSigninEnabled(
                    boolean nightModeEnabled, int orientation) throws IOException {
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL1);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_signin_disabled_by_policy_and_string_variation_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithoutAccount(boolean nightModeEnabled, int orientation)
            throws IOException {
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_without_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithoutAccount_replaceSyncWithSignin(
            boolean nightModeEnabled, int orientation) throws IOException {
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_without_account_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithoutAccountOnManagedDevice(boolean nightModeEnabled, int orientation)
            throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_without_account_managed");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithoutAccountOnManagedDevice_replaceSyncWithSigninPromos(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_without_account_managed_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithChildAccount(boolean nightModeEnabled, int orientation)
            throws IOException {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_child_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithChildAccount_replaceSyncWithSigninPromosEnabled(
            boolean nightModeEnabled, int orientation) throws IOException {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_child_account_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWithChildAccount_doesNotApplyFREStringVariation(
            boolean nightModeEnabled, int orientation) throws IOException {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_child_account_and_string_variation");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void
            testFragmentWithChildAccount_doesNotApplyFREStringVariation_replaceSyncWithSigninPromosAnabled(
                    boolean nightModeEnabled, int orientation) throws IOException {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_with_child_account_and_string_variation_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWhenCannotUseGooglePlayService(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_signin_not_supported");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWhenCannotUseGooglePlayService_replaceSyncWithSigninEnabled(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_signin_not_supported_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWhenMetricsReportingIsDisabledByPolicy(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);

        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_metrics_reporting_is_disabled_by_policy");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWhenMetricsReportingIsDisabledByPolicy_replaceSyncWithSigninEnabled(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);

        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);

        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_metrics_reporting_is_disabled_by_policy_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWhenMetricsReportingIsDisabledByPolicyWithAccount(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);

        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);

        mAccountManagerTestRule.addAccount(TEST_EMAIL1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_metrics_reporting_is_disabled_by_policy_with_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void
            testFragmentWhenMetricsReportingIsDisabledByPolicyWithAccount_replaceSyncWithSigninPromosEnabled(
                    boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);

        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);

        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_metrics_reporting_is_disabled_by_policy_with_account_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragmentWhenMetricsReportingIsDisabledByPolicyWithChildAccount(
            boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);

        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);

        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_metrics_reporting_is_disabled_by_policy_with_child_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void
            testFragmentWhenMetricsReportingIsDisabledByPolicyWithChildAccount_replaceSyncWithSigninPromosEnabled(
                    boolean nightModeEnabled, int orientation) throws IOException {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);

        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);

        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);

        launchActivityWithFragment(orientation);

        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.account_text_secondary).isShown();
                });
        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_when_metrics_reporting_is_disabled_by_policy_with_child_account_replace_sync_with_signin_promos_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragment_WelcomeToChrome_EasierAcrossDevices(
            boolean nightModeEnabled, int orientation) throws IOException {
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_welcome_to_chrome_easier_across_devices");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @ParameterAnnotations.UseMethodParameter(NightModeAndOrientationParameterProvider.class)
    public void testFragment_WelcomeToChrome_EasierAcrossDevices_replaceSyncWithSigninPromoEnabled(
            boolean nightModeEnabled, int orientation) throws IOException {
        launchActivityWithFragment(orientation);

        mRenderTestRule.render(
                getActivity().findViewById(android.R.id.content),
                "signin_first_run_fragment_welcome_to_chrome_easier_across_devices_replace_sync_with_signin_promos_enabled");
    }

    private void launchActivityWithFragment(int orientation) {
        ActivityTestUtils.rotateActivityToOrientation(getActivity(), orientation);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity()
                            .getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, mFragment)
                            .commit();
                    // Set background color to the content view for better screenshot readability,
                    // especially in dark mode.
                    getActivity()
                            .findViewById(android.R.id.content)
                            .setBackgroundColor(
                                    SemanticColorUtils.getDefaultBgColor(getActivity()));
                });
        ApplicationTestUtils.waitForActivityState(getActivity(), Stage.RESUMED);
        // Parts of SigninFirstRunFragment are initialized asynchronously, so ensure the load
        // spinner is not displayed before grabbing a screenshot.
        ViewUtils.waitForVisibleView(withId(R.id.signin_fre_continue_button));
        onView(withId(R.id.fre_native_and_policy_load_progress_spinner))
                .check(matches(not(isDisplayed())));
    }
}
