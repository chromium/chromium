// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.AllOf.allOf;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

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
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;

/**
 * Render tests of account picker bottom sheet.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.DEPRECATE_MENAGERIE_API})
@Batch(Batch.PER_CLASS)
public class AccountPickerBottomSheetRenderTest {
    private static final String TEST_EMAIL1 = "test.account1@gmail.com";
    private static final String FULL_NAME1 = "Test Account1";
    private static final String GIVEN_NAME1 = "Account1";
    private static final String TEST_EMAIL2 = "test.account2@gmail.com";

    private static final class CustomAccountPickerDelegate implements AccountPickerDelegate {
        private @Nullable GoogleServiceAuthError mError;

        CustomAccountPickerDelegate() {
            mError = null;
        }

        void setError(@State int state) {
            mError = new GoogleServiceAuthError(state);
        }

        void clearError() {
            mError = null;
        }

        @Override
        public void onDismiss() {}

        @Override
        public void signIn(
                String accountEmail, Callback<GoogleServiceAuthError> onSignInErrorCallback) {
            if (mError != null) {
                onSignInErrorCallback.onResult(mError);
            }
        }

        @Override
        public void addAccount(Callback<String> callback) {}

        @Override
        public void updateCredentials(
                String accountName, Callback<Boolean> onUpdateCredentialsCallback) {
            onUpdateCredentialsCallback.onResult(true);
        }
    }

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus().setRevision(3).build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeAccountInfoService());

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private final CustomAccountPickerDelegate mAccountPickerDelegate =
            new CustomAccountPickerDelegate();

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
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        buildAndShowCollapsedBottomSheet();
        onView(isRoot()).check(ViewUtils.waitForView(allOf(withText(TEST_EMAIL1), isDisplayed())));
        onView(isRoot()).check(ViewUtils.waitForView(allOf(withText(FULL_NAME1), isDisplayed())));
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "collapsed_sheet_with_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testExpandedSheetView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        mAccountManagerTestRule.addAccount(TEST_EMAIL2, null, null, null);
        buildAndShowCollapsedBottomSheet();
        expandBottomSheet();
        mRenderTestRule.render(mCoordinator.getBottomSheetViewForTesting(), "expanded_sheet");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSignInInProgressView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndCheckSigninInProgressView();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testTryAgainButtonOnSignInGeneralErrorSheet(boolean nightModeEnabled)
            throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        mAccountPickerDelegate.setError(State.CONNECTION_FAILED);
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorView();
        // Clear the error so that the sign-in could continue normally.
        mAccountPickerDelegate.clearError();
        clickContinueButtonAndCheckSigninInProgressView();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testSigninGeneralErrorView(boolean nightModeEnabled) throws IOException {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        mAccountPickerDelegate.setError(State.CONNECTION_FAILED);
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
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        mAccountPickerDelegate.setError(State.INVALID_GAIA_CREDENTIALS);
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorView();
        mRenderTestRule.render(
                mCoordinator.getBottomSheetViewForTesting(), "signin_auth_error_sheet");
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
                    getBottomSheetController(), mAccountPickerDelegate);
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
