// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.hamcrest.CoreMatchers.allOf;

import android.content.Context;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.test.util.TestAccounts;

/** Tests for the {@link MismatchNotificationController} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@DoNotBatch(reason = "This test relies on native initialization")
public class MismatchNotificationControllerTest {
    private static final String TEST_URL = "https://www.google.com";

    @Rule
    public final CustomTabActivityTestRule mActivityTestRule = new CustomTabActivityTestRule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    private MismatchNotificationController mMismatchNotificationController;

    @Before
    public void setUp() {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        mActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, TEST_URL));
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController =
                                MismatchNotificationController.get(
                                        mActivityTestRule.getActivity().getWindowAndroid(),
                                        ProfileManager.getLastUsedRegularProfile(),
                                        TestAccounts.ACCOUNT1.getEmail()));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/374328422")
    public void testShowSignedOutMessage() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity()));

        // Verify that the message is displayed
        onView(withText(R.string.custom_tabs_signed_out_message_title))
                .check(matches(isDisplayed()));
        onView(withText(R.string.custom_tabs_signed_out_message_subtitle))
                .check(matches(isDisplayed()));
        onView(withText(R.string.custom_tabs_signed_out_message_button))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "TODO(crbug.com/374056362): Fix and re-enable.")
    public void testSignedOutMessagePrimaryButton() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity()));

        // Click the primary button.
        onView(withText(R.string.custom_tabs_signed_out_message_button)).perform(click());

        // Verify that the message was dismissed.
        onView(withText(R.string.custom_tabs_signed_out_message_subtitle)).check(doesNotExist());

        // Verify that the bottom sheet is displayed
        onView(
                        allOf(
                                withId(
                                        org.chromium.chrome.browser.ui.signin.R.id
                                                .account_picker_header_title),
                                withParent(
                                        withId(
                                                org.chromium.chrome.browser.ui.signin.R.id
                                                        .account_picker_state_collapsed))))
                .check(matches(isDisplayed()));
        onView(
                        allOf(
                                withId(
                                        org.chromium.chrome.browser.ui.signin.R.id
                                                .account_picker_header_subtitle),
                                withParent(
                                        withId(
                                                org.chromium.chrome.browser.ui.signin.R.id
                                                        .account_picker_state_collapsed))))
                .check(matches(isDisplayed()));
        onView(withText(TestAccounts.ACCOUNT1.getFullName())).check(matches(isDisplayed()));

        // Accept sign in
        onView(
                        allOf(
                                withId(
                                        org.chromium.chrome.browser.ui.signin.R.id
                                                .account_picker_continue_as_button),
                                withParent(
                                        withId(
                                                org.chromium.chrome.browser.ui.signin.R.id
                                                        .account_picker_state_collapsed))))
                .perform(click());

        mSigninTestRule.completeDeviceLockIfOnAutomotive();

        // Verify signed-in state.
        mSigninTestRule.waitForSignin(TestAccounts.ACCOUNT1);
    }
}
