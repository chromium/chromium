// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressBack;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.AllOf.allOf;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;

import android.view.View;

import androidx.test.espresso.ViewInteraction;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.signin.ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/**
 * Tests account picker bottom sheet of the web signin flow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class AccountPickerBottomSheetTest {
    private static class CustomFakeAccountInfoService extends FakeAccountInfoService {
        int getNumberOfObservers() {
            return TestThreadUtils.runOnUiThreadBlockingNoException(mObservers::size);
        }
    }

    private static final String TEST_EMAIL1 = "test.account1@gmail.com";
    private static final String FULL_NAME1 = "Test Account1";
    private static final String GIVEN_NAME1 = "Account1";
    private static final String TEST_EMAIL2 = "test.account2@gmail.com";

    // Disable animations to reduce flakiness.
    @ClassRule
    public static final DisableAnimationsTestRule sNoAnimationsRule =
            new DisableAnimationsTestRule();

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private final CustomFakeAccountInfoService mFakeAccountInfoService =
            new CustomFakeAccountInfoService();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountInfoService);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private AccountPickerDelegate mAccountPickerDelegateMock;

    @Captor
    public ArgumentCaptor<Callback<String>> callbackArgumentCaptor;

    private AccountPickerBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        mAccountManagerTestRule.addAccount(TEST_EMAIL2, null, null, null);
        SigninPreferencesManager.getInstance().clearAccountPickerBottomSheetActiveDismissalCount();
    }

    @After
    public void tearDown() {
        SigninPreferencesManager.getInstance().clearAccountPickerBottomSheetActiveDismissalCount();
    }

    @Test
    @MediumTest
    public void testCollapsedSheetWithAccount() {
        HistogramDelta accountConsistencyHistogram = new HistogramDelta(
                "Signin.AccountConsistencyPromoAction", AccountConsistencyPromoAction.SHOWN);
        buildAndShowCollapsedBottomSheet();
        checkCollapsedAccountList(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
        Assert.assertEquals(1, accountConsistencyHistogram.getDelta());
    }

    @Test
    @MediumTest
    public void testExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        onVisibleView(withText(TEST_EMAIL1)).check(matches(isDisplayed()));
        onVisibleView(withText(FULL_NAME1)).check(matches(isDisplayed()));
        onView(withText(TEST_EMAIL2)).check(matches(isDisplayed()));
        onVisibleView(withText(R.string.signin_add_account_to_device))
                .check(matches(isDisplayed()));

        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testCollapsedSheetWithZeroAccount() {
        // As we have already added accounts in our current AccountManagerFacade mock
        // Here since we want to test a zero account case, we would like to set up
        // a new AccountManagerFacade mock with no account in it. The mock will be
        // torn down in the end of the test in AccountManagerTestRule.
        AccountManagerFacadeProvider.setInstanceForTests(new FakeAccountManagerFacade());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCoordinator = new AccountPickerBottomSheetCoordinator(sActivityTestRule.getActivity(),
                    getBottomSheetController(), mAccountPickerDelegateMock);
        });
        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testDismissCollapsedSheet() {
        HistogramDelta accountConsistencyHistogram =
                new HistogramDelta("Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.DISMISSED_BACK);
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.ACCOUNT_PICKER_BOTTOM_SHEET_ACTIVE_DISMISSAL_COUNT, 1);
        buildAndShowCollapsedBottomSheet();
        onView(withText(TEST_EMAIL1)).check(matches(isDisplayed()));
        BottomSheetController controller = getBottomSheetController();
        Assert.assertTrue(controller.isSheetOpen());
        Assert.assertEquals(2, mFakeAccountInfoService.getNumberOfObservers());
        onView(isRoot()).perform(pressBack());
        Assert.assertFalse(controller.isSheetOpen());
        verify(mAccountPickerDelegateMock).onDismiss();
        Assert.assertEquals(0, mFakeAccountInfoService.getNumberOfObservers());
        Assert.assertEquals(1, accountConsistencyHistogram.getDelta());
        Assert.assertEquals(2,
                SigninPreferencesManager.getInstance()
                        .getAccountPickerBottomSheetActiveDismissalCount());
    }

    @Test
    @MediumTest
    public void testDismissCollapsedSheetWithDismissButton() {
        HistogramDelta accountConsistencyHistogram =
                new HistogramDelta("Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.DISMISSED_BUTTON);
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.ACCOUNT_PICKER_BOTTOM_SHEET_ACTIVE_DISMISSAL_COUNT, 1);
        buildAndShowCollapsedBottomSheet();
        onView(withText(TEST_EMAIL1)).check(matches(isDisplayed()));
        BottomSheetController controller = getBottomSheetController();
        Assert.assertTrue(controller.isSheetOpen());
        Assert.assertEquals(2, mFakeAccountInfoService.getNumberOfObservers());
        onView(withId(R.id.account_picker_dismiss_button)).perform(click());
        Assert.assertFalse(controller.isSheetOpen());
        verify(mAccountPickerDelegateMock).onDismiss();
        Assert.assertEquals(0, mFakeAccountInfoService.getNumberOfObservers());
        Assert.assertEquals(1, accountConsistencyHistogram.getDelta());
        Assert.assertEquals(2,
                SigninPreferencesManager.getInstance()
                        .getAccountPickerBottomSheetActiveDismissalCount());
    }

    @Test
    @MediumTest
    public void testCollapsedSheetShowsWhenBackpressingOnExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        onView(isRoot()).perform(pressBack());
        checkCollapsedAccountList(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
    }

    @Test
    @MediumTest
    public void testAccountDisappearedOnCollapsedSheet() {
        buildAndShowCollapsedBottomSheet();
        mAccountManagerTestRule.removeAccount(TEST_EMAIL1);
        mAccountManagerTestRule.removeAccount(TEST_EMAIL2);
        CriteriaHelper.pollUiThread(() -> {
            return !mCoordinator.getBottomSheetViewForTesting()
                            .findViewById(R.id.account_picker_selected_account)
                            .isShown();
        });
        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testAccountDisappearedOnExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        mAccountManagerTestRule.removeAccount(TEST_EMAIL1);
        mAccountManagerTestRule.removeAccount(TEST_EMAIL2);
        CriteriaHelper.pollUiThread(() -> {
            return !mCoordinator.getBottomSheetViewForTesting()
                            .findViewById(R.id.account_picker_account_list)
                            .isShown();
        });
        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testAccountReappearedOnCollapsedSheet() {
        mAccountManagerTestRule.removeAccount(TEST_EMAIL1);
        mAccountManagerTestRule.removeAccount(TEST_EMAIL2);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCoordinator = new AccountPickerBottomSheetCoordinator(sActivityTestRule.getActivity(),
                    getBottomSheetController(), mAccountPickerDelegateMock);
        });
        checkZeroAccountBottomSheet();

        mAccountManagerTestRule.addAccount(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1, null);
        checkCollapsedAccountList(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
    }

    @Test
    @MediumTest
    public void testOtherAccountsChangeOnCollapsedSheet() {
        buildAndShowCollapsedBottomSheet();
        checkCollapsedAccountList(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
        mAccountManagerTestRule.removeAccount(TEST_EMAIL2);
        checkCollapsedAccountList(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
    }

    @Test
    @MediumTest
    public void testSelectedAccountChangeOnCollapsedSheet() {
        buildAndShowCollapsedBottomSheet();
        mAccountManagerTestRule.removeAccount(TEST_EMAIL1);
        checkCollapsedAccountList(TEST_EMAIL2, null, null);
    }

    @Test
    @MediumTest
    public void testProfileDataUpdateOnExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        String newFullName = "New Full Name1";
        String newGivenName = "New Given Name1";
        mFakeAccountInfoService.addAccountInfo(TEST_EMAIL1, newFullName, newGivenName, null);
        onVisibleView(withText(TEST_EMAIL1)).check(matches(isDisplayed()));
        onVisibleView(withText(newFullName)).check(matches(isDisplayed()));
        // Check that profile data update when the bottom sheet is expanded won't
        // toggle out any hidden part.
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testSignInDefaultAccountOnCollapsedSheet() {
        HistogramDelta accountConsistencyHistogram =
                new HistogramDelta("Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.SIGNED_IN_WITH_DEFAULT_ACCOUNT);
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndCheckSignInInProgressSheet();
        Assert.assertEquals(1, accountConsistencyHistogram.getDelta());
    }

    @Test
    @MediumTest
    public void testSignInAnotherAccount() {
        HistogramDelta accountConsistencyHistogram =
                new HistogramDelta("Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT);
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.ACCOUNT_PICKER_BOTTOM_SHEET_ACTIVE_DISMISSAL_COUNT, 2);
        buildAndShowExpandedBottomSheet();
        onView(withText(TEST_EMAIL2)).perform(click());
        CriteriaHelper.pollUiThread(mCoordinator.getBottomSheetViewForTesting().findViewById(
                R.id.account_picker_selected_account)::isShown);
        clickContinueButtonAndCheckSignInInProgressSheet();
        Assert.assertEquals(1, accountConsistencyHistogram.getDelta());
        Assert.assertEquals(0,
                SigninPreferencesManager.getInstance()
                        .getAccountPickerBottomSheetActiveDismissalCount());
    }

    @Test
    @MediumTest
    public void testSigninWithAddedAccount() {
        HistogramDelta addAccountHistogram =
                new HistogramDelta("Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.ADD_ACCOUNT_STARTED);
        HistogramDelta signedInWithAddedAccountHistogram =
                new HistogramDelta("Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.SIGNED_IN_WITH_ADDED_ACCOUNT);
        HistogramDelta signedInWithNonDefaultAccountHistogram =
                new HistogramDelta("Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT);
        buildAndShowExpandedBottomSheet();
        onVisibleView(withText(R.string.signin_add_account_to_device)).perform(click());
        verify(mAccountPickerDelegateMock).addAccount(callbackArgumentCaptor.capture());
        Callback<String> callback = callbackArgumentCaptor.getValue();
        TestThreadUtils.runOnUiThreadBlocking(() -> callback.onResult("test.account3@gmail.com"));
        clickContinueButtonAndCheckSignInInProgressSheet();
        Assert.assertEquals(1, addAccountHistogram.getDelta());
        Assert.assertEquals(1, signedInWithAddedAccountHistogram.getDelta());
        Assert.assertEquals(0, signedInWithNonDefaultAccountHistogram.getDelta());
    }

    @Test
    @MediumTest
    public void testSignInGeneralError() {
        HistogramDelta accountConsistencyHistogram =
                new HistogramDelta("Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN);
        // Throws a connection error during the sign-in action
        doAnswer(invocation -> {
            Callback<GoogleServiceAuthError> onSignInErrorCallback = invocation.getArgument(1);
            onSignInErrorCallback.onResult(new GoogleServiceAuthError(State.CONNECTION_FAILED));
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(TEST_EMAIL1), any());

        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorSheet();
        onVisibleView(withText(R.string.signin_account_picker_bottom_sheet_error_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_general_error_subtitle))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_general_error_button))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_signin_spinner_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
        Assert.assertEquals(1, accountConsistencyHistogram.getDelta());
    }

    @Test
    @MediumTest
    public void testSignInAuthError() {
        HistogramDelta accountConsistencyHistogram =
                new HistogramDelta("Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.AUTH_ERROR_SHOWN);
        // Throws an auth error during the sign-in action
        doAnswer(invocation -> {
            Callback<GoogleServiceAuthError> onSignInErrorCallback = invocation.getArgument(1);
            onSignInErrorCallback.onResult(
                    new GoogleServiceAuthError(State.INVALID_GAIA_CREDENTIALS));
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(TEST_EMAIL1), any());

        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorSheet();

        onVisibleView(withText(R.string.signin_account_picker_bottom_sheet_error_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_auth_error_subtitle))
                .check(matches(isDisplayed()));
        onView(withText(R.string.auth_error_card_button)).check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_signin_spinner_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
        Assert.assertEquals(1, accountConsistencyHistogram.getDelta());
    }

    @Test
    @MediumTest
    public void testTryAgainButtonOnSignInGeneralErrorSheet() {
        mMockitoRule.strictness(Strictness.LENIENT);
        // Throws a connection error during the sign-in action
        doAnswer(invocation -> {
            Callback<GoogleServiceAuthError> onSignInErrorCallback = invocation.getArgument(1);
            onSignInErrorCallback.onResult(new GoogleServiceAuthError(State.CONNECTION_FAILED));
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(TEST_EMAIL1), any());

        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorSheet();
        doNothing().when(mAccountPickerDelegateMock).signIn(eq(TEST_EMAIL1), any());
        // Clicking on the |Try again| button should perform the sign-in again and opens the sign-in
        // in progress page.
        clickContinueButtonAndCheckSignInInProgressSheet();
    }

    @Test
    @MediumTest
    public void testSigninAgainButtonOnSigninAuthErrorSheet() {
        // Throws an auth error during the sign-in action
        doAnswer(invocation -> {
            Callback<GoogleServiceAuthError> onSignInErrorCallback = invocation.getArgument(1);
            onSignInErrorCallback.onResult(
                    new GoogleServiceAuthError(State.INVALID_GAIA_CREDENTIALS));
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(TEST_EMAIL1), any());

        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorSheet();
        doAnswer(invocation -> {
            Callback<Boolean> callback = invocation.getArgument(1);
            callback.onResult(true);
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .updateCredentials(eq(TEST_EMAIL1), any());
        onView(withText(R.string.auth_error_card_button)).perform(click());
        checkCollapsedAccountList(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
    }

    @Test
    @MediumTest
    public void testAddAccountOnExpandedSheet() {
        HistogramDelta addAccountStartedHistogram =
                new HistogramDelta("Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.ADD_ACCOUNT_STARTED);
        HistogramDelta addAccountCompletedHistogram =
                new HistogramDelta("Signin.AccountConsistencyPromoAction",
                        AccountConsistencyPromoAction.ADD_ACCOUNT_COMPLETED);
        buildAndShowExpandedBottomSheet();
        onVisibleView(withText(R.string.signin_add_account_to_device)).perform(click());
        verify(mAccountPickerDelegateMock).addAccount(callbackArgumentCaptor.capture());
        final String accountEmailAdded = "test.account3@gmail.com";
        Callback<String> callback = callbackArgumentCaptor.getValue();
        TestThreadUtils.runOnUiThreadBlocking(() -> callback.onResult(accountEmailAdded));
        checkCollapsedAccountList(accountEmailAdded, null, null);
        Assert.assertEquals(1, addAccountStartedHistogram.getDelta());
        Assert.assertEquals(1, addAccountCompletedHistogram.getDelta());
    }

    @Test
    @MediumTest
    public void testSelectAnotherAccountOnExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        onView(withText(TEST_EMAIL2)).perform(click());
        checkCollapsedAccountList(TEST_EMAIL2, null, null);
    }

    @Test
    @MediumTest
    public void testSelectTheSameAccountOnExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        onVisibleView(withText(TEST_EMAIL1)).perform(click());
        checkCollapsedAccountList(TEST_EMAIL1, FULL_NAME1, GIVEN_NAME1);
    }

    private void clickContinueButtonAndWaitForErrorSheet() {
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            bottomSheetView.findViewById(R.id.account_picker_continue_as_button).performClick();
        });
        CriteriaHelper.pollUiThread(() -> {
            return !bottomSheetView.findViewById(R.id.account_picker_selected_account).isShown();
        });
    }

    private void clickContinueButtonAndCheckSignInInProgressSheet() {
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            bottomSheetView.findViewById(R.id.account_picker_continue_as_button).performClick();
        });
        CriteriaHelper.pollUiThread(() -> {
            return bottomSheetView.findViewById(R.id.account_picker_signin_spinner_view).isShown();
        });
        // TODO(https://crbug.com/1116348): Check AccountPickerDelegate.signIn() is called
        // after solving AsyncTask wait problem in espresso
        // Currently the ProgressBar animation cannot be disabled on android-marshmallow-arm64-rel
        // bot with DisableAnimationsTestRule, we hide the ProgressBar manually here to enable
        // checks of other elements on the screen.
        // TODO(https://crbug.com/1115067): Delete this line once DisableAnimationsTestRule is
        // fixed.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            bottomSheetView.findViewById(R.id.account_picker_signin_spinner_view)
                    .setVisibility(View.GONE);
        });
        onView(withText(R.string.signin_account_picker_bottom_sheet_signin_title))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
    }

    private void checkZeroAccountBottomSheet() {
        onVisibleView(withText(TEST_EMAIL1)).check(doesNotExist());
        onVisibleView(withText(TEST_EMAIL2)).check(doesNotExist());
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onVisibleView(withText(R.string.signin_add_account_to_device)).perform(click());
        verify(mAccountPickerDelegateMock).addAccount(notNull());
    }

    private void checkCollapsedAccountList(String email, String fullName, String givenName) {
        CriteriaHelper.pollUiThread(mCoordinator.getBottomSheetViewForTesting().findViewById(
                R.id.account_picker_selected_account)::isShown);
        onVisibleView(withText(R.string.signin_account_picker_dialog_title))
                .check(matches(isDisplayed()));
        onVisibleView(withText(R.string.signin_account_picker_bottom_sheet_subtitle))
                .check(matches(isDisplayed()));
        onVisibleView(withText(email)).check(matches(isDisplayed()));
        if (fullName != null) {
            onVisibleView(withText(fullName)).check(matches(isDisplayed()));
        }
        String continueAsText = sActivityTestRule.getActivity().getString(
                R.string.signin_promo_continue_as, givenName != null ? givenName : email);
        onView(withText(continueAsText)).check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
    }

    private void buildAndShowCollapsedBottomSheet() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCoordinator = new AccountPickerBottomSheetCoordinator(sActivityTestRule.getActivity(),
                    getBottomSheetController(), mAccountPickerDelegateMock);
        });
        CriteriaHelper.pollUiThread(mCoordinator.getBottomSheetViewForTesting().findViewById(
                R.id.account_picker_selected_account)::isShown);
    }

    private void buildAndShowExpandedBottomSheet() {
        buildAndShowCollapsedBottomSheet();
        onView(withText(FULL_NAME1)).perform(click());
    }

    private BottomSheetController getBottomSheetController() {
        return sActivityTestRule.getActivity()
                .getRootUiCoordinatorForTesting()
                .getBottomSheetController();
    }

    private static ViewInteraction onVisibleView(Matcher<View> matcher) {
        // Some view elements like PROFILE_DATA1 exist in both visible view and hidden view,
        // withEffectiveVisibility(VISIBLE) is needed here to get only the visible view of the
        // matcher.
        return onView(allOf(matcher, withEffectiveVisibility(VISIBLE)));
    }
}
