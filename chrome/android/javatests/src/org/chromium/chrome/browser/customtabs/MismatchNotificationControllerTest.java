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
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import androidx.test.espresso.action.ViewActions;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.CloseType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

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

    private int mCloseType;

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
    public void testShowSignedOutMessage() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity(), this::onClose));

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
    public void testSignedOutMessagePrimaryButton() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity(), this::onClose));

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
        assertEquals(
                "Accept sign-in should result in ACCEPTED",
                CloseType.ACCEPTED.getNumber(),
                mCloseType);
    }

    @Test
    @MediumTest
    public void testUserSignsInWhileMessageDisplayed() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity(), this::onClose));

        // Verify that the message is displayed.
        onView(withText(R.string.custom_tabs_signed_out_message_subtitle))
                .check(matches(isDisplayed()));

        // Sign in.
        SigninTestUtil.signin(TestAccounts.ACCOUNT1);

        // Verify that the message was dismissed.
        onView(withText(R.string.custom_tabs_signed_out_message_subtitle)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testAccountRemovedWhileMessageIsDisplayed() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity(), this::onClose));

        // Verify that the message is displayed.
        onView(withText(R.string.custom_tabs_signed_out_message_subtitle))
                .check(matches(isDisplayed()));

        // Remove the account.
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        // Verify that the message was dismissed.
        onView(withText(R.string.custom_tabs_signed_out_message_subtitle)).check(doesNotExist());

        assertEquals(
                "Account switch should result in TIMED_OUT",
                CloseType.TIMED_OUT.getNumber(),
                mCloseType);
    }

    @Test
    @MediumTest
    public void testDismissalByGesture() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity(), this::onClose));
        int titleRes = R.string.custom_tabs_signed_out_message_subtitle;

        // Verify that the message is displayed.
        onView(withText(titleRes)).check(matches(isDisplayed()));

        // Swipe away the UI
        onView(withText(titleRes)).perform(ViewActions.swipeRight());

        // Verify that the message was dismissed.
        onView(withText(titleRes)).check(doesNotExist());

        assertEquals(
                "Swiping away should result in DISMISSED",
                CloseType.DISMISSED.getNumber(),
                mCloseType);
    }

    @Test
    @MediumTest
    public void testDismissalByClosingCct() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity(), this::onClose));
        int titleRes = R.string.custom_tabs_signed_out_message_subtitle;

        // Verify that the message is displayed.
        onView(withText(titleRes)).check(matches(isDisplayed()));

        // Tab close button
        onView(withId(R.id.close_button)).perform(click());

        assertEquals(
                "Closing CCT should result in TIMED_OUT",
                CloseType.TIMED_OUT.getNumber(),
                mCloseType);
    }

    @Test
    @MediumTest
    public void testDismissalByClosingCct_whileStackedBehind() {
        // Display a message UI of high priority that won't let the sign-in message be visible.
        ThreadUtils.runOnUiThreadBlocking(this::showHighPriorityMessage);

        int titleRes = R.string.account_selection_content_description;
        onView(withText(titleRes)).check(matches(isDisplayed()));

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity(), this::onClose));

        // Verify that the message is displayed but actually doesn't get visible.
        titleRes = R.string.custom_tabs_signed_out_message_subtitle;
        onView(withText(titleRes)).check(matches(isDisplayed()));
        assertFalse(
                "Message should not be visible as it gets queued behind",
                mMismatchNotificationController.wasShownForTesting());

        // Tap close button
        onView(withId(R.id.close_button)).perform(click());

        assertEquals(
                "Message that was never visible should result in UNKNOWN",
                CloseType.UNKNOWN.getNumber(),
                mCloseType);
    }

    @Test
    @MediumTest
    public void testDismissalByClosingCct_whileStackedBehind2() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity(), this::onClose));

        // Verify that the message is displayed.
        int titleRes = R.string.custom_tabs_signed_out_message_subtitle;
        onView(withText(titleRes)).check(matches(isDisplayed()));

        // Display a message UI of high priority in succession, hiding the sign-in message.
        ThreadUtils.runOnUiThreadBlocking(this::showHighPriorityMessage);

        titleRes = R.string.account_selection_content_description;
        onView(withText(titleRes)).check(matches(isDisplayed()));

        // The sign-in message was visible momentarily but soon got hidden.
        // Should this be regarded as invisible, and result in UNKNOWN instead of TIMED_OUT?
        assertTrue("Message was visible.", mMismatchNotificationController.wasShownForTesting());

        // Tap close button
        onView(withId(R.id.close_button)).perform(click());

        assertEquals(
                "Message that was visible should result in TIMED_OUT",
                CloseType.TIMED_OUT.getNumber(),
                mCloseType);
    }

    private void onClose(int closeType) {
        mCloseType = closeType;
    }

    private void showHighPriorityMessage() {
        WindowAndroid window = mMismatchNotificationController.getWindowForTesting();
        Context context = window.getActivity().get();
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                context.getString(R.string.custom_tabs_signed_out_message_button))
                        .with(
                                MessageBannerProperties.TITLE,
                                context.getString(R.string.custom_tabs_signed_out_message_title))
                        .with(
                                MessageBannerProperties.DESCRIPTION,
                                context.getString(R.string.account_selection_content_description))
                        .with(MessageBannerProperties.ICON_RESOURCE_ID, R.drawable.fre_product_logo)
                        .with(
                                MessageBannerProperties.ICON_TINT_COLOR,
                                MessageBannerProperties.TINT_NONE)
                        .build();

        MessageDispatcher dispatcher = MessageDispatcherProvider.from(window);
        dispatcher.enqueueWindowScopedMessage(model, /* highPriority= */ true);
    }
}
