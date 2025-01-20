// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import android.content.Context;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.hats.TestSurveyUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Integration test for {@link PrivacySandboxSurveyController} using {@link SurveyClient}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PrivacySandboxSurveyControllerIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public TestSurveyUtils.TestSurveyComponentRule mTestSurveyComponentRule =
            new TestSurveyUtils.TestSurveyComponentRule();

    private MessageDispatcher mMessageDispatcher;
    private PropertyModel mSurveyMessage;
    private String mTestPage;
    private EmbeddedTestServer mTestServer;
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";

    @Before
    public void setup() {
        PrivacySandboxSurveyController.setEnableForTesting();
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);

        // CCT setup.
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        Context appContext = getInstrumentation().getTargetContext().getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);

        // Explicitly remove the `DISABLE_FIRST_RUN_EXPERIENCE` (set via `TestSurveyComponentRule`)
        // commandline switch which prevents us from receiving a valid prompt type via
        // `PrivacySandboxBridge`.
        CommandLine.getInstance().removeSwitch(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE);
        PrivacySandboxSurveyController.overrideAdsCctSurveyDelayForTesting(
                /* delayMilliseconds= */ 0);
    }

    private Intent createMinimalCustomTabIntent() {
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), mTestPage);
        var token = SessionHolder.getSessionHolderFromIntent(intent);
        // x86 devices will return a null package name unless we explicitly override it.
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        connection.newSession(token.getSessionAsCustomTab());
        connection.overridePackageNameForSessionForTesting(token, "org.chromium.chrome.tests");
        return intent;
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":app-id/org.chromium.chrome.tests/"
                + "row-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    @DisableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    public void adsCctSurveyForRowControlAcceptSurvey() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
        waitForSurveyMessageToShowOnCct();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
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
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":app-id/org.chromium.chrome.tests/"
                + "row-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    @DisableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    public void adsCctSurveyForRowControlDismissSurvey() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
        waitForSurveyMessageToShowOnCct();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mMessageDispatcher.dismissMessage(mSurveyMessage, DismissReason.GESTURE));
        Assert.assertTrue(
                "Survey displayed not recorded.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":app-id/org.chromium.chrome.tests/",
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    @DisableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    public void adsCctSurveyForRowControlNotShownWhenNoTriggerIdProvided() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        Assert.assertFalse(
                "Survey was displayed.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":app-id/org.chromium.chrome.tests/"
                + "eea-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true"
    })
    @DisableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    public void adsCctSurveyForEeaControlAcceptSurvey() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
        waitForSurveyMessageToShowOnCct();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
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
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":app-id/org.chromium.chrome.tests/"
                + "eea-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true"
    })
    @DisableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    public void adsCctSurveyForEeaControlDismissSurvey() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
        waitForSurveyMessageToShowOnCct();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mMessageDispatcher.dismissMessage(mSurveyMessage, DismissReason.GESTURE));
        Assert.assertTrue(
                "Survey displayed not recorded.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":app-id/org.chromium.chrome.tests/",
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true"
    })
    @DisableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    public void adsCctSurveyForEeaControlNotShownWhenNoTriggerIdProvided() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        Assert.assertFalse(
                "Survey was displayed.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":app-id/org.chromium.chrome.tests/",
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true"
                + "/force-show-notice-row-for-testing/true/notice-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT
    })
    public void adsCctSurveyForControlSurveyNotShownWhenAdsNoticeCctFeatureEnabled() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        Assert.assertFalse(
                "Survey was displayed.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY + ":app-id/invalid-app-id/",
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true"
                + "/force-show-notice-row-for-testing/true/notice-required/true",
    })
    @DisableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT})
    public void adsCctSurveyForControlSurveyNotShownWithInvalidAppId() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        Assert.assertFalse(
                "Survey was displayed.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true"
                + "/force-show-notice-row-for-testing/true/notice-required/true",
    })
    @DisableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
    })
    public void adsCctSurveyForControlSurveyNotShownWithSurveyFeatureDisabled() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        Assert.assertEquals(
                "Survey was displayed.", mTestSurveyComponentRule.getLastShownTriggerId(), null);
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
                () -> {
                    mActivityTestRule.getActivity().getTabCreator(false).launchNtp();
                });
        waitForSurveyMessageToShow();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
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
                () -> {
                    mActivityTestRule.getActivity().getTabCreator(false).launchNtp();
                });
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

    private void waitForSurveyMessageToShowOnCct() {
        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        CriteriaHelper.pollUiThread(() -> !tab.isLoading() && tab.isUserInteractable());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMessageDispatcher =
                            MessageDispatcherProvider.from(
                                    mCustomTabActivityTestRule.getActivity().getWindowAndroid());
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    mSurveyMessage = getSurveyMessage();
                    return mSurveyMessage != null;
                });
    }

    private void waitForSurveyMessageToShow() {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
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
