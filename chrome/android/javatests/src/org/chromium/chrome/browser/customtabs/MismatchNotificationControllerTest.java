// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;
import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.RootSpec.anyRoot;
import static org.chromium.base.test.transit.ViewElement.rootSpecOption;
import static org.chromium.base.test.transit.ViewFinder.waitForNoView;
import static org.chromium.base.test.transit.ViewFinder.waitForView;

import android.content.Context;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.features.branding.MismatchNotificationChecker;
import org.chromium.chrome.browser.customtabs.features.branding.proto.AccountMismatchData.CloseType;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for the {@link MismatchNotificationController} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
// TODO(http://crbug.com/495529795): Enable side panel and fix this test.
@DisableFeatures({ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL})
@Batch(Batch.PER_CLASS)
@EnableFeatures(SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT)
public class MismatchNotificationControllerTest {
    private static final String TEST_URL = "https://www.google.com";

    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = SigninTestRule.createWithCleanups();

    @Rule(order = 1)
    public final CustomTabActivityTestRule mActivityTestRule = new CustomTabActivityTestRule();

    private MismatchNotificationController mMismatchNotificationController;
    private MismatchNotificationChecker mMismatchNotificationChecker;

    private int mCloseType;

    private final CallbackHelper mCloseCallbackHelper = new CallbackHelper();

    @Before
    public void setUp() {
        Context context = getInstrumentation().getTargetContext().getApplicationContext();
        mActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, TEST_URL));
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    CustomTabActivity activity = mActivityTestRule.getActivity();
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    mMismatchNotificationChecker =
                            new MismatchNotificationChecker(
                                    activity,
                                    activity.getWindowAndroid(),
                                    activity.getActivityResultTracker(),
                                    DeviceLockActivityLauncherImpl.get(),
                                    profile,
                                    IdentityServicesProvider.get().getIdentityManager(profile),
                                    SigninAndHistorySyncActivityLauncherImpl.get(),
                                    () ->
                                            activity.getRootUiCoordinatorForTesting()
                                                    .getBottomSheetController(),
                                    activity.getModalDialogManager(),
                                    activity.getSnackbarManager(),
                                    (delegate, accountId, lastShownTime, mimData, onClose) ->
                                            false);
                    mMismatchNotificationController =
                            MismatchNotificationController.get(
                                    activity.getWindowAndroid(),
                                    profile,
                                    TestAccounts.ACCOUNT1.getEmail(),
                                    mMismatchNotificationChecker);
                });
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
    @DisableFeatures(SigninFeatures.ENABLE_ACTIVITYLESS_SIGNIN_ALL_ENTRY_POINT)
    public void testSignedOutMessagePrimaryButton_legacy() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity(), this::onClose));
        startAndVerifySigninFlow();
    }

    @Test
    @MediumTest
    public void testSignedOutMessagePrimaryButton() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity(), this::onClose));
        startAndVerifySigninFlow();
    }

    private void startAndVerifySigninFlow() {
        // Click the primary button to initiate the sign-in flow.
        waitForView(withText(R.string.custom_tabs_signed_out_message_button)).click();

        // Verify that the message was dismissed.
        waitForNoView(withText(R.string.custom_tabs_signed_out_message_subtitle));

        // Verify that the bottom sheet is displayed. Use RootSpec.anyRoot() because
        // launching SigninAndHistorySyncActivity over CustomTabActivity in Android Desktop
        // freeform multi-window can leave the underlying or transitioning window unfocused,
        // causing Espresso's RootViewPicker to time out with RootViewWithoutFocusException.
        waitForView(
                allOf(
                        withId(R.id.account_picker_header_title),
                        withParent(withId(R.id.account_picker_state_collapsed))),
                rootSpecOption(anyRoot()));
        waitForView(
                allOf(
                        withId(R.id.account_picker_header_subtitle),
                        withParent(withId(R.id.account_picker_state_collapsed))),
                rootSpecOption(anyRoot()));
        waitForView(withText(TestAccounts.ACCOUNT1.getFullName()), rootSpecOption(anyRoot()));

        // Accept sign in
        waitForView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed))),
                        rootSpecOption(anyRoot()))
                .click();

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
        waitForNoView(withText(R.string.custom_tabs_signed_out_message_subtitle));
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
        waitForNoView(withText(R.string.custom_tabs_signed_out_message_subtitle));

        assertEquals(
                "Account switch should result in TIMED_OUT",
                CloseType.TIMED_OUT.getNumber(),
                mCloseType);
    }

    @Test
    @MediumTest
    public void testDismissalByGesture() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity(), this::onClose));
        int titleRes = R.string.custom_tabs_signed_out_message_subtitle;

        // Verify that the message is displayed.
        onView(withText(titleRes)).check(matches(isDisplayed()));

        // Dismiss the message as if it were swiped away. A real Espresso swipe is flaky here: the
        // message banner ignores swipes until its enter animation has finished, which is not
        // observable by Espresso's idle sync (see also b/329707221). The gesture handling itself
        // is covered by the messages component's own tests.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    MessageDispatcher dispatcher =
                            MessageDispatcherProvider.from(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                    List<MessageStateHandler> messages =
                            MessagesTestHelper.getEnqueuedMessages(
                                    dispatcher, MessageIdentifier.CCT_ACCOUNT_MISMATCH_NOTICE);
                    assertEquals(1, messages.size());
                    dispatcher.dismissMessage(
                            MessagesTestHelper.getCurrentMessage(messages.get(0)),
                            DismissReason.GESTURE);
                });
        mCloseCallbackHelper.waitForOnly();

        // Verify that the message was dismissed. The view is removed from the hierarchy once the
        // exit animation ends, which happens asynchronously even with animations disabled.
        ViewGroup messageContainer =
                mActivityTestRule.getActivity().findViewById(R.id.message_container);
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(messageContainer.getChildCount(), is(0)));
        waitForNoView(withText(titleRes));

        assertEquals(
                "Swiping away should result in DISMISSED",
                CloseType.DISMISSED.getNumber(),
                mCloseType);
    }

    @Test
    @MediumTest
    public void testDismissalByClosingCct() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMismatchNotificationController.showSignedOutMessage(
                                mActivityTestRule.getActivity(), this::onClose));
        int titleRes = R.string.custom_tabs_signed_out_message_subtitle;

        // Verify that the message is displayed.
        onView(withText(titleRes)).check(matches(isDisplayed()));

        // Tab close button, and wait till the callback updates the result.
        onView(withId(R.id.close_button)).perform(click());
        mCloseCallbackHelper.waitForOnly();

        assertEquals(
                "Closing CCT should result in TIMED_OUT",
                CloseType.TIMED_OUT.getNumber(),
                mCloseType);
    }

    @Test
    @MediumTest
    public void testDismissalByClosingCct_whileStackedBehind() throws TimeoutException {
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
        mCloseCallbackHelper.waitForOnly();

        assertEquals(
                "Message that was never visible should result in UNKNOWN",
                CloseType.UNKNOWN.getNumber(),
                mCloseType);
    }

    @Test
    @MediumTest
    public void testDismissalByClosingCct_whileStackedBehind2() throws TimeoutException {
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
        mCloseCallbackHelper.waitForOnly();

        assertEquals(
                "Message that was visible should result in TIMED_OUT",
                CloseType.TIMED_OUT.getNumber(),
                mCloseType);
    }

    private void onClose(int closeType) {
        mCloseType = closeType;
        mCloseCallbackHelper.notifyCalled();
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
