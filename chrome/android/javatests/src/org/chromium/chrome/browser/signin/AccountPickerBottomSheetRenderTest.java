// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressBack;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.Callback;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.interstitial.IncognitoInterstitialDelegate;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.signin.ui.account_picker.AccountPickerDelegate;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/**
 * Render tests of account picker bottom sheet.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY})
@Batch(Batch.PER_CLASS)
public class AccountPickerBottomSheetRenderTest {
    private static final ProfileDataSource.ProfileData PROFILE_DATA1 =
            new ProfileDataSource.ProfileData(
                    /* accountName= */ "test.account1@gmail.com", /* avatar= */ null,
                    /* fullName= */ "Test Account1", /* givenName= */ "Account1");
    private static final ProfileDataSource.ProfileData PROFILE_DATA2 =
            new ProfileDataSource.ProfileData(
                    /* accountName= */ "test.account2@gmail.com", /* avatar= */ null,
                    /* fullName= */ null, /* givenName= */ null);

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus().setRevision(1).build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeProfileDataSource());

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Mock
    private AccountPickerDelegate mAccountPickerDelegateMock;

    @Mock
    private IncognitoInterstitialDelegate mIncognitoInterstitialDelegateMock;

    private AccountPickerBottomSheetCoordinator mCoordinator;

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
        initMocks(this);
        when(mAccountPickerDelegateMock.isIncognitoModeEnabled()).thenReturn(true);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testCollapsedSheetWithAccountView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(PROFILE_DATA1);
        buildAndShowCollapsedBottomSheet();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "collapsed_sheet_with_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testExpandedSheetView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(PROFILE_DATA1);
        mAccountManagerTestRule.addAccount(PROFILE_DATA2);
        buildAndShowCollapsedBottomSheet();
        expandBottomSheet();
        mRenderTestRule.render(mCoordinator.getBottomSheetViewForTesting(), "expanded_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testExpandedSheetShowsWhenBackpressingOnIncognitoInterstitial(
            boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(PROFILE_DATA1);
        mAccountManagerTestRule.addAccount(PROFILE_DATA2);
        buildAndShowCollapsedBottomSheet();
        expandBottomSheet();
        openIncognitoInterstitialOnExpandedSheet();
        onView(isRoot()).perform(pressBack());
        mRenderTestRule.render(mCoordinator.getBottomSheetViewForTesting(), "expanded_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSignInInProgressView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(PROFILE_DATA1);
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndCheckSigninInProgressView();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testTryAgainButtonOnSignInGeneralErrorSheet(boolean nightModeEnabled)
            throws IOException {
        CoreAccountInfo coreAccountInfo = mAccountManagerTestRule.addAccount(PROFILE_DATA1);
        // Throws a connection error during the sign-in action
        doAnswer(invocation -> {
            Callback<GoogleServiceAuthError> onSignInErrorCallback = invocation.getArgument(1);
            onSignInErrorCallback.onResult(new GoogleServiceAuthError(State.CONNECTION_FAILED));
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(coreAccountInfo), any());
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorView();
        doNothing().when(mAccountPickerDelegateMock).signIn(eq(coreAccountInfo), any());
        clickContinueButtonAndCheckSigninInProgressView();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSigninGeneralErrorView(boolean nightModeEnabled) throws IOException {
        CoreAccountInfo coreAccountInfo = mAccountManagerTestRule.addAccount(PROFILE_DATA1);
        // Throws a connection error during the sign-in action
        doAnswer(invocation -> {
            Callback<GoogleServiceAuthError> onSignInErrorCallback = invocation.getArgument(1);
            onSignInErrorCallback.onResult(new GoogleServiceAuthError(State.CONNECTION_FAILED));
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(coreAccountInfo), any());
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorView();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "signin_general_error_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSigninAuthErrorView(boolean nightModeEnabled) throws IOException {
        CoreAccountInfo coreAccountInfo = mAccountManagerTestRule.addAccount(PROFILE_DATA1);
        // Throws an authentication error during the sign-in action
        doAnswer(invocation -> {
            Callback<GoogleServiceAuthError> onSignInErrorCallback = invocation.getArgument(1);
            onSignInErrorCallback.onResult(
                    new GoogleServiceAuthError(State.INVALID_GAIA_CREDENTIALS));
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(coreAccountInfo), any());
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorView();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "signin_auth_error_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSigninAgainButtonOnSigninAuthErrorSheet(boolean nightModeEnabled)
            throws IOException {
        CoreAccountInfo coreAccountInfo = mAccountManagerTestRule.addAccount(PROFILE_DATA1);
        // Throws an auth error during the sign-in action
        doAnswer(invocation -> {
            Callback<GoogleServiceAuthError> onSignInErrorCallback = invocation.getArgument(1);
            onSignInErrorCallback.onResult(
                    new GoogleServiceAuthError(State.INVALID_GAIA_CREDENTIALS));
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(coreAccountInfo), any());

        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorView();
        doAnswer(invocation -> {
            Callback<Boolean> callback = invocation.getArgument(1);
            callback.onResult(true);
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .updateCredentials(eq(PROFILE_DATA1.getAccountEmail()), any());
        onView(withText(R.string.auth_error_card_button)).perform(click());
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "collapsed_sheet_with_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testIncognitoInterstitialView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(PROFILE_DATA1);
        mAccountManagerTestRule.addAccount(PROFILE_DATA2);
        buildAndShowCollapsedBottomSheet();
        expandBottomSheet();
        openIncognitoInterstitialOnExpandedSheet();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "signin_incognito_interstitial_view");
    }

    private void openIncognitoInterstitialOnExpandedSheet() {
        // RecyclerView: PROFILE_DATA1, PROFILE_DATA2, |Add account to device|, |Sign in
        // temporarily|
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        RecyclerView accountListView =
                bottomSheetView.findViewById(R.id.account_picker_account_list);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            accountListView.findViewHolderForAdapterPosition(3).itemView.performClick();
        });
        CriteriaHelper.pollUiThread(
                bottomSheetView.findViewById(R.id.incognito_interstitial_learn_more)::isShown);
    }

    private void clickContinueButtonAndWaitForErrorView() {
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            bottomSheetView.findViewById(R.id.account_picker_continue_as_button).performClick();
        });
        CriteriaHelper.pollUiThread(() -> {
            return !bottomSheetView.findViewById(R.id.account_picker_selected_account).isShown();
        });
    }

    private void clickContinueButtonAndCheckSigninInProgressView() throws IOException {
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            bottomSheetView.findViewById(R.id.account_picker_continue_as_button).performClick();
        });
        CriteriaHelper.pollUiThread(
                bottomSheetView.findViewById(R.id.account_picker_signin_spinner_view)::isShown);
        // Currently the ProgressBar animation cannot be disabled on android-marshmallow-arm64-rel
        // bot with DisableAnimationsTestRule, we hide the ProgressBar manually here to enable
        // checks of other elements on the screen.
        // TODO(https://crbug.com/1115067): Delete this line and use DisableAnimationsTestRule
        //  once DisableAnimationsTestRule is fixed.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            bottomSheetView.findViewById(R.id.account_picker_signin_spinner_view)
                    .setVisibility(View.INVISIBLE);
        });
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "signin_in_progress_sheet");
    }

    private void expandBottomSheet() {
        View view = mCoordinator.getBottomSheetViewForTesting();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { view.findViewById(R.id.account_picker_selected_account).performClick(); });
        CriteriaHelper.pollUiThread(view.findViewById(R.id.account_picker_account_list)::isShown);
    }

    private void buildAndShowCollapsedBottomSheet() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCoordinator = new AccountPickerBottomSheetCoordinator(mActivityTestRule.getActivity(),
                    getBottomSheetController(), mAccountPickerDelegateMock,
                    mIncognitoInterstitialDelegateMock);
        });
        CriteriaHelper.pollUiThread(mCoordinator.getBottomSheetViewForTesting().findViewById(
                R.id.account_picker_selected_account)::isShown);
    }

    private BottomSheetController getBottomSheetController() {
        return mActivityTestRule.getActivity()
                .getRootUiCoordinatorForTesting()
                .getBottomSheetController();
    }
}
