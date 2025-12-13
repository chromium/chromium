// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.view.View;

import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorUtil;
import org.chromium.chrome.browser.ui.hats.TestSurveyUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.OverrideContextWrapperTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.List;

/** Integration test for {@link PrivacySandboxSurveyController} using {@link SurveyClient}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
// TODO(crbug.com/391968140): Re-enable tests when supporting tablets
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public class PrivacySandboxSurveyControllerIntegrationTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public OverrideContextWrapperTestRule mAutomotiveRule = new OverrideContextWrapperTestRule();

    @Rule
    public TestSurveyUtils.TestSurveyComponentRule mTestSurveyComponentRule =
            new TestSurveyUtils.TestSurveyComponentRule();

    private MessageDispatcher mMessageDispatcher;
    private PropertyModel mSurveyMessage;
    private RegularNewTabPageStation mNtp;

    public static ViewAction repeatedlyUntil(
            final ViewAction action, final Matcher<View> condition, final int maxAttempts) {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return ViewMatchers.isDisplayed();
            }

            @Override
            public String getDescription() {
                return "Repeatedly performing: "
                        + action.getDescription()
                        + " until: "
                        + condition.toString()
                        + " (max attempts: "
                        + maxAttempts
                        + ")";
            }

            @Override
            public void perform(final UiController uiController, final View view) {
                int attempts = 0;
                while (!condition.matches(view) && attempts < maxAttempts) {
                    action.perform(uiController, view);
                    attempts++;
                    uiController.loopMainThreadForAtLeast(100); // Give the UI some time to update
                }
            }
        };
    }

    @Before
    public void setup() {
        PrivacySandboxSurveyController.setEnableForTesting();
        mNtp = mActivityTestRule.startOnNtp();

        // Explicitly remove the `DISABLE_FIRST_RUN_EXPERIENCE` (set via `TestSurveyComponentRule`)
        // commandline switch which prevents us from receiving a valid prompt type via
        // `PrivacySandboxBridge`.
        CommandLine.getInstance().removeSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (PrivacySandboxDialogController.getDialog() != null) {
                        PrivacySandboxDialogController.getDialog().dismiss();
                    }
                });
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_SENTIMENT_SURVEY
                + ":sentiment-survey-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
    })
    public void sentimentSurveyAcceptSurvey() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        TabCreatorUtil.launchNtp(
                                mActivityTestRule
                                        .getActivity()
                                        .getTabCreator(/* incognito= */ false)));
        waitForSurveyMessageToShow();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var unused =
                            mSurveyMessage.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
                });
        Assert.assertEquals(
                "Last shown survey triggerId not match.",
                TestSurveyUtils.TEST_TRIGGER_ID_FOO,
                mTestSurveyComponentRule.getLastShownTriggerId());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_SENTIMENT_SURVEY
                + ":sentiment-survey-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
    })
    @RequiresRestart("State from previous test may prevent survey from surfacing")
    public void sentimentSurveyDismissSurvey() {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        TabCreatorUtil.launchNtp(
                                mActivityTestRule
                                        .getActivity()
                                        .getTabCreator(/* incognito= */ false)));
        waitForSurveyMessageToShow();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mMessageDispatcher.dismissMessage(mSurveyMessage, DismissReason.GESTURE));
        Assert.assertTrue(
                "Survey displayed not recorded.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    public void sentimentSurveyNotShown() {
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        Assert.assertFalse(
                "Survey was displayed.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    private void waitForSurveyMessageToShow() {
        Tab tab = mActivityTestRule.getActivityTab();
        CriteriaHelper.pollUiThread(() -> !tab.isLoading() && tab.isUserInteractable());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMessageDispatcher =
                            MessageDispatcherProvider.from(
                                    mActivityTestRule.getActivity().getWindowAndroid());
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    mSurveyMessage = getSurveyMessage();
                    return mSurveyMessage != null;
                });
    }

    private PropertyModel getSurveyMessage() {
        List<MessageStateHandler> messages =
                MessagesTestHelper.getEnqueuedMessages(
                        mMessageDispatcher, MessageIdentifier.CHROME_SURVEY);
        return messages.size() == 0 ? null : MessagesTestHelper.getCurrentMessage(messages.get(0));
    }
}
