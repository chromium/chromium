// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.swipeUp;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Context;
import android.content.Intent;
import android.view.View;
import android.widget.ScrollView;

import androidx.test.core.app.ApplicationProvider;
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
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.hats.TestSurveyUtils;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
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
import org.chromium.ui.base.DeviceFormFactor;
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
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveRule =
            new AutomotiveContextWrapperTestRule();

    @Rule
    public TestSurveyUtils.TestSurveyComponentRule mTestSurveyComponentRule =
            new TestSurveyUtils.TestSurveyComponentRule();

    private MessageDispatcher mMessageDispatcher;
    private PropertyModel mSurveyMessage;
    private String mTestPage;
    private EmbeddedTestServer mTestServer;
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";

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
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);

        // CCT setup.
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        Context appContext = getInstrumentation().getTargetContext().getApplicationContext();
        mTestServer = EmbeddedTestServer.createAndStartServer(appContext);
        mTestPage = mTestServer.getURL(TEST_PAGE);
        PrivacySandboxDialogController.disableAnimations(true);
        mAutomotiveRule.setIsAutomotive(false);

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
                    if (mCustomTabActivityTestRule.getActivity() == null) return;
                    AppMenuCoordinator coordinator =
                            mCustomTabActivityTestRule.getAppMenuCoordinator();
                    // CCT doesn't always have a menu (ex. in the media viewer).
                    if (coordinator == null) return;
                    AppMenuHandler handler = coordinator.getAppMenuHandler();
                    if (handler != null) handler.hideAppMenu();
                });
        CustomTabsTestUtils.cleanupSessions();
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

    // TODO(crbug.com/389997409): Move this to a shared test util library.
    private void tryClickOn(Matcher<View> viewMatcher) {
        // Scroll down till the view is visible.
        onViewWaiting(
                        allOf(
                                instanceOf(ScrollView.class),
                                withId(R.id.privacy_sandbox_dialog_scroll_view)))
                .check(matches(isDisplayed()))
                .perform(
                        repeatedlyUntil(
                                swipeUp(),
                                hasDescendant(allOf(viewMatcher)),
                                5 // Max attempt count.
                                ));
        onViewWaiting(viewMatcher, true).check(matches(isCompletelyDisplayed())).perform(click());
    }

    private void acknowledgeRowNotice() {
        onViewWaiting(withId(R.id.privacy_sandbox_dialog)).check(matches(isCompletelyDisplayed()));
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true)
                .check(matches(isCompletelyDisplayed()));
        tryClickOn(withId(R.id.ack_button));
    }

    private void interactWithEeaConsentAndNotice(boolean shouldAcceptConsent) {
        // Interact with the consent
        onViewWaiting(withId(R.id.privacy_sandbox_dialog)).check(matches(isCompletelyDisplayed()));
        if (shouldAcceptConsent) {
            tryClickOn(withId(R.id.ack_button));
        } else {
            tryClickOn(withId(R.id.no_button));
        }

        // Acknowledge the EEA notice
        onViewWaiting(withId(R.id.privacy_sandbox_dialog)).check(matches(isCompletelyDisplayed()));
        onViewWaiting(withId(R.id.privacy_sandbox_notice_title), true)
                .check(matches(isCompletelyDisplayed()));
        tryClickOn(withId(R.id.ack_button));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
                + "row-acknowledged-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForRowTreatmentAcceptSurvey() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        acknowledgeRowNotice();
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
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
                + "row-acknowledged-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForRowTreatmentDismissSurvey() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        acknowledgeRowNotice();
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
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
                + "eea-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/row-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/eea-declined-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/eea-accepted-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForRowTreatmentNotShownWhenTriggerIdNotSet() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        acknowledgeRowNotice();
        Assert.assertFalse(
                "Survey was displayed.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT
    })
    @DisableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY})
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForRowNoticeNotShownWithSurveyFeatureDisabled() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        acknowledgeRowNotice();
        Assert.assertEquals(
                "Survey was displayed.", mTestSurveyComponentRule.getLastShownTriggerId(), null);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
                + "accepted-trigger-rate/1.0/"
                + "eea-accepted-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForEeaAcceptedConsentAcceptSurvey() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        interactWithEeaConsentAndNotice(/* shouldAcceptConsent= */ true);
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
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
                + "accepted-trigger-rate/1.0/"
                + "eea-accepted-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForEeaAcceptedConsentDismissSurvey() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        interactWithEeaConsentAndNotice(/* shouldAcceptConsent= */ true);
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
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
                + "accepted-trigger-rate/1.0/"
                + "eea-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/row-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/eea-declined-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/row-acknowledged-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForEeaAcceptedNotShownWhenTriggerIdNotSet() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        interactWithEeaConsentAndNotice(/* shouldAcceptConsent= */ true);
        Assert.assertFalse(
                "Survey was displayed.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
                + "accepted-trigger-rate/0.0/"
                + "eea-accepted-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForEeaAcceptedNotShownSurveyDueToAcceptedTriggerRate() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        interactWithEeaConsentAndNotice(/* shouldAcceptConsent= */ true);
        Assert.assertFalse(
                "Survey was displayed.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
                + "declined-trigger-rate/1.0/"
                + "eea-declined-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForEeaDeclinedConsentAcceptSurvey() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        interactWithEeaConsentAndNotice(/* shouldAcceptConsent= */ false);
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
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
                + "declined-trigger-rate/1.0/"
                + "eea-declined-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForEeaDeclinedConsentDismissSurvey() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        interactWithEeaConsentAndNotice(/* shouldAcceptConsent= */ false);
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
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
                + "declined-trigger-rate/0.0/"
                + "eea-accepted-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForEeaDeclinedNotShownSurveyDueToDeclinedTriggerRate() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        interactWithEeaConsentAndNotice(/* shouldAcceptConsent= */ false);
        Assert.assertFalse(
                "Survey was displayed.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
                + "declined-trigger-rate/1.0/"
                + "eea-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/row-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/eea-accepted-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/row-acknowledged-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForEeaDeclinedNotShownWhenTriggerIdNotSet() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        interactWithEeaConsentAndNotice(/* shouldAcceptConsent= */ false);
        Assert.assertFalse(
                "Survey was displayed.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
                + "declined-trigger-rate/1.0/"
                + "eea-declined-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/eea-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/row-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/eea-accepted-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/row-acknowledged-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true"
                + "/force-show-notice-row-for-testing/true/notice-required/true",
    })
    @DisableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT})
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForTreatmentNotShownWhenAdsNoticeCctFeatureDisabled() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        onView(withId(R.id.privacy_sandbox_dialog)).check(doesNotExist());
        Assert.assertFalse(
                "Survey was displayed.",
                mTestSurveyComponentRule.isPromptShownForTriggerId(
                        TestSurveyUtils.TEST_TRIGGER_ID_FOO));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true",
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT
    })
    @DisableFeatures({ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY})
    // TODO(crbug.com/391968140): Re-enable tests when supporting tablets
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void adsCctSurveyForEeaConsentNotShownWithSurveyFeatureDisabled() {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        interactWithEeaConsentAndNotice(/* shouldAcceptConsent= */ true);
        Assert.assertEquals(
                "Survey was displayed.", mTestSurveyComponentRule.getLastShownTriggerId(), null);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
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
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
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
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "eea-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/row-acknowledged-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/eea-declined-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/eea-accepted-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-notice-row-for-testing/true/notice-required/true"
    })
    @DisableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    public void adsCctSurveyForRowControlNotShownWhenTriggerIdNotSet() {
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
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
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
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "survey-delay-ms/0/"
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
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "row-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/row-acknowledged-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/eea-declined-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/eea-accepted-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
        ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4
                + ":force-show-consent-for-testing/true/consent-required/true"
    })
    @DisableFeatures({
        ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
    })
    public void adsCctSurveyForEeaControlNotShownWhenTriggerIdNotSet() {
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
                + ":survey-app-id/org.chromium.chrome.tests/"
                + "eea-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/row-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
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
        ChromeFeatureList.PRIVACY_SANDBOX_CCT_ADS_NOTICE_SURVEY
                + ":survey-app-id/invalid-app-id/"
                + "eea-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO
                + "/row-control-trigger-id/"
                + TestSurveyUtils.TEST_TRIGGER_ID_FOO,
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
