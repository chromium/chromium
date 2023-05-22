// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import android.app.Activity;
import android.content.Context;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.UiRestriction;

import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Integration test for {@link ChromeSurveyController}. */
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures(ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.CHROME_FORCE_ENABLE_SURVEY,
    ChromeSurveyController.COMMAND_LINE_PARAM_NAME + "=" +
        ChromeSurveyControllerIntegrationTest.TEST_TRIGGER_ID})
// Batched PER_CLASS because survey initialization is triggered at ChromeTabbedActivity creation.
// Can be updated to UNIT_TESTS when the dependency on CTA creation is removed.
@Batch(Batch.PER_CLASS)
public class ChromeSurveyControllerIntegrationTest {
    // clang-format on
    static final String TEST_TRIGGER_ID = "test_trigger_id";
    static final long MESSAGE_AUTO_DISMISS_DURATION_MS = 10000L;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private SharedPreferencesManager mSharedPreferenceManager =
            SharedPreferencesManager.getInstance();
    private AlwaysSuccessfulSurveyController mTestSurveyController;
    private CallbackHelper mAllAnimationsFinishedCallback;
    private String mPrefKey;
    private MessageDispatcher mMessageDispatcher;

    @Before
    public void setUp() throws InterruptedException, TimeoutException, ExecutionException {
        mAllAnimationsFinishedCallback = new CallbackHelper();

        ChromeSurveyController.forceIsUMAEnabledForTesting(true);
        mPrefKey = ChromePreferenceKeys.CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP.createKey(
                TEST_TRIGGER_ID);
        mSharedPreferenceManager = SharedPreferencesManager.getInstance();

        mTestSurveyController = new AlwaysSuccessfulSurveyController();
        SurveyController.setInstanceForTesting(mTestSurveyController);

        mActivityTestRule.startMainActivityOnBlankPage();

        mMessageDispatcher = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> MessageDispatcherProvider.from(
                                mActivityTestRule.getActivity().getWindowAndroid()));

        waitUntilSurveyPromptPresented();
    }

    @After
    public void tearDown() {
        SurveyController.setInstanceForTesting(null);
        mSharedPreferenceManager.removeKey(mPrefKey);
        ChromeSurveyController.forceIsUMAEnabledForTesting(false);
        ChromeSurveyController.resetMessageShownForTesting();
    }

    @DisabledTest(message = "https:://crbug.com/1447519")
    @Test
    @MediumTest
    public void testMessagePrimaryButtonClicked() throws TimeoutException, ExecutionException {
        PropertyModel message = getSurveyMessage();
        Assert.assertNotNull("Message should not be null.", message);

        // Simulate the message primary button click.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { message.get(MessageBannerProperties.ON_PRIMARY_ACTION).get(); });
        // Simulate message dismissal on primary button click.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mMessageDispatcher.dismissMessage(message, DismissReason.PRIMARY_ACTION));

        Assert.assertEquals("#showSurveyIfAvailable should be attempted.", 1,
                mTestSurveyController.showSurveyCallbackHelper.getCallCount());
    }

    @DisabledTest(message = "https:://crbug.com/1447519")
    @Test
    @MediumTest
    public void testMessageDismissed() throws TimeoutException, ExecutionException {
        PropertyModel message = getSurveyMessage();
        Assert.assertNotNull("Message should not be null.", message);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mMessageDispatcher.dismissMessage(message, DismissReason.GESTURE));
    }

    @DisabledTest(message = "https:://crbug.com/1447519")
    @Test
    @MediumTest
    public void testNoMessageInNewTab() throws InterruptedException, ExecutionException {
        // Simulate message visibility for the auto-dismiss duration length of time.
        waitUntilSurveyPromptStateRecorded(MESSAGE_AUTO_DISMISS_DURATION_MS);

        // Survey prompt should not be displayed in another tab.
        Tab tabTwo = mActivityTestRule.loadUrlInNewTab("about:blank", false);

        waitUntilTabIsReady(tabTwo);
        Assert.assertNull("Tab two should not have the message.", getSurveyMessage());
    }

    private void waitUntilSurveyPromptPresented() throws TimeoutException {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        waitUntilTabIsReady(tab);
        mTestSurveyController.downloadCallbackHelper.waitForFirst();
        Assert.assertNotNull("Tab should have a message.", getSurveyMessage());
    }

    private void waitUntilSurveyPromptStateRecorded(long visibilityDuration)
            throws InterruptedException {
        Thread.sleep(visibilityDuration);
        CriteriaHelper.pollUiThread(
                () -> SharedPreferencesManager.getInstance().contains(mPrefKey));
    }

    private void waitUntilTabIsReady(Tab tab) {
        CriteriaHelper.pollUiThread(() -> !tab.isLoading() && tab.isUserInteractable());
    }

    private PropertyModel getSurveyMessage() {
        List<MessageStateHandler> messages = MessagesTestHelper.getEnqueuedMessages(
                mMessageDispatcher, MessageIdentifier.CHROME_SURVEY);
        return messages.size() == 0 ? null : MessagesTestHelper.getCurrentMessage(messages.get(0));
    }

    private static class AlwaysSuccessfulSurveyController extends SurveyController {
        public final CallbackHelper downloadCallbackHelper = new CallbackHelper();
        public final CallbackHelper showSurveyCallbackHelper = new CallbackHelper();

        @Override
        public void downloadSurvey(Context context, String triggerId, Runnable onSuccessRunnable,
                Runnable onFailureRunnable) {
            downloadCallbackHelper.notifyCalled();

            Assert.assertEquals(TEST_TRIGGER_ID, triggerId);
            onSuccessRunnable.run();
        }

        @Override
        public void showSurveyIfAvailable(Activity activity, String siteId,
                boolean showAsBottomSheet, int displayLogoResId,
                ActivityLifecycleDispatcher lifecycleDispatcher) {
            showSurveyCallbackHelper.notifyCalled();
        }
    }
}
