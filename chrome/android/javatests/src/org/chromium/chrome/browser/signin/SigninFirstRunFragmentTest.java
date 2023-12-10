// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;
import static org.chromium.ui.test.util.MockitoHelper.doRunnable;

import android.accounts.Account;
import android.app.Activity;
import android.content.res.Configuration;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.widget.ProgressBar;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.test.espresso.ViewAction;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.enterprise.util.EnterpriseInfo;
import org.chromium.chrome.browser.enterprise.util.EnterpriseInfo.OwnedState;
import org.chromium.chrome.browser.enterprise.util.FakeEnterpriseInfo;
import org.chromium.chrome.browser.firstrun.FirstRunPageDelegate;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.firstrun.FirstRunUtilsJni;
import org.chromium.chrome.browser.firstrun.MobileFreProgress;
import org.chromium.chrome.browser.firstrun.PolicyLoadListener;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninChecker;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInCallback;
import org.chromium.chrome.browser.ui.signin.fre.SigninFirstRunMediator.LoadPoint;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.ViewUtils;

/** Tests for the class {@link SigninFirstRunFragment}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Relies on global state")
public class SigninFirstRunFragmentTest {
    private static final String TEST_EMAIL1 = "test.account1@gmail.com";
    private static final String FULL_NAME1 = "Test Account1";
    private static final String GIVEN_NAME1 = "Account1";
    private static final String TEST_EMAIL2 = "test.account2@gmail.com";
    private static final String CHILD_ACCOUNT_EMAIL =
            AccountManagerTestRule.generateChildEmail("account@gmail.com");
    private static final String CHILD_FULL_NAME = "Test Child";

    /** This class is used to test {@link SigninFirstRunFragment}. */
    public static class CustomSigninFirstRunFragment extends SigninFirstRunFragment {
        private FirstRunPageDelegate mFirstRunPageDelegate;

        @Override
        public FirstRunPageDelegate getPageDelegate() {
            return mFirstRunPageDelegate;
        }

        void setPageDelegate(FirstRunPageDelegate delegate) {
            mFirstRunPageDelegate = delegate;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public AutomotiveContextWrapperTestRule mAutoTestRule = new AutomotiveContextWrapperTestRule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule(BlankUiTestActivity.class);

    @Mock private ExternalAuthUtils mExternalAuthUtilsMock;
    @Mock private FirstRunPageDelegate mFirstRunPageDelegateMock;
    @Mock public FirstRunUtils.Natives mFirstRunUtils;
    @Mock private PolicyLoadListener mPolicyLoadListenerMock;
    @Mock private OneshotSupplierImpl<Boolean> mChildAccountStatusListenerMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private SigninChecker mSigninCheckerMock;
    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;
    @Captor private ArgumentCaptor<Callback<Boolean>> mCallbackCaptor;
    @Mock private PrivacyPreferencesManagerImpl mPrivacyPreferencesManagerMock;
    @Mock private ProfileProvider mProfileProvider;

    private Promise<Void> mNativeInitializationPromise;
    private final FakeEnterpriseInfo mFakeEnterpriseInfo = new FakeEnterpriseInfo();
    private CustomSigninFirstRunFragment mFragment;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppCompatDelegate.setDefaultNightMode(
                            nightModeEnabled
                                    ? AppCompatDelegate.MODE_NIGHT_YES
                                    : AppCompatDelegate.MODE_NIGHT_NO);
                });
    }

    @Before
    public void setUp() {
        // SigninTestRule requires access to Profile which in turn requires browser process to be
        // initialized. Calling this method in #setUpBeforeActivityLaunched() method causes a
        // crash.
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mSigninTestRule.waitForSeeding();

        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        EnterpriseInfo.setInstanceForTest(mFakeEnterpriseInfo);
        mFakeEnterpriseInfo.initialize(
                new OwnedState(/* isDeviceOwned= */ false, /* isProfileOwned= */ false));
        FirstRunUtils.setDisableDelayOnExitFreForTest(true);
        FirstRunUtilsJni.TEST_HOOKS.setInstanceForTesting(mFirstRunUtils);
        SigninCheckerProvider.setForTests(mSigninCheckerMock);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNativeInitializationPromise = new Promise<>();
                    mNativeInitializationPromise.fulfill(null);
                    // Use thenAnswer in case mNativeSideIsInitialized is changed in some tests.
                    when(mFirstRunPageDelegateMock.getNativeInitializationPromise())
                            .thenAnswer(ignored -> mNativeInitializationPromise);
                });

        when(mPolicyLoadListenerMock.get()).thenReturn(false);
        when(mFirstRunPageDelegateMock.getPolicyLoadListener()).thenReturn(mPolicyLoadListenerMock);
        when(mChildAccountStatusListenerMock.get()).thenReturn(false);
        when(mFirstRunPageDelegateMock.getChildAccountStatusSupplier())
                .thenReturn(mChildAccountStatusListenerMock);
        when(mFirstRunPageDelegateMock.isLaunchedFromCct()).thenReturn(false);

        OneshotSupplierImpl<ProfileProvider> profileSupplier =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> {
                            OneshotSupplierImpl<ProfileProvider> supplier =
                                    new OneshotSupplierImpl<>();
                            when(mProfileProvider.getOriginalProfile())
                                    .thenReturn(Profile.getLastUsedRegularProfile());
                            supplier.set(mProfileProvider);
                            return supplier;
                        });
        when(mFirstRunPageDelegateMock.getProfileProviderSupplier()).thenReturn(profileSupplier);

        mActivityTestRule.launchActivity(null);
        mFragment = new CustomSigninFirstRunFragment();
        mFragment.setPageDelegate(mFirstRunPageDelegateMock);
    }

    @Test
    @MediumTest
    public void testFragmentWhenAddingAccountDynamically() {
        launchActivityWithFragment();
        Assert.assertFalse(
                mFragment.getView().findViewById(R.id.signin_fre_selected_account).isShown());
        onView(withText(R.string.signin_add_account_to_device)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(isDisplayed()));

        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);

        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
    }

    @Test
    @MediumTest
    public void testFragmentWhenAddingChildAccountDynamically() {
        launchActivityWithFragment();
        onView(withText(R.string.signin_add_account_to_device)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(isDisplayed()));

        mSigninTestRule.addAccount(
                CHILD_ACCOUNT_EMAIL, CHILD_FULL_NAME, /* givenName= */ null, /* avatar= */ null);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);

        checkFragmentWithChildAccount(
                /* hasDisplayableFullName= */ true, /* hasDisplayableEmail= */ true);
    }

    @Test
    @MediumTest
    public void testFragmentWhenRemovingChildAccountDynamically() {
        mSigninTestRule.addAccount(
                CHILD_ACCOUNT_EMAIL, CHILD_FULL_NAME, /* givenName= */ null, /* avatar= */ null);
        launchActivityWithFragment();
        checkFragmentWithChildAccount(true, true);

        mSigninTestRule.removeAccount(CHILD_ACCOUNT_EMAIL);

        CriteriaHelper.pollUiThread(
                () -> {
                    return !mFragment
                            .getView()
                            .findViewById(R.id.signin_fre_selected_account)
                            .isShown();
                });
        onView(withText(R.string.signin_add_account_to_device)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_footer)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testFragmentWhenDefaultAccountIsRemoved() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, /* avatar= */ null);
        mSigninTestRule.addAccount(
                TEST_EMAIL2, /* fullName= */ null, /* givenName= */ null, /* avatar= */ null);
        launchActivityWithFragment();

        mSigninTestRule.removeAccount(TEST_EMAIL1);

        checkFragmentWithSelectedAccount(TEST_EMAIL2, /* fullName= */ null, /* givenName= */ null);
    }

    @Test
    @MediumTest
    public void testRemovingAllAccountsDismissesAccountPickerDialog() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, /* avatar= */ null);
        launchActivityWithFragment();
        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
        onView(withText(TEST_EMAIL1)).perform(click());
        onView(withText(R.string.signin_account_picker_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        mSigninTestRule.removeAccount(TEST_EMAIL1);

        onView(withText(R.string.signin_account_picker_dialog_title)).check(doesNotExist());
        onView(withText(R.string.signin_add_account_to_device)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testFragmentWithDefaultAccount() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);

        launchActivityWithFragment();

        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
        onView(withId(R.id.fre_browser_managed_by)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testFragmentWhenCannotUseGooglePlayService() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);

        launchActivityWithFragment();

        CriteriaHelper.pollUiThread(
                () -> {
                    return !mFragment
                            .getView()
                            .findViewById(R.id.signin_fre_selected_account)
                            .isShown();
                });
        ViewUtils.waitForVisibleView(withText(R.string.continue_button));
        onView(withId(R.id.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));
        ViewUtils.waitForVisibleView(withId(R.id.signin_fre_footer));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1434098")
    public void testFragmentWhenSigninIsDisabledByPolicy() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(IdentityServicesProvider.get()
                                    .getSigninManager(Profile.getLastUsedRegularProfile()))
                            .thenReturn(mSigninManagerMock);
                });
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);

        launchActivityWithFragment();

        checkFragmentWhenSigninIsDisabledByPolicy();
    }

    @Test
    @MediumTest
    public void testFragmentWhenSigninErrorOccurs() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(IdentityServicesProvider.get()
                                    .getSigninManager(Profile.getLastUsedRegularProfile()))
                            .thenReturn(mSigninManagerMock);
                    // IdentityManager#getPrimaryAccountInfo() is called during this test flow by
                    // SigninFirstRunMediator.
                    when(IdentityServicesProvider.get()
                                    .getIdentityManager(Profile.getLastUsedRegularProfile()))
                            .thenReturn(mIdentityManagerMock);
                });
        doCallback(/* index= */ 2, (SignInCallback callback) -> callback.onSignInAborted())
                .when(mSigninManagerMock)
                .signin(eq(AccountUtils.createAccountFromName(TEST_EMAIL1)), anyInt(), any());
        launchActivityWithFragment();
        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);

        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, GIVEN_NAME1);
        clickContinueButton(continueAsText);

        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
        verify(mFirstRunPageDelegateMock, never()).advanceToNextPage();
        // TODO(crbug/1248090): For now we enable the buttons again to not block the users from
        // continuing to the next page. Should show a dialog with the signin error.
        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1434098")
    public void testFragmentWhenAddingAccountDynamicallyAndSigninIsDisabledByPolicy() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(IdentityServicesProvider.get()
                                    .getSigninManager(Profile.getLastUsedRegularProfile()))
                            .thenReturn(mSigninManagerMock);
                });
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        launchActivityWithFragment();
        checkFragmentWhenSigninIsDisabledByPolicy();

        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);

        checkFragmentWhenSigninIsDisabledByPolicy();
    }

    @Test
    @MediumTest
    public void testContinueButtonWhenCannotUseGooglePlayService() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);
        launchActivityWithFragment();
        CriteriaHelper.pollUiThread(
                () -> {
                    return !mFragment
                            .getView()
                            .findViewById(R.id.signin_fre_selected_account)
                            .isShown();
                });

        onView(withText(R.string.continue_button)).perform(click());

        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
        verify(mFirstRunPageDelegateMock).advanceToNextPage();
        verify(mFirstRunPageDelegateMock, never()).recordFreProgressHistogram(anyInt());
    }

    @Test
    @MediumTest
    public void testFragmentWhenChoosingAnotherAccount() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        mSigninTestRule.addAccount(
                TEST_EMAIL2, /* fullName= */ null, /* givenName= */ null, /* avatar= */ null);
        launchActivityWithFragment();
        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);

        onView(withText(TEST_EMAIL1)).perform(click());
        onView(withText(TEST_EMAIL2)).inRoot(isDialog()).perform(click());

        checkFragmentWithSelectedAccount(TEST_EMAIL2, /* fullName= */ null, /* givenName= */ null);
        onView(withId(R.id.fre_browser_managed_by)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testFragmentWithDefaultAccountWhenPolicyAvailableOnDevice() {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);

        launchActivityWithFragment();

        checkFragmentWithSelectedAccount(
                TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, /* shouldShowSubtitle= */ false);
        onView(withId(R.id.fre_browser_managed_by)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testFragmentWithChildAccount() {
        mSigninTestRule.addAccount(
                CHILD_ACCOUNT_EMAIL, CHILD_FULL_NAME, /* givenName= */ null, /* avatar= */ null);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);

        launchActivityWithFragment();
        checkFragmentWithChildAccount(
                /* hasDisplayableFullName= */ true, /* hasDisplayableEmail= */ true);
    }

    @Test
    @MediumTest
    public void testFragmentWithChildAccountWithNonDisplayableAccountEmail() {
        mSigninTestRule.addAccount(
                CHILD_ACCOUNT_EMAIL,
                CHILD_FULL_NAME,
                /* givenName= */ null,
                /* avatar= */ null,
                SigninTestRule.NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);

        launchActivityWithFragment();

        checkFragmentWithChildAccount(
                /* hasDisplayableFullName= */ true, /* hasDisplayableEmail= */ false);
    }

    @Test
    @MediumTest
    public void testFragmentWithChildAccountWithNonDisplayableAccountEmailWithEmptyDisplayName() {
        mSigninTestRule.addAccount(
                CHILD_ACCOUNT_EMAIL,
                /* fullName= */ null,
                /* givenName= */ null,
                /* avatar= */ null,
                SigninTestRule.NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);

        launchActivityWithFragment();

        checkFragmentWithChildAccount(
                /* hasDisplayableFullName= */ false, /* hasDisplayableEmail= */ false);
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testSigninWithDefaultAccount() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        launchActivityWithFragment();
        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, GIVEN_NAME1);

        onView(withText(continueAsText)).perform(click());
        // ToS should be accepted right away, without waiting for the sign-in to complete.
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);

        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
                });
        final CoreAccountInfo primaryAccount =
                mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN);
        Assert.assertEquals(TEST_EMAIL1, primaryAccount.getEmail());
        // Sign-in has completed, so the FRE should advance to the next page.
        verify(mFirstRunPageDelegateMock).advanceToNextPage();
        verify(mFirstRunPageDelegateMock)
                .recordFreProgressHistogram(MobileFreProgress.WELCOME_SIGNIN_WITH_DEFAULT_ACCOUNT);
    }

    @Test
    @MediumTest
    public void testContinueButton_automotiveDevice_signInWithDefaultAccount() {
        mAutoTestRule.setIsAutomotive(true);

        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        launchActivityWithFragment();
        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, GIVEN_NAME1);

        // Click and continue to the device lock page
        onView(withText(continueAsText)).perform(click());
        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.device_lock_title).isShown();
                });

        // Verify that sign-in has not proceeded
        verify(mFirstRunPageDelegateMock, never()).acceptTermsOfService(anyBoolean());
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));

        // Continue past the device lock page
        TestThreadUtils.runOnUiThreadBlocking(() -> mFragment.onDeviceLockReady());

        // ToS should be accepted right away, without waiting for the sign-in to complete.
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);

        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
                });
        final CoreAccountInfo primaryAccount =
                mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN);
        Assert.assertEquals(TEST_EMAIL1, primaryAccount.getEmail());
        // Sign-in has completed, so the FRE should advance to the next page.
        verify(mFirstRunPageDelegateMock).advanceToNextPage();
        verify(mFirstRunPageDelegateMock)
                .recordFreProgressHistogram(MobileFreProgress.WELCOME_SIGNIN_WITH_DEFAULT_ACCOUNT);
    }

    @Test
    @MediumTest
    public void testContinueButton_automotiveDevice_dismissSignInFromDeviceLockPage() {
        mAutoTestRule.setIsAutomotive(true);

        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        launchActivityWithFragment();
        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, GIVEN_NAME1);

        // Click and continue to the device lock page
        onView(withText(continueAsText)).perform(click());
        CriteriaHelper.pollUiThread(
                () -> {
                    return mFragment.getView().findViewById(R.id.device_lock_title).isShown();
                });

        // Verify that sign-in has not proceeded
        verify(mFirstRunPageDelegateMock, never()).acceptTermsOfService(anyBoolean());
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));

        // Continue past the device lock page
        TestThreadUtils.runOnUiThreadBlocking(() -> mFragment.onDeviceLockRefused());

        CriteriaHelper.pollUiThread(
                () -> {
                    return !IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
                });
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
        verify(mFirstRunPageDelegateMock).advanceToNextPage();
        verify(mFirstRunPageDelegateMock)
                .recordFreProgressHistogram(MobileFreProgress.WELCOME_DISMISS);
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testSigninWithNonDefaultAccount() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, /* avatar= */ null);
        mSigninTestRule.addAccount(
                TEST_EMAIL2, /* fullName= */ null, /* givenName= */ null, /* avatar= */ null);
        launchActivityWithFragment();
        onView(withText(TEST_EMAIL1)).perform(click());
        onView(withText(TEST_EMAIL2)).inRoot(isDialog()).perform(click());
        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, TEST_EMAIL2);

        ViewUtils.onViewWaiting(withText(continueAsText)).perform(click());

        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
                });
        final CoreAccountInfo primaryAccount =
                mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN);
        Assert.assertEquals(TEST_EMAIL2, primaryAccount.getEmail());
        verify(mFirstRunPageDelegateMock)
                .recordFreProgressHistogram(
                        MobileFreProgress.WELCOME_SIGNIN_WITH_NON_DEFAULT_ACCOUNT);
    }

    @Test
    @MediumTest
    @Restriction({DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testContinueButtonWithAnAccountOtherThanTheSignedInAccount() {
        final CoreAccountInfo targetPrimaryAccount =
                mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        final CoreAccountInfo primaryAccount = mSigninTestRule.addTestAccountThenSignin();
        Assert.assertNotEquals(
                "The primary account should be a different account!",
                targetPrimaryAccount.getEmail(),
                primaryAccount.getEmail());
        launchActivityWithFragment();

        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, GIVEN_NAME1);
        onView(withText(continueAsText)).perform(click());

        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
        CriteriaHelper.pollUiThread(
                () -> {
                    return targetPrimaryAccount.equals(
                            IdentityServicesProvider.get()
                                    .getIdentityManager(Profile.getLastUsedRegularProfile())
                                    .getPrimaryAccountInfo(ConsentLevel.SIGNIN));
                });
        verify(mFirstRunPageDelegateMock).advanceToNextPage();
    }

    @Test
    @MediumTest
    public void testContinueButtonWithTheSignedInAccount() {
        final CoreAccountInfo signedInAccount =
                mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        when(mIdentityManagerMock.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(signedInAccount);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(IdentityServicesProvider.get()
                                    .getSigninManager(Profile.getLastUsedRegularProfile()))
                            .thenReturn(mSigninManagerMock);
                    // IdentityManager#getPrimaryAccountInfo() is called during this test flow by
                    // SigninFirstRunMediator.
                    when(IdentityServicesProvider.get()
                                    .getIdentityManager(Profile.getLastUsedRegularProfile()))
                            .thenReturn(mIdentityManagerMock);
                });
        launchActivityWithFragment();

        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, GIVEN_NAME1);
        clickContinueButton(continueAsText);

        verify(mSigninManagerMock, never()).signin(any(Account.class), anyInt(), any());
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
        verify(mFirstRunPageDelegateMock).advanceToNextPage();
    }

    @Test
    @MediumTest
    public void testDismissButtonWhenUserIsSignedIn() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        final CoreAccountInfo primaryAccount = mSigninTestRule.addTestAccountThenSignin();
        Assert.assertNotEquals(
                "The primary account should be a different account!",
                TEST_EMAIL1,
                primaryAccount.getEmail());
        launchActivityWithFragment();

        onView(withText(R.string.signin_fre_dismiss_button)).perform(click());

        CriteriaHelper.pollUiThread(
                () -> {
                    return !IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
                });
        waitForEvent(mFirstRunPageDelegateMock).acceptTermsOfService(true);
        waitForEvent(mFirstRunPageDelegateMock).advanceToNextPage();
        waitForEvent(mFirstRunPageDelegateMock)
                .recordFreProgressHistogram(MobileFreProgress.WELCOME_DISMISS);
    }

    @Test
    @MediumTest
    public void testDismissButtonWithDefaultAccount() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        launchActivityWithFragment();

        onView(withText(R.string.signin_fre_dismiss_button)).perform(click());
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
        verify(mFirstRunPageDelegateMock).advanceToNextPage();
        verify(mFirstRunPageDelegateMock)
                .recordFreProgressHistogram(MobileFreProgress.WELCOME_DISMISS);
    }

    @Test
    @MediumTest
    public void testContinueButtonWithChildAccount() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(IdentityServicesProvider.get()
                                    .getSigninManager(Profile.getLastUsedRegularProfile()))
                            .thenReturn(mSigninManagerMock);
                });

        mSigninTestRule.addAccount(
                CHILD_ACCOUNT_EMAIL, CHILD_FULL_NAME, /* givenName= */ null, /* avatar= */ null);

        checkContinueButtonWithChildAccount(/* hasFullNameInButtonText= */ true);
    }

    @Test
    @MediumTest
    public void testContinueButtonWithChildAccountWithNonDisplayableAccountEmail() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(IdentityServicesProvider.get()
                                    .getSigninManager(Profile.getLastUsedRegularProfile()))
                            .thenReturn(mSigninManagerMock);
                });

        mSigninTestRule.addAccount(
                CHILD_ACCOUNT_EMAIL,
                CHILD_FULL_NAME,
                /* givenName= */ null,
                /* avatar= */ null,
                SigninTestRule.NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES);

        checkContinueButtonWithChildAccount(/* hasFullNameInButtonText= */ true);
    }

    @Test
    @MediumTest
    public void
            testContinueButtonWithChildAccountWithNonDisplayableAccountEmailWithEmptyDisplayName() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(IdentityServicesProvider.get()
                                    .getSigninManager(Profile.getLastUsedRegularProfile()))
                            .thenReturn(mSigninManagerMock);
                });

        mSigninTestRule.addAccount(
                CHILD_ACCOUNT_EMAIL,
                /* fullName= */ null,
                /* givenName= */ null,
                /* avatar= */ null,
                SigninTestRule.NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES);

        checkContinueButtonWithChildAccount(/* hasFullNameInButtonText= */ false);
    }

    @Test
    @MediumTest
    public void testProgressSpinnerOnContinueButtonPress() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(IdentityServicesProvider.get()
                                    .getSigninManager(Profile.getLastUsedRegularProfile()))
                            .thenReturn(mSigninManagerMock);
                    // IdentityManager#getPrimaryAccountInfo() is called during this test flow by
                    // SigninFirstRunMediator.
                    when(IdentityServicesProvider.get()
                                    .getIdentityManager(Profile.getLastUsedRegularProfile()))
                            .thenReturn(mIdentityManagerMock);
                });
        launchActivityWithFragment();

        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, GIVEN_NAME1);
        clickContinueButton(continueAsText);

        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
        onView(withId(R.id.fre_signin_progress_spinner)).check(matches(isDisplayed()));
        onView(withText(R.string.fre_signing_in)).check(matches(isDisplayed()));
        onView(withText(R.string.fre_welcome)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_subtitle)).check(matches(isDisplayed()));
        onView(withText(TEST_EMAIL1)).check(matches(not(isDisplayed())));
        onView(withText(FULL_NAME1)).check(matches(not(isDisplayed())));
        onView(withId(R.id.signin_fre_selected_account_expand_icon))
                .check(matches(not(isDisplayed())));
        onView(withText(continueAsText)).check(matches(not(isDisplayed())));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.signin_fre_footer)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testFragmentWhenClickingOnTosLink() {
        launchActivityWithFragment();

        onView(withId(R.id.signin_fre_footer)).perform(clickOnTosLink());

        verify(mFirstRunPageDelegateMock).showInfoPage(R.string.google_terms_of_service_url);
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testFragmentWhenClickingOnTosLinkInDarkMode(boolean nightModeEnabled) {
        launchActivityWithFragment();

        onView(withId(R.id.signin_fre_footer)).perform(clickOnTosLink());

        verify(mFirstRunPageDelegateMock)
                .showInfoPage(
                        nightModeEnabled
                                ? R.string.google_terms_of_service_dark_mode_url
                                : R.string.google_terms_of_service_url);
    }

    @Test
    @MediumTest
    public void testFragmentWhenClickingOnUmaDialogLink() {
        launchActivityWithFragment();

        clickOnUmaDialogLinkAndWait();

        onView(withText(R.string.signin_fre_uma_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withId(R.id.fre_uma_dialog_switch)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_uma_dialog_first_section_header))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_uma_dialog_first_section_body))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_uma_dialog_second_section_header))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_uma_dialog_second_section_body))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.done)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testFragmentWhenDismissingUmaDialog() {
        launchActivityWithFragment();
        clickOnUmaDialogLinkAndWait();

        onView(withText(R.string.done)).perform(click());

        onView(withText(R.string.signin_fre_uma_dialog_title)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testDismissButtonWhenAllowCrashUploadTurnedOff() {
        launchActivityWithFragment();
        clickOnUmaDialogLinkAndWait();
        onView(withId(R.id.fre_uma_dialog_switch)).perform(click());
        onView(withText(R.string.done)).perform(click());

        onView(withText(R.string.signin_fre_dismiss_button)).perform(click());

        verify(mFirstRunPageDelegateMock).acceptTermsOfService(false);
        verify(mFirstRunPageDelegateMock, timeout(1000)).advanceToNextPage();
    }

    @Test
    @MediumTest
    public void testUmaDialogSwitchIsOffWhenAllowCrashUploadWasTurnedOffBefore() {
        launchActivityWithFragment();
        clickOnUmaDialogLinkAndWait();
        onView(withId(R.id.fre_uma_dialog_switch)).check(matches(isChecked())).perform(click());
        onView(withText(R.string.done)).perform(click());

        clickOnUmaDialogLinkAndWait();

        onView(withId(R.id.fre_uma_dialog_switch))
                .check(matches(not(isChecked())))
                .perform(click());
        onView(withText(R.string.done)).perform(click());
        onView(withText(R.string.signin_fre_dismiss_button)).perform(click());

        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
        verify(mFirstRunPageDelegateMock).advanceToNextPage();
    }

    @Test
    @MediumTest
    public void testContinueButtonWhenAllowCrashUploadTurnedOff() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        launchActivityWithFragment();
        clickOnUmaDialogLinkAndWait();
        onView(withId(R.id.fre_uma_dialog_switch)).perform(click());
        onView(withText(R.string.done)).perform(click());

        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, GIVEN_NAME1);
        clickContinueButton(continueAsText);

        verify(mFirstRunPageDelegateMock).acceptTermsOfService(false);
        verify(mFirstRunPageDelegateMock, timeout(2000)).advanceToNextPage();
    }

    @Test
    @MediumTest
    public void testFragmentWhenAddingAnotherAccount() {
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_OK, TEST_EMAIL2);
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        launchActivityWithFragment();
        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);

        onView(withText(TEST_EMAIL1)).perform(click());
        onView(withText(R.string.signin_add_account_to_device)).perform(click());

        checkFragmentWithSelectedAccount(TEST_EMAIL2, /* fullName= */ null, /* givenName= */ null);
        verify(mFirstRunPageDelegateMock)
                .recordFreProgressHistogram(MobileFreProgress.WELCOME_ADD_ACCOUNT);
    }

    @Test
    @MediumTest
    public void testFragmentWhenAddingDefaultAccount() {
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_OK, TEST_EMAIL1);
        launchActivityWithFragment();

        onView(withText(R.string.signin_add_account_to_device)).perform(click());

        checkFragmentWithSelectedAccount(TEST_EMAIL1, /* fullName= */ null, /* givenName= */ null);
        verify(mFirstRunPageDelegateMock)
                .recordFreProgressHistogram(MobileFreProgress.WELCOME_ADD_ACCOUNT);
    }

    @Test
    @MediumTest
    public void testFragmentWhenPolicyIsLoadedAfterNativeAndChildStatus() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        when(mPolicyLoadListenerMock.get()).thenReturn(null);
        launchActivityWithFragment();
        checkFragmentWhenLoadingNativeAndPolicy();
        var slowestPointHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "MobileFre.SlowestLoadPoint", LoadPoint.POLICY_LOAD);

        // TODO(https://crbug.com/1346258): Use OneshotSupplierImpl instead.
        when(mPolicyLoadListenerMock.get()).thenReturn(false);
        verify(mPolicyLoadListenerMock, atLeastOnce()).onAvailable(mCallbackCaptor.capture());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (Callback<Boolean> callback : mCallbackCaptor.getAllValues()) {
                        callback.onResult(false);
                    }
                });

        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
        slowestPointHistogram.assertExpected(
                "Policy loading should be the slowest and SlowestLoadpoint "
                        + "histogram should be counted only once");
    }

    @Test
    @MediumTest
    public void testFragmentWhenNativeIsLoadedAfterPolicyAndChildStatus() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNativeInitializationPromise = new Promise<>();
                });
        launchActivityWithFragment();
        checkFragmentWhenLoadingNativeAndPolicy();
        var slowestPointHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "MobileFre.SlowestLoadPoint", LoadPoint.NATIVE_INITIALIZATION);

        TestThreadUtils.runOnUiThreadBlocking(() -> mNativeInitializationPromise.fulfill(null));

        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
        slowestPointHistogram.assertExpected(
                "Native initialization should be the slowest and "
                        + "SlowestLoadpoint histogram should be counted only once");
        verify(mFirstRunPageDelegateMock).recordNativeInitializedHistogram();
    }

    @Test
    @MediumTest
    public void testFragmentWhenChildStatusIsLoadedAfterNativeAndPolicy() {
        mSigninTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        when(mChildAccountStatusListenerMock.get()).thenReturn(null);
        launchActivityWithFragment();
        checkFragmentWhenLoadingNativeAndPolicy();
        var slowestPointHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "MobileFre.SlowestLoadPoint", LoadPoint.CHILD_STATUS_LOAD);

        // TODO(https://crbug.com/1346258): Use OneshotSupplierImpl instead.
        when(mChildAccountStatusListenerMock.get()).thenReturn(false);
        verify(mChildAccountStatusListenerMock, atLeastOnce())
                .onAvailable(mCallbackCaptor.capture());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (Callback<Boolean> callback : mCallbackCaptor.getAllValues()) {
                        callback.onResult(false);
                    }
                });

        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
        slowestPointHistogram.assertExpected(
                "Child status loading should be the slowest and "
                        + "SlowestLoadpoint histogram should be counted only once");
    }

    @Test
    @MediumTest
    public void testNativePolicyAndChildStatusLoadMetricRecordedOnlyOnce() {
        var slowestPointHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "MobileFre.SlowestLoadPoint", LoadPoint.NATIVE_INITIALIZATION);
        launchActivityWithFragment();
        verify(mFirstRunPageDelegateMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .recordNativePolicyAndChildStatusLoadedHistogram();
        verify(mFirstRunPageDelegateMock).recordNativeInitializedHistogram();
        slowestPointHistogram.assertExpected("Native initialization should be the slowest");

        slowestPointHistogram =
                HistogramWatcher.newBuilder().expectNoRecords("MobileFre.SlowestLoadPoint").build();

        // Changing the activity orientation will create SigninFirstRunCoordinator again and call
        // SigninFirstRunFragment.notifyCoordinatorWhenNativePolicyAndChildStatusAreLoaded()
        ActivityTestUtils.rotateActivityToOrientation(
                mActivityTestRule.getActivity(), Configuration.ORIENTATION_LANDSCAPE);

        // These histograms should not be recorded again. The call count should be the same as
        // before as mockito does not reset invocation counts between consecutive verify calls.
        verify(mFirstRunPageDelegateMock).recordNativePolicyAndChildStatusLoadedHistogram();
        verify(mFirstRunPageDelegateMock).recordNativeInitializedHistogram();
        slowestPointHistogram.assertExpected(
                "SlowestLoadPoint histogram should not be recorded again");
    }

    @Test
    @MediumTest
    public void testFragmentWithTosDialogBehaviorPolicy() throws Exception {
        CallbackHelper callbackHelper = new CallbackHelper();
        doRunnable(callbackHelper::notifyCalled).when(mFirstRunPageDelegateMock).exitFirstRun();
        when(mFirstRunPageDelegateMock.isLaunchedFromCct()).thenReturn(true);
        mFakeEnterpriseInfo.initialize(
                new OwnedState(/* isDeviceOwned= */ true, /* isProfileOwned= */ false));
        doCallback((Callback<Boolean> callback) -> callback.onResult(true))
                .when(mPolicyLoadListenerMock)
                .onAvailable(any());
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mFirstRunUtils.getCctTosDialogEnabled()).thenReturn(false);
        launchActivityWithFragment();

        callbackHelper.waitForFirst();
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(false);
        verify(mFirstRunPageDelegateMock).exitFirstRun();
    }

    @Test
    @MediumTest
    public void testFragmentWithMetricsReportingDisabled() throws Exception {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        when(mPrivacyPreferencesManagerMock.isUsageAndCrashReportingPermittedByPolicy())
                .thenReturn(false);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyPreferencesManagerMock);
        launchActivityWithFragment();

        onView(withText(R.string.signin_fre_dismiss_button)).perform(click());

        verify(mFirstRunPageDelegateMock).acceptTermsOfService(false);
        verify(mFirstRunPageDelegateMock).advanceToNextPage();
    }



    @Test
    @MediumTest
    public void testFragment_WelcomeToChrome_EasierAcrossDevices() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mNativeInitializationPromise = new Promise<>();
                });
        launchActivityWithFragment();
        onView(withId(R.id.fre_native_and_policy_load_progress_spinner))
                .check(matches(isDisplayed()));
        onView(withId(R.id.title)).check(matches(isDisplayed()));
        onView(withId(R.id.subtitle)).check(matches(not(isDisplayed())));

        TestThreadUtils.runOnUiThreadBlocking(() -> mNativeInitializationPromise.fulfill(null));

        onView(withText(R.string.fre_welcome)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_subtitle)).check(matches(isDisplayed()));
    }




    @Test
    @MediumTest
    public void testFragmentWithChildAccount_doesNotApplyFREStringVariation() {
        mSigninTestRule.addAccount(
                CHILD_ACCOUNT_EMAIL, CHILD_FULL_NAME, /* givenName= */ null, /* avatar= */ null);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);

        launchActivityWithFragment();
        checkFragmentWithChildAccount(
                /* hasDisplayableFullName= */ true, /* hasDisplayableEmail= */ true);
    }

    @Test
    @MediumTest
    public void testDismissWithTosDialogBehaviorPolicy() throws Exception {
        reset(mPolicyLoadListenerMock);
        when(mPolicyLoadListenerMock.onAvailable(any())).thenReturn(null);
        when(mFirstRunPageDelegateMock.isLaunchedFromCct()).thenReturn(true);
        mFakeEnterpriseInfo.initialize(
                new OwnedState(/* isDeviceOwned= */ true, /* isProfileOwned= */ false));
        when(mFirstRunUtils.getCctTosDialogEnabled()).thenReturn(false);
        launchActivityWithFragment();

        // Detach the current fragment. Needs to be done before the PolicyLoadListener callback
        // otherwise this test is racy.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ((BlankUiTestActivity) mActivityTestRule.getActivity())
                            .getSupportFragmentManager()
                            .beginTransaction()
                            .detach(mFragment)
                            .commit();
                });
        CriteriaHelper.pollUiThread(() -> mFragment.isDetached());
        mFragment.setPageDelegate(null);

        // Emulate policy loading being completed, and the ToS behavior policy wants to skip the
        // ToS/FRE. The fragment should now start waiting some duration.
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        verify(mPolicyLoadListenerMock, atLeastOnce()).onAvailable(mCallbackCaptor.capture());

        // Wait for the delayed task to run. Although this test setup reduces delay to 0 seconds.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCallbackCaptor.getValue().onResult(true);
                });

        // Delayed task should run, but not call into the delegate.
        CriteriaHelper.pollUiThread(mFragment::getDelayedExitFirstRunCalledForTesting);
        verify(mFirstRunPageDelegateMock, never()).acceptTermsOfService(false);
        verify(mFirstRunPageDelegateMock, never()).exitFirstRun();
    }

    private void checkFragmentWithSelectedAccount(
            String email, String fullName, String givenName, boolean shouldShowSubtitle) {
        CriteriaHelper.pollUiThread(
                mFragment.getView().findViewById(R.id.signin_fre_selected_account)::isShown);
        verify(mFirstRunPageDelegateMock).recordNativePolicyAndChildStatusLoadedHistogram();
        final DisplayableProfileData profileData =
                new DisplayableProfileData(email, mock(Drawable.class), fullName, givenName, true);
        onView(withText(R.string.fre_welcome)).check(matches(isDisplayed()));
        if (shouldShowSubtitle) {
            onView(withText(R.string.signin_fre_subtitle)).check(matches(isDisplayed()));
        } else {
            onView(withId(R.id.subtitle)).check(matches(not(isDisplayed())));
        }
        onView(withText(email)).check(matches(isDisplayed()));
        if (fullName != null) {
            onView(withText(fullName)).check(matches(isDisplayed()));
        }
        onView(withId(R.id.signin_fre_selected_account_expand_icon)).check(matches(isDisplayed()));
        final String continueAsText =
                mFragment.getString(
                        R.string.sync_promo_continue_as,
                        profileData.getGivenNameOrFullNameOrEmail());
        onView(withText(continueAsText)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_footer)).check(matches(isDisplayed()));
    }

    private void checkFragmentWithSelectedAccount(String email, String fullName, String givenName) {
        checkFragmentWithSelectedAccount(email, fullName, givenName, true);
    }

    private void checkFragmentWhenLoadingNativeAndPolicy() {
        onView(withId(R.id.fre_native_and_policy_load_progress_spinner))
                .check(matches(isDisplayed()));
        onView(withText(R.string.fre_welcome)).check(matches(isDisplayed()));
        onView(withId(R.id.subtitle)).check(matches(not(isDisplayed())));
        onView(withText(TEST_EMAIL1)).check(matches(not(isDisplayed())));
        onView(withText(FULL_NAME1)).check(matches(not(isDisplayed())));
        onView(withId(R.id.signin_fre_selected_account_expand_icon))
                .check(matches(not(isDisplayed())));
        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, GIVEN_NAME1);
        onView(withText(continueAsText)).check(matches(not(isDisplayed())));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.signin_fre_footer)).check(matches(not(isDisplayed())));
        verify(mPolicyLoadListenerMock, atLeastOnce()).onAvailable(notNull());
    }

    private void checkFragmentWithChildAccount(
            boolean hasDisplayableFullName, boolean hasDisplayableEmail) {
        CriteriaHelper.pollUiThread(
                mFragment.getView().findViewById(R.id.signin_fre_selected_account)::isShown);
        verify(mFirstRunPageDelegateMock).recordNativePolicyAndChildStatusLoadedHistogram();
        onView(withText(R.string.fre_welcome)).check(matches(isDisplayed()));
        onView(withId(R.id.subtitle)).check(matches(not(isDisplayed())));
        Assert.assertFalse(
                mFragment.getView().findViewById(R.id.signin_fre_selected_account).isEnabled());
        if (hasDisplayableEmail) {
            onView(withText(CHILD_ACCOUNT_EMAIL)).check(matches(isDisplayed()));
        } else {
            onView(withText(CHILD_ACCOUNT_EMAIL)).check(doesNotExist());
        }
        if (hasDisplayableFullName) {
            onView(withText(CHILD_FULL_NAME)).check(matches(isDisplayed()));
        } else {
            onView(withText(mFragment.getString(R.string.default_google_account_username)))
                    .check(matches(isDisplayed()));
        }
        onView(withId(R.id.signin_fre_selected_account_expand_icon))
                .check(matches(not(isDisplayed())));
        final String continueAsText =
                hasDisplayableFullName
                        ? mFragment.getString(R.string.sync_promo_continue_as, CHILD_FULL_NAME)
                        : mFragment.getString(R.string.sync_promo_continue);
        onView(withText(continueAsText)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_footer)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.fre_browser_managed_by)).check(matches(isDisplayed()));
        onView(withId(R.id.privacy_disclaimer)).check(matches(isDisplayed()));
        onView(withText(R.string.fre_browser_managed_by_parent)).check(matches(isDisplayed()));
    }

    private void checkContinueButtonWithChildAccount(boolean hasFullNameInButtonText) {
        launchActivityWithFragment();
        final String continueAsText =
                hasFullNameInButtonText
                        ? mActivityTestRule
                                .getActivity()
                                .getString(R.string.sync_promo_continue_as, CHILD_FULL_NAME)
                        : mActivityTestRule.getActivity().getString(R.string.sync_promo_continue);

        clickContinueButton(continueAsText);

        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
        verify(mFirstRunPageDelegateMock).advanceToNextPage();

        // Sign-in isn't processed by SigninFirstRunFragment for child accounts.
        verify(mSigninManagerMock, never()).signin(any(Account.class), anyInt(), any());
        verify(mSigninManagerMock, never())
                .signinAndEnableSync(any(Account.class), anyInt(), any());
    }

    private void checkFragmentWhenSigninIsDisabledByPolicy() {
        CriteriaHelper.pollUiThread(
                () -> {
                    return !mFragment
                            .getView()
                            .findViewById(R.id.signin_fre_selected_account)
                            .isShown();
                });
        verify(mFirstRunPageDelegateMock).recordNativePolicyAndChildStatusLoadedHistogram();
        ViewUtils.waitForVisibleView(withId(R.id.fre_browser_managed_by));
        ViewUtils.waitForVisibleView(withText(R.string.continue_button));
        ViewUtils.waitForVisibleView(withId(R.id.signin_fre_footer));
        onView(withId(R.id.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));
    }

    private void launchActivityWithFragment() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ((BlankUiTestActivity) mActivityTestRule.getActivity())
                            .getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, mFragment)
                            .commit();
                });
        // Wait for fragment to be added to the activity.
        CriteriaHelper.pollUiThread(() -> mFragment.isResumed());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Replace all the progress bars with dummies. Currently the progress bar cannot
                    // be stopped otherwise due to some espresso issues (crbug/1115067).
                    ProgressBar nativeAndPolicyProgressBar =
                            mFragment
                                    .getView()
                                    .findViewById(R.id.fre_native_and_policy_load_progress_spinner);
                    nativeAndPolicyProgressBar.setIndeterminateDrawable(
                            new ColorDrawable(
                                    SemanticColorUtils.getDefaultBgColor(mFragment.getContext())));
                    ProgressBar signinProgressSpinner =
                            mFragment.getView().findViewById(R.id.fre_signin_progress_spinner);
                    signinProgressSpinner.setIndeterminateDrawable(
                            new ColorDrawable(
                                    SemanticColorUtils.getDefaultBgColor(mFragment.getContext())));
                });

        ViewUtils.waitForVisibleView(allOf(withId(R.id.fre_logo), isDisplayed()));
    }

    /**
     * The dialog does not open instantly, and if we do not wait we get a small percentage of
     * flakes. See https://crbug.com/1343519.
     */
    private void clickOnUmaDialogLinkAndWait() {
        onView(withId(R.id.signin_fre_footer)).perform(clickOnUmaDialogLink());
        ViewUtils.onViewWaiting(withText(R.string.done)).check(matches(isDisplayed()));
    }

    private ViewAction clickOnUmaDialogLink() {
        return ViewUtils.clickOnClickableSpan(1);
    }

    private ViewAction clickOnTosLink() {
        return ViewUtils.clickOnClickableSpan(0);
    }

    private static <T> T waitForEvent(T mock) {
        return verify(
                mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private void clickContinueButton(String continueAsText) {
        onView(withText(continueAsText)).perform(click());
        SigninTestUtil.completeAutoDeviceLockIfNeeded(mFragment);
    }
}
