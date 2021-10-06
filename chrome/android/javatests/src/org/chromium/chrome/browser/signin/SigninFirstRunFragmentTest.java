// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.Activity;
import android.content.Intent;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.support.test.runner.lifecycle.Stage;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
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
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunPageDelegate;
import org.chromium.chrome.browser.firstrun.PolicyLoadListener;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the class {@link SigninFirstRunFragment}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninFirstRunFragmentTest {
    private static final String TEST_EMAIL1 = "test.account1@gmail.com";
    private static final String FULL_NAME1 = "Test Account1";
    private static final String GIVEN_NAME1 = "Account1";
    private static final String TEST_EMAIL2 = "test.account2@gmail.com";
    private static final String CHILD_EMAIL = "child.account@gmail.com";
    private static final String CHILD_FULL_NAME = "Test Child";

    /**
     * This class is used to test {@link SigninFirstRunFragment}.
     */
    public static class CustomSigninFirstRunFragment extends SigninFirstRunFragment {
        private FirstRunPageDelegate mFirstRunPageDelegate;
        private boolean mIsAcceptTermsOfServiceCalled;

        @Override
        public FirstRunPageDelegate getPageDelegate() {
            return mFirstRunPageDelegate;
        }

        void setPageDelegate(FirstRunPageDelegate delegate) {
            mFirstRunPageDelegate = delegate;
        }

        @Override
        public void acceptTermsOfService() {
            super.acceptTermsOfService();
            mIsAcceptTermsOfServiceCalled = true;
        }
    }

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

    @Captor
    private ArgumentCaptor<Callback<Boolean>> mCallbackCaptor;

    private CustomSigninFirstRunFragment mFragment;

    @Before
    public void setUp() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        SigninCheckerProvider.setForTests(mock(SigninChecker.class));
        when(mPolicyLoadListenerMock.get()).thenReturn(false);
        when(mFirstRunPageDelegateMock.getPolicyLoadListener()).thenReturn(mPolicyLoadListenerMock);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mFragment = new CustomSigninFirstRunFragment();
        mFragment.setPageDelegate(mFirstRunPageDelegateMock);
    }

    @Test
    @MediumTest
    public void testFragmentWhenAddingAccountDynamically() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        launchActivityWithFragment();
        Assert.assertFalse(
                mFragment.getView().findViewById(R.id.signin_fre_selected_account).isShown());
        onView(withText(R.string.signin_add_account_to_device)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(isDisplayed()));

        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);

        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
    }

    @Test
    @MediumTest
    public void testFragmentWhenAddingChildAccountDynamically() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        launchActivityWithFragment();
        onView(withText(R.string.signin_add_account_to_device)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(isDisplayed()));

        mAccountManagerTestRule.addAccount(
                CHILD_EMAIL, CHILD_FULL_NAME, /* givenName= */ null, /* avatar= */ null);

        checkFragmentWithChildAccount();
    }

    @Test
    @MediumTest
    public void testFragmentWhenRemovingChildAccountDynamically() {
        mAccountManagerTestRule.addAccount(
                CHILD_EMAIL, CHILD_FULL_NAME, /* givenName= */ null, /* avatar= */ null);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        launchActivityWithFragment();

        mAccountManagerTestRule.removeAccount(CHILD_EMAIL);

        CriteriaHelper.pollUiThread(() -> {
            return !mFragment.getView().findViewById(R.id.signin_fre_selected_account).isShown();
        });
        onView(withText(R.string.signin_add_account_to_device)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_footer)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testFragmentWithDefaultAccount() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);

        launchActivityWithFragment();

        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
        onView(withId(R.id.fre_browser_managed_by_organization)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testFragmentWhenCannotUseGooglePlayService() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });

        launchActivityWithFragment();

        CriteriaHelper.pollUiThread(() -> {
            return !mFragment.getView().findViewById(R.id.signin_fre_selected_account).isShown();
        });
        onView(withText(R.string.continue_button)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_footer)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testFragmentWhenSigninIsDisabledByPolicy() {
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(IdentityServicesProvider.get().getSigninManager(
                         Profile.getLastUsedRegularProfile()))
                    .thenReturn(mSigninManagerMock);
        });
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });

        launchActivityWithFragment();

        checkFragmentWhenSigninIsDisabledByPolicy();
    }

    @Test
    @MediumTest
    public void testFragmentWhenAddingAccountDynamicallyAndSigninIsDisabledByPolicy() {
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(IdentityServicesProvider.get().getSigninManager(
                         Profile.getLastUsedRegularProfile()))
                    .thenReturn(mSigninManagerMock);
        });
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        launchActivityWithFragment();
        checkFragmentWhenSigninIsDisabledByPolicy();

        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);

        checkFragmentWhenSigninIsDisabledByPolicy();
    }

    @Test
    @MediumTest
    public void testContinueButtonWhenCannotUseGooglePlayService() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        launchActivityWithFragment();
        CriteriaHelper.pollUiThread(() -> {
            return !mFragment.getView().findViewById(R.id.signin_fre_selected_account).isShown();
        });

        onView(withText(R.string.continue_button)).perform(click());

        CriteriaHelper.pollUiThread(() -> mFragment.mIsAcceptTermsOfServiceCalled);
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
    }

    @Test
    @MediumTest
    public void testFragmentWhenChoosingAnotherAccount() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        mAccountManagerTestRule.addAccount(
                TEST_EMAIL2, /* fullName= */ null, /* givenName= */ null, /* avatar= */ null);
        launchActivityWithFragment();
        onView(withText(TEST_EMAIL1)).perform(click());

        onView(withText(TEST_EMAIL2)).inRoot(isDialog()).perform(click());

        checkFragmentWithSelectedAccount(TEST_EMAIL2, /* fullName= */ null, /* givenName= */ null);
        onView(withId(R.id.fre_browser_managed_by_organization)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testFragmentWithDefaultAccountWhenPolicyAvailableOnDevice() {
        when(mPolicyLoadListenerMock.get()).thenReturn(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);

        launchActivityWithFragment();

        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
        onView(withId(R.id.fre_browser_managed_by_organization)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testFragmentWithChildAccount() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        mAccountManagerTestRule.addAccount(
                CHILD_EMAIL, CHILD_FULL_NAME, /* givenName= */ null, /* avatar= */ null);

        launchActivityWithFragment();

        checkFragmentWithChildAccount();
    }

    @Test
    @MediumTest
    public void testSigninWithDefaultAccount() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        launchActivityWithFragment();
        final String continueAsText = mChromeActivityTestRule.getActivity().getString(
                R.string.signin_promo_continue_as, GIVEN_NAME1);

        onView(withText(continueAsText)).perform(click());

        CriteriaHelper.pollUiThread(() -> {
            return IdentityServicesProvider.get()
                    .getIdentityManager(Profile.getLastUsedRegularProfile())
                    .hasPrimaryAccount(ConsentLevel.SIGNIN);
        });
        final CoreAccountInfo primaryAccount =
                mAccountManagerTestRule.getPrimaryAccount(ConsentLevel.SIGNIN);
        Assert.assertEquals(TEST_EMAIL1, primaryAccount.getEmail());
    }

    @Test
    @MediumTest
    public void testContinueButtonWhenUserIsSignedIn() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        final CoreAccountInfo primaryAccount = mAccountManagerTestRule.addTestAccountThenSignin();
        Assert.assertNotEquals("The primary account should be a different account!", TEST_EMAIL1,
                primaryAccount.getEmail());
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        launchActivityWithFragment();
        final String continueAsText = mChromeActivityTestRule.getActivity().getString(
                R.string.signin_promo_continue_as, GIVEN_NAME1);

        onView(withText(continueAsText)).perform(click());

        CriteriaHelper.pollUiThread(() -> mFragment.mIsAcceptTermsOfServiceCalled);
        final CoreAccountInfo currentPrimaryAccount =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    return IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
                });
        Assert.assertEquals(primaryAccount, currentPrimaryAccount);
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
    }

    @Test
    @MediumTest
    public void testDismissButtonWhenUserIsSignedIn() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        final CoreAccountInfo primaryAccount = mAccountManagerTestRule.addTestAccountThenSignin();
        Assert.assertNotEquals("The primary account should be a different account!", TEST_EMAIL1,
                primaryAccount.getEmail());
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        launchActivityWithFragment();

        onView(withText(R.string.signin_fre_dismiss_button)).perform(click());

        CriteriaHelper.pollUiThread(() -> {
            return !IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
        });
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
    }

    @Test
    @MediumTest
    public void testDismissButtonWithDefaultAccount() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        launchActivityWithFragment();

        onView(withText(R.string.signin_fre_dismiss_button)).perform(click());

        CriteriaHelper.pollUiThread(() -> mFragment.mIsAcceptTermsOfServiceCalled);
        Assert.assertNull(mAccountManagerTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
    }

    @Test
    @MediumTest
    public void testContinueButtonWithChildAccount() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        mAccountManagerTestRule.addAccount(
                CHILD_EMAIL, CHILD_FULL_NAME, /* givenName= */ null, /* avatar= */ null);
        launchActivityWithFragment();
        final String continueAsText = mChromeActivityTestRule.getActivity().getString(
                R.string.signin_promo_continue_as, CHILD_FULL_NAME);

        onView(withText(continueAsText)).perform(click());

        CriteriaHelper.pollUiThread(() -> mFragment.mIsAcceptTermsOfServiceCalled);
        Assert.assertNull(mAccountManagerTestRule.getPrimaryAccount(ConsentLevel.SIGNIN));
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(true);
    }

    @Test
    @MediumTest
    public void testFragmentWhenClickingOnFooter() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        launchActivityWithFragment();

        onView(withId(R.id.signin_fre_footer)).perform(clickOnClickableSpan());

        onView(withText(R.string.signin_fre_uma_dialog_title)).check(matches(isDisplayed()));
        onView(withId(R.id.fre_uma_dialog_switch)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_uma_dialog_first_section_header))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_uma_dialog_first_section_body))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_uma_dialog_second_section_header))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_uma_dialog_second_section_body))
                .check(matches(isDisplayed()));
        onView(withText(R.string.done)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testFragmentWhenDismissingUMADialog() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        launchActivityWithFragment();
        onView(withId(R.id.signin_fre_footer)).perform(clickOnClickableSpan());

        onView(withText(R.string.done)).perform(click());

        onView(withText(R.string.signin_fre_uma_dialog_title)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testDismissButtonWhenAllowCrashUploadTurnedOff() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        launchActivityWithFragment();
        onView(withId(R.id.signin_fre_footer)).perform(clickOnClickableSpan());
        onView(withId(R.id.fre_uma_dialog_switch)).perform(click());
        onView(withText(R.string.done)).perform(click());

        onView(withText(R.string.signin_fre_dismiss_button)).perform(click());

        CriteriaHelper.pollUiThread(() -> mFragment.mIsAcceptTermsOfServiceCalled);
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(false);
    }

    @Test
    @MediumTest
    public void testContinueButtonWhenAllowCrashUploadTurnedOff() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        launchActivityWithFragment();
        onView(withId(R.id.signin_fre_footer)).perform(clickOnClickableSpan());
        onView(withId(R.id.fre_uma_dialog_switch)).perform(click());
        onView(withText(R.string.done)).perform(click());

        final String continueAsText = mChromeActivityTestRule.getActivity().getString(
                R.string.signin_promo_continue_as, GIVEN_NAME1);
        onView(withText(continueAsText)).perform(click());

        CriteriaHelper.pollUiThread(() -> mFragment.mIsAcceptTermsOfServiceCalled);
        verify(mFirstRunPageDelegateMock).acceptTermsOfService(false);
    }

    @Test
    @MediumTest
    public void testFragmentWhenAddingAnotherAccount() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        launchActivityWithFragment();
        onView(withText(TEST_EMAIL1)).perform(click());
        onView(withText(R.string.signin_add_account_to_device)).perform(click());

        Intent data = new Intent();
        data.putExtra(AccountManager.KEY_ACCOUNT_NAME, TEST_EMAIL2);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mFragment.onActivityResult(
                                SigninFirstRunFragment.ADD_ACCOUNT_REQUEST_CODE, Activity.RESULT_OK,
                                data));

        checkFragmentWithSelectedAccount(TEST_EMAIL2, /* fullName= */ null, /* givenName= */ null);
    }

    @Test
    @MediumTest
    public void testFragmentWhenAddingDefaultAccount() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        launchActivityWithFragment();
        onView(withText(R.string.signin_add_account_to_device)).perform(click());

        Intent data = new Intent();
        data.putExtra(AccountManager.KEY_ACCOUNT_NAME, TEST_EMAIL1);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mFragment.onActivityResult(
                                SigninFirstRunFragment.ADD_ACCOUNT_REQUEST_CODE, Activity.RESULT_OK,
                                data));

        checkFragmentWithSelectedAccount(TEST_EMAIL1, /* fullName= */ null, /* givenName= */ null);
    }

    @Test
    @MediumTest
    public void testFragmentWhenPolicyIsLoadedAfterNative() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        when(mPolicyLoadListenerMock.get()).thenReturn(null);
        launchActivityWithFragment();
        checkFragmentWhenLoadingNativeAndPolicyAndHideTheSpinner();

        when(mPolicyLoadListenerMock.get()).thenReturn(false);
        verify(mPolicyLoadListenerMock).onAvailable(mCallbackCaptor.capture());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mCallbackCaptor.getValue().onResult(false); });

        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
    }

    @Test
    @MediumTest
    public void testFragmentWhenNativeIsLoadedAfterPolicy() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        launchActivityWithFragment();
        checkFragmentWhenLoadingNativeAndPolicyAndHideTheSpinner();

        TestThreadUtils.runOnUiThreadBlocking(() -> { mFragment.onNativeInitialized(); });

        checkFragmentWithSelectedAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
    }

    private void checkFragmentWithSelectedAccount(String email, String fullName, String givenName) {
        CriteriaHelper.pollUiThread(
                mFragment.getView().findViewById(R.id.signin_fre_selected_account)::isShown);
        final DisplayableProfileData profileData =
                new DisplayableProfileData(email, mock(Drawable.class), fullName, givenName);
        onView(withText(R.string.fre_welcome)).check(matches(isDisplayed()));
        onView(withText(email)).check(matches(isDisplayed()));
        if (fullName != null) {
            onView(withText(fullName)).check(matches(isDisplayed()));
        }
        onView(withId(R.id.signin_fre_selected_account_expand_icon)).check(matches(isDisplayed()));
        final String continueAsText = mFragment.getString(
                R.string.signin_promo_continue_as, profileData.getGivenNameOrFullNameOrEmail());
        onView(withText(continueAsText)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_footer)).check(matches(isDisplayed()));
    }

    private void checkFragmentWhenLoadingNativeAndPolicyAndHideTheSpinner() {
        CriteriaHelper.pollUiThread(() -> {
            return mFragment.getView().findViewById(R.id.signin_fre_progress_spinner).isShown();
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ProgressBar progressBar =
                    mFragment.getView().findViewById(R.id.signin_fre_progress_spinner);
            // Replace the progress bar with a dummy to allow other checks. Currently the
            // progress bar cannot be stopped otherwise due to some espresso issues (crbug/1115067).
            progressBar.setIndeterminateDrawable(
                    new ColorDrawable(mFragment.getResources().getColor(R.color.default_bg_color)));
        });
        onView(withText(R.string.fre_welcome)).check(matches(isDisplayed()));
        onView(withText(TEST_EMAIL1)).check(matches(not(isDisplayed())));
        onView(withText(FULL_NAME1)).check(matches(not(isDisplayed())));
        onView(withId(R.id.signin_fre_selected_account_expand_icon))
                .check(matches(not(isDisplayed())));
        final String continueAsText = mChromeActivityTestRule.getActivity().getString(
                R.string.signin_promo_continue_as, GIVEN_NAME1);
        onView(withText(continueAsText)).check(matches(not(isDisplayed())));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.signin_fre_footer)).check(matches(not(isDisplayed())));
        verify(mPolicyLoadListenerMock).onAvailable(notNull());
    }

    private void checkFragmentWithChildAccount() {
        CriteriaHelper.pollUiThread(
                mFragment.getView().findViewById(R.id.signin_fre_selected_account)::isShown);
        onView(withText(R.string.fre_welcome)).check(matches(isDisplayed()));
        Assert.assertFalse(
                mFragment.getView().findViewById(R.id.signin_fre_selected_account).isEnabled());
        onView(withText(CHILD_EMAIL)).check(matches(isDisplayed()));
        onView(withText(CHILD_FULL_NAME)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_selected_account_expand_icon))
                .check(matches(not(isDisplayed())));
        final String continueAsText =
                mFragment.getString(R.string.signin_promo_continue_as, CHILD_FULL_NAME);
        onView(withText(continueAsText)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_footer)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.fre_browser_managed_by_organization)).check(matches(not(isDisplayed())));
    }

    private void checkFragmentWhenSigninIsDisabledByPolicy() {
        CriteriaHelper.pollUiThread(() -> {
            return !mFragment.getView().findViewById(R.id.signin_fre_selected_account).isShown();
        });
        onView(withId(R.id.fre_browser_managed_by_organization)).check(matches(isDisplayed()));
        onView(withText(R.string.continue_button)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_footer)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));
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

    private ViewAction clickOnClickableSpan() {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return Matchers.instanceOf(TextView.class);
            }

            @Override
            public String getDescription() {
                return "Clicks on the one and only clickable span in the view";
            }

            @Override
            public void perform(UiController uiController, View view) {
                TextView textView = (TextView) view;
                Spanned spannedString = (Spanned) textView.getText();
                ClickableSpan[] spans =
                        spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
                if (spans.length == 0) {
                    throw new NoMatchingViewException.Builder()
                            .includeViewHierarchy(true)
                            .withRootView(textView)
                            .build();
                }
                Assert.assertEquals("There should be only one clickable link", 1, spans.length);
                spans[0].onClick(view);
            }
        };
    }
}