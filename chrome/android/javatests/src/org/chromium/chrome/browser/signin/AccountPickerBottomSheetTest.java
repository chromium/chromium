// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

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
import static org.mockito.MockitoAnnotations.initMocks;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.interstitial.IncognitoInterstitialDelegate;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerBottomSheetCoordinator;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/**
 * Tests account picker bottom sheet of the web signin flow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY})
@Batch(Batch.PER_CLASS)
public class AccountPickerBottomSheetTest {
    private static class CustomFakeProfileDataSource extends FakeProfileDataSource {
        int getNumberOfObservers() {
            return mObservers.size();
        }
    }

    private static final ProfileDataSource.ProfileData PROFILE_DATA1 =
            new ProfileDataSource.ProfileData(
                    /* accountName= */ "test.account1@gmail.com", /* avatar= */ null,
                    /* fullName= */ "Test Account1", /* givenName= */ "Account1");
    private static final ProfileDataSource.ProfileData PROFILE_DATA2 =
            new ProfileDataSource.ProfileData(
                    /* accountName= */ "test.account2@gmail.com", /* avatar= */ null,
                    /* fullName= */ null, /* givenName= */ null);

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

    private final CustomFakeProfileDataSource mFakeProfileDataSource =
            new CustomFakeProfileDataSource();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeProfileDataSource);

    @Mock
    private AccountPickerDelegate mAccountPickerDelegateMock;

    @Mock
    private IncognitoInterstitialDelegate mIncognitoInterstitialDelegateMock;

    @Captor
    public ArgumentCaptor<Callback<String>> callbackArgumentCaptor;

    private AccountPickerBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        initMocks(this);
        IncognitoUtils.setEnabledForTesting(true);
        mAccountManagerTestRule.addAccount(PROFILE_DATA1);
        mAccountManagerTestRule.addAccount(PROFILE_DATA2);
    }

    @Test
    @MediumTest
    public void testCollapsedSheetWithAccount() {
        buildAndShowCollapsedBottomSheet();
        checkCollapsedAccountList(PROFILE_DATA1);
    }

    @Test
    @MediumTest
    public void testExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        // Since PROFILE_DATA1 exists also in the hidden view of the selected account,
        // withEffectiveVisibility(VISIBLE) is needed here
        onView(allOf(withText(PROFILE_DATA1.getAccountName()), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        onView(allOf(withText(PROFILE_DATA1.getFullName()), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        onView(withText(PROFILE_DATA2.getAccountName())).check(matches(isDisplayed()));
        onView(withText(R.string.signin_add_account_to_device)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_incognito_button)).check(matches(isDisplayed()));

        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_continue_as_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testExpandedSheetWithIncognitoModeDisabled() {
        IncognitoUtils.setEnabledForTesting(false);
        buildAndShowExpandedBottomSheet();
        // Since PROFILE_DATA1 exists also in the hidden view of the selected account,
        // withEffectiveVisibility(VISIBLE) is needed here
        onView(allOf(withText(PROFILE_DATA1.getAccountName()), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        onView(allOf(withText(PROFILE_DATA1.getFullName()), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        onView(withText(PROFILE_DATA2.getAccountName())).check(matches(isDisplayed()));
        onView(withText(R.string.signin_add_account_to_device)).check(matches(isDisplayed()));

        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_continue_as_button)).check(matches(not(isDisplayed())));
        onView(withText(R.string.signin_incognito_button)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testCollapsedSheetWithZeroAccount() {
        // As we have already added accounts in our current AccountManagerFacade mock
        // Here since we want to test a zero account case, we would like to set up
        // a new AccountManagerFacade mock with no account in it. The mock will be
        // torn down in the end of the test in AccountManagerTestRule.
        AccountManagerFacadeProvider.setInstanceForTests(
                new FakeAccountManagerFacade(mFakeProfileDataSource));
        buildAndShowCollapsedBottomSheet();
        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testDismissCollapsedSheet() {
        buildAndShowCollapsedBottomSheet();
        onView(withText(PROFILE_DATA1.getAccountName())).check(matches(isDisplayed()));
        BottomSheetController controller = getBottomSheetController();
        Assert.assertTrue(controller.isSheetOpen());
        Assert.assertEquals(2, mFakeProfileDataSource.getNumberOfObservers());
        onView(isRoot()).perform(pressBack());
        Assert.assertFalse(controller.isSheetOpen());
        verify(mAccountPickerDelegateMock).onDismiss();
        Assert.assertEquals(0, mFakeProfileDataSource.getNumberOfObservers());
    }

    @Test
    @MediumTest
    public void testDismissCollapsedSheetWithDismissButton() {
        buildAndShowCollapsedBottomSheet();
        onView(withText(PROFILE_DATA1.getAccountName())).check(matches(isDisplayed()));
        BottomSheetController controller = getBottomSheetController();
        Assert.assertTrue(controller.isSheetOpen());
        Assert.assertEquals(2, mFakeProfileDataSource.getNumberOfObservers());
        onView(withId(R.id.account_picker_dismiss_button)).perform(click());
        Assert.assertFalse(controller.isSheetOpen());
        verify(mAccountPickerDelegateMock).onDismiss();
        Assert.assertEquals(0, mFakeProfileDataSource.getNumberOfObservers());
    }

    @Test
    @MediumTest
    public void testCollapsedSheetShowsWhenBackpressingOnExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        onView(isRoot()).perform(pressBack());
        checkCollapsedAccountList(PROFILE_DATA1);
    }

    @Test
    @MediumTest
    public void testExpandedSheetShowsWhenBackpressingOnIncognitoInterstitial() {
        buildAndShowExpandedBottomSheet();
        onView(withText(R.string.signin_incognito_button)).perform(click());
        onView(isRoot()).perform(pressBack());

        onView(withText(R.string.signin_account_picker_dialog_title)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_bottom_sheet_subtitle))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_horizontal_divider)).check(matches(isDisplayed()));
        onView(allOf(withText(PROFILE_DATA1.getAccountName()), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        onView(allOf(withText(PROFILE_DATA1.getFullName()), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        onView(withText(PROFILE_DATA2.getAccountName())).check(matches(isDisplayed()));
        onView(withText(R.string.signin_add_account_to_device)).check(matches(isDisplayed()));

        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_continue_as_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testAccountDisappearedOnCollapsedSheet() {
        buildAndShowCollapsedBottomSheet();
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(PROFILE_DATA1.getAccountName());
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(PROFILE_DATA2.getAccountName());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testAccountDisappearedOnExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(PROFILE_DATA1.getAccountName());
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(PROFILE_DATA2.getAccountName());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        checkZeroAccountBottomSheet();
    }

    @Test
    @MediumTest
    public void testAccountReappearedOnCollapsedSheet() {
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(PROFILE_DATA1.getAccountName());
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(PROFILE_DATA2.getAccountName());
        buildAndShowCollapsedBottomSheet();
        checkZeroAccountBottomSheet();

        mAccountManagerTestRule.addAccount(PROFILE_DATA1.getAccountName());
        checkCollapsedAccountList(PROFILE_DATA1);
    }

    @Test
    @MediumTest
    public void testOtherAccountsChangeOnCollapsedSheet() {
        buildAndShowCollapsedBottomSheet();
        checkCollapsedAccountList(PROFILE_DATA1);
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(PROFILE_DATA2.getAccountName());
        checkCollapsedAccountList(PROFILE_DATA1);
    }

    @Test
    @MediumTest
    public void testSelectedAccountChangeOnCollapsedSheet() {
        buildAndShowCollapsedBottomSheet();
        mAccountManagerTestRule.removeAccountAndWaitForSeeding(PROFILE_DATA1.getAccountName());
        checkCollapsedAccountList(PROFILE_DATA2);
    }

    @Test
    @MediumTest
    public void testProfileDataUpdateOnExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        String newFullName = "New Full Name1";
        String newGivenName = "New Given Name1";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mFakeProfileDataSource.setProfileData(PROFILE_DATA1.getAccountName(),
                    new ProfileDataSource.ProfileData(
                            PROFILE_DATA1.getAccountName(), null, newFullName, newGivenName));
        });
        onView(allOf(withText(newFullName), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        // Check that profile data update when the bottom sheet is expanded won't
        // toggle out any hidden part.
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_continue_as_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testSignInDefaultAccountOnCollapsedSheet() {
        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndCheckSignInInProgressSheet();
    }

    @Test
    @MediumTest
    public void testSignInAnotherAccount() {
        buildAndShowExpandedBottomSheet();
        onView(withText(PROFILE_DATA2.getAccountName())).perform(click());
        CriteriaHelper.pollUiThread(mCoordinator.getBottomSheetViewForTesting().findViewById(
                R.id.account_picker_continue_as_button)::isShown);
        clickContinueButtonAndCheckSignInInProgressSheet();
    }

    @Test
    @MediumTest
    public void testSignInGeneralError() {
        // Throws a connection error during the sign-in action
        doAnswer(invocation -> {
            Callback<GoogleServiceAuthError> onSignInErrorCallback = invocation.getArgument(1);
            onSignInErrorCallback.onResult(new GoogleServiceAuthError(State.CONNECTION_FAILED));
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(mAccountManagerTestRule.toCoreAccountInfo(
                                PROFILE_DATA1.getAccountName())),
                        any());

        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorSheet();
        onView(withText(R.string.signin_account_picker_bottom_sheet_error_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_general_error_subtitle))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_general_error_button))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_horizontal_divider)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_signin_spinner_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testSignInAuthError() {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.toCoreAccountInfo(PROFILE_DATA1.getAccountName());
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
        clickContinueButtonAndWaitForErrorSheet();
        onView(withText(R.string.signin_account_picker_bottom_sheet_error_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_auth_error_subtitle))
                .check(matches(isDisplayed()));
        onView(withText(R.string.auth_error_card_button)).check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_horizontal_divider)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_signin_spinner_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testTryAgainButtonOnSignInGeneralErrorSheet() {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.toCoreAccountInfo(PROFILE_DATA1.getAccountName());
        // Throws a connection error during the sign-in action
        doAnswer(invocation -> {
            Callback<GoogleServiceAuthError> onSignInErrorCallback = invocation.getArgument(1);
            onSignInErrorCallback.onResult(new GoogleServiceAuthError(State.CONNECTION_FAILED));
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .signIn(eq(coreAccountInfo), any());

        buildAndShowCollapsedBottomSheet();
        clickContinueButtonAndWaitForErrorSheet();
        doNothing().when(mAccountPickerDelegateMock).signIn(eq(coreAccountInfo), any());
        // Clicking on the |Try again| button should perform the sign-in again and opens the sign-in
        // in progress page.
        clickContinueButtonAndCheckSignInInProgressSheet();
    }

    @Test
    @MediumTest
    public void testSigninAgainButtonOnSigninAuthErrorSheet() {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.toCoreAccountInfo(PROFILE_DATA1.getAccountName());
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
        clickContinueButtonAndWaitForErrorSheet();
        doAnswer(invocation -> {
            Callback<Boolean> callback = invocation.getArgument(1);
            callback.onResult(true);
            return null;
        })
                .when(mAccountPickerDelegateMock)
                .updateCredentials(eq(PROFILE_DATA1.getAccountName()), any());
        onView(withText(R.string.auth_error_card_button)).perform(click());
        checkCollapsedAccountList(PROFILE_DATA1);
    }

    @Test
    @MediumTest
    public void testAddAccountOnExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        onView(withText(R.string.signin_add_account_to_device)).perform(click());
        verify(mAccountPickerDelegateMock).addAccount(callbackArgumentCaptor.capture());
        ProfileDataSource.ProfileData profileDataAdded = new ProfileDataSource.ProfileData(
                /* accountName= */ "test.account3@gmail.com", /* avatar= */ null,
                /* fullName= */ null, /* givenName= */ null);
        Callback<String> callback = callbackArgumentCaptor.getValue();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> callback.onResult(profileDataAdded.getAccountName()));
        checkCollapsedAccountList(profileDataAdded);
    }

    @Test
    @MediumTest
    public void testSelectAnotherAccountOnExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        onView(withText(PROFILE_DATA2.getAccountName())).perform(click());
        checkCollapsedAccountList(PROFILE_DATA2);
    }

    @Test
    @MediumTest
    public void testSelectTheSameAccountOnExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        onView(allOf(withText(PROFILE_DATA1.getAccountName()), withEffectiveVisibility(VISIBLE)))
                .perform(click());
        checkCollapsedAccountList(PROFILE_DATA1);
    }

    @Test
    @MediumTest
    public void testIncognitoOptionShownOnExpandedSheet() {
        buildAndShowExpandedBottomSheet();
        onView(withText(R.string.signin_incognito_button)).perform(click());
        checkIncognitoInterstitialSheet();
    }

    @Test
    @MediumTest
    public void testLearnMoreButtonOnIncognitoInterstitial() {
        buildAndShowExpandedBottomSheet();
        onView(withText(R.string.signin_incognito_button)).perform(click());
        onView(withId(R.id.incognito_interstitial_learn_more)).perform(click());
        verify(mIncognitoInterstitialDelegateMock).openLearnMorePage();
    }

    @Test
    @MediumTest
    public void testContinueButtonOnIncognitoInterstitial() {
        buildAndShowExpandedBottomSheet();
        onView(withText(R.string.signin_incognito_button)).perform(click());
        onView(withId(R.id.incognito_interstitial_continue_button)).perform(click());
        verify(mIncognitoInterstitialDelegateMock).openCurrentUrlInIncognitoTab();
    }

    private void checkIncognitoInterstitialSheet() {
        onView(withId(R.id.account_picker_bottom_sheet_logo)).check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_bottom_sheet_title))
                .check(matches(isDisplayed()))
                .check(matches(withText(R.string.incognito_interstitial_title)));
        onView(withId(R.id.incognito_interstitial_bottom_sheet_view)).check(matches(isDisplayed()));
        onView(withId(R.id.incognito_interstitial_message)).check(matches(isDisplayed()));
        onView(withId(R.id.incognito_interstitial_learn_more)).check(matches(isDisplayed()));
        onView(withId(R.id.incognito_interstitial_continue_button)).check(matches(isDisplayed()));

        onView(withId(R.id.account_picker_bottom_sheet_subtitle))
                .check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_horizontal_divider)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_continue_as_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
    }

    private void clickContinueButtonAndWaitForErrorSheet() {
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            bottomSheetView.findViewById(R.id.account_picker_continue_as_button).performClick();
        });
        CriteriaHelper.pollUiThread(() -> {
            return !bottomSheetView.findViewById(R.id.account_picker_selected_account).isShown()
                    && bottomSheetView.findViewById(R.id.account_picker_bottom_sheet_subtitle)
                               .isShown();
        });
    }

    private void clickContinueButtonAndCheckSignInInProgressSheet() {
        View bottomSheetView = mCoordinator.getBottomSheetViewForTesting();
        ThreadUtils.runOnUiThread(
                bottomSheetView.findViewById(R.id.account_picker_continue_as_button)::performClick);
        CriteriaHelper.pollUiThread(() -> {
            return !bottomSheetView.findViewById(R.id.account_picker_continue_as_button).isShown();
        });
        // TODO(https://crbug.com/1116348): Check AccountPickerDelegate.signIn() is called
        // after solving AsyncTask wait problem in espresso
        Assert.assertTrue(
                bottomSheetView.findViewById(R.id.account_picker_signin_spinner_view).isShown());
        // Currently the ProgressBar animation cannot be disabled on android-marshmallow-arm64-rel
        // bot with DisableAnimationsTestRule, we hide the ProgressBar manually here to enable
        // checks of other elements on the screen.
        // TODO(https://crbug.com/1115067): Delete this line once DisableAnimationsTestRule is
        // fixed.
        ThreadUtils.runOnUiThread(() -> {
            bottomSheetView.findViewById(R.id.account_picker_signin_spinner_view)
                    .setVisibility(View.GONE);
        });
        onView(withText(R.string.signin_account_picker_bottom_sheet_signin_title))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_bottom_sheet_subtitle))
                .check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_horizontal_divider)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_continue_as_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(not(isDisplayed())));
    }

    private void checkZeroAccountBottomSheet() {
        onView(allOf(withText(PROFILE_DATA1.getAccountName()), withEffectiveVisibility(VISIBLE)))
                .check(doesNotExist());
        onView(allOf(withText(PROFILE_DATA2.getAccountName()), withEffectiveVisibility(VISIBLE)))
                .check(doesNotExist());
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
        onView(withId(R.id.account_picker_selected_account)).check(matches(not(isDisplayed())));
        onView(allOf(withText(R.string.signin_add_account_to_device),
                       withEffectiveVisibility(VISIBLE)))
                .perform(click());
        verify(mAccountPickerDelegateMock).addAccount(notNull());
    }

    private void checkCollapsedAccountList(ProfileDataSource.ProfileData profileData) {
        CriteriaHelper.pollUiThread(mCoordinator.getBottomSheetViewForTesting().findViewById(
                R.id.account_picker_selected_account)::isShown);
        onView(withText(R.string.signin_account_picker_dialog_title)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_account_picker_bottom_sheet_subtitle))
                .check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_horizontal_divider)).check(matches(isDisplayed()));
        onView(allOf(withText(profileData.getAccountName()), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        if (profileData.getFullName() != null) {
            onView(allOf(withText(profileData.getFullName()), withEffectiveVisibility(VISIBLE)))
                    .check(matches(isDisplayed()));
        }
        onView(allOf(withId(R.id.account_selection_mark), withEffectiveVisibility(VISIBLE)))
                .check(matches(isDisplayed()));
        String continueAsText =
                sActivityTestRule.getActivity().getString(R.string.signin_promo_continue_as,
                        profileData.getGivenName() != null ? profileData.getGivenName()
                                                           : profileData.getAccountName());
        onView(withText(continueAsText)).check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_dismiss_button)).check(matches(isDisplayed()));
        onView(withId(R.id.account_picker_account_list)).check(matches(not(isDisplayed())));
    }

    private void buildAndShowCollapsedBottomSheet() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCoordinator = new AccountPickerBottomSheetCoordinator(sActivityTestRule.getActivity(),
                    getBottomSheetController(), mAccountPickerDelegateMock,
                    mIncognitoInterstitialDelegateMock);
        });
        CriteriaHelper.pollUiThread(mCoordinator.getBottomSheetViewForTesting().findViewById(
                R.id.account_picker_continue_as_button)::isShown);
    }

    private void buildAndShowExpandedBottomSheet() {
        buildAndShowCollapsedBottomSheet();
        onView(withText(PROFILE_DATA1.getFullName())).perform(click());
    }

    private BottomSheetController getBottomSheetController() {
        return sActivityTestRule.getActivity()
                .getRootUiCoordinatorForTesting()
                .getBottomSheetController();
    }
}
