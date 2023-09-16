// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.hats.TestSurveyUtils;
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

import java.util.List;

/**
 * Integration test for {@link ChromeSurveyController} using {@link SurveyClient}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({"force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:autodismiss_duration_ms/500/"
                + TestSurveyUtils.TEST_SURVEY_TRIGGER_ID_OVERRIDE_TEMPLATE
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO})
@Features.EnableFeatures({ChromeFeatureList.ANDROID_HATS_REFACTOR + "<Study",
        ChromeFeatureList.CHROME_SURVEY_NEXT_ANDROID + "<Study",
        ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE + "<Study"})
@Batch(Batch.PER_CLASS)
public class ChromeStartupSurveyIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestSurveyUtils.TestSurveyComponentRule mTestSurveyComponentRule =
            new TestSurveyUtils.TestSurveyComponentRule();
    private MessageDispatcher mMessageDispatcher;
    private PropertyModel mSurveyMessage;

    @Before
    public void setup() {
        ChromeSurveyController.forceIsUMAEnabledForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();
        waitForSurveyMessagePresented();
    }

    @Test
    @MediumTest
    public void acceptSurvey() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mSurveyMessage.get(MessageBannerProperties.ON_PRIMARY_ACTION).get(); });
        Assert.assertEquals("Last shown survey triggerId not match.",
                TestSurveyUtils.TEST_TRIGGER_ID_FOO,
                mTestSurveyComponentRule.getLastShownTriggerId());
    }

    @Test
    @MediumTest
    public void dismissSurvey() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mMessageDispatcher.dismissMessage(mSurveyMessage, DismissReason.GESTURE));
        Assert.assertTrue("Survey displayed not recorded.",
                SharedPreferencesManager.getInstance().contains(
                        ChromePreferenceKeys.CHROME_SURVEY_PROMPT_DISPLAYED_TIMESTAMP.createKey(
                                TestSurveyUtils.TEST_TRIGGER_ID_FOO)));
    }

    private void waitForSurveyMessagePresented() {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        CriteriaHelper.pollUiThread(() -> !tab.isLoading() && tab.isUserInteractable());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMessageDispatcher = MessageDispatcherProvider.from(
                    mActivityTestRule.getActivity().getWindowAndroid());
        });
        CriteriaHelper.pollUiThread(() -> {
            mSurveyMessage = getSurveyMessage();
            return mSurveyMessage != null;
        });
    }

    private PropertyModel getSurveyMessage() {
        List<MessageStateHandler> messages = MessagesTestHelper.getEnqueuedMessages(
                mMessageDispatcher, MessageIdentifier.CHROME_SURVEY);
        return messages.size() == 0 ? null : MessagesTestHelper.getCurrentMessage(messages.get(0));
    }
}
