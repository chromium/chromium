// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.createDefaultTriggerScriptUI;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitAtLeast;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.provider.Browser;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.autofill_assistant.proto.GetTriggerScriptsResponseProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for heuristics-based triggering in tabs created by GSA.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class InCctTriggeringFromGsaTest {
    private static final String HTML_DIRECTORY = "/components/test/data/autofill_assistant/html/";
    private static final String TEST_PAGE_UNSUPPORTED = "autofill_assistant_target_website.html";
    private static final String TEST_PAGE_SUPPORTED = "cart.html";
    private static final String TEST_PAGE_FORM = "form_target_website.html";

    @Rule
    public final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private String getTargetWebsiteUrl(String testPage) {
        return mTestRule.getTestServer().getURL(HTML_DIRECTORY + testPage);
    }

    /** Helper function for setting a pref that must be called on the UI thread. */
    private void setBooleanPref(String preference, boolean value) {
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        prefService.setBoolean(preference, value);
    }

    @Before
    public void setUp() {
        mTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils
                        .createMinimalCustomTabIntent(InstrumentationRegistry.getTargetContext(),
                                getTargetWebsiteUrl(TEST_PAGE_UNSUPPORTED))
                        .putExtra(Browser.EXTRA_APPLICATION_ID, IntentHandler.PACKAGE_GSA));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Enable trigger scripts and MSBB.
            setBooleanPref(Pref.AUTOFILL_ASSISTANT_TRIGGER_SCRIPTS_ENABLED, true);
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    Profile.getLastUsedRegularProfile(), true);

            // Force native to pick up the changes we made to Chrome preferences.
            AutofillAssistantTabHelper
                    .get(TabModelUtils.getCurrentTab(mTestRule.getActivity().getCurrentTabModel()))
                    .forceSettingsChangeNotificationForTesting();
        });
    }

    private GetTriggerScriptsResponseProto createDefaultTriggerScriptResponse(
            String statusMessage) {
        return GetTriggerScriptsResponseProto.newBuilder()
                .addTriggerScripts(TriggerScriptProto.newBuilder().setUserInterface(
                        createDefaultTriggerScriptUI(statusMessage,
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false)))
                .build();
    }

    /**
     * Tests the launched shopping and cart trigger heuristic.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"variations-override-country=us"})
    public void triggerImplicitlyOnSupportedSite() {
        AutofillAssistantTestServiceRequestSender testServiceRequestSender =
                new AutofillAssistantTestServiceRequestSender();
        testServiceRequestSender.setNextResponse(
                /* httpStatus = */ 200, createDefaultTriggerScriptResponse("TriggerScript"));
        testServiceRequestSender.scheduleForInjection();

        mTestRule.loadUrl(getTargetWebsiteUrl(TEST_PAGE_UNSUPPORTED));
        onView(withText("TriggerScript")).check(doesNotExist());

        mTestRule.loadUrl(getTargetWebsiteUrl(TEST_PAGE_SUPPORTED));
        // Note: allow for some extra time here to account for both the navigation and the start.
        waitUntilViewMatchesCondition(
                withText("TriggerScript"), isDisplayed(), 2 * DEFAULT_MAX_TIME_TO_POLL);

        // Disabling the proactive help setting should stop the trigger script.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            setBooleanPref(Pref.AUTOFILL_ASSISTANT_TRIGGER_SCRIPTS_ENABLED, false);
            AutofillAssistantTabHelper
                    .get(TabModelUtils.getCurrentTab(mTestRule.getActivity().getCurrentTabModel()))
                    .forceSettingsChangeNotificationForTesting();
        });
        waitUntilViewAssertionTrue(
                withText("TriggerScript"), doesNotExist(), DEFAULT_POLLING_INTERVAL);

        // Re-enabling the proactive help setting should restart the trigger script, since we are
        // still on a supported URL.
        testServiceRequestSender.setNextResponse(
                /* httpStatus = */ 200, createDefaultTriggerScriptResponse("TriggerScript"));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            setBooleanPref(Pref.AUTOFILL_ASSISTANT_TRIGGER_SCRIPTS_ENABLED, true);
            AutofillAssistantTabHelper
                    .get(TabModelUtils.getCurrentTab(mTestRule.getActivity().getCurrentTabModel()))
                    .forceSettingsChangeNotificationForTesting();
        });
        waitUntilViewMatchesCondition(withText("TriggerScript"), isDisplayed());
    }

    /**
     * Tests a simple trigger heuristic that checks URL for the appearance of 'form'.
     *
     * {
     *   "intent":"COWIN_VACCINATION",
     *   "heuristics":[
     *     {
     *       "conditionSet":{
     *         "urlMatches":".*form.*"
     *       }
     *     }
     *   ],
     *   "enabledInCustomTabs":true,
     *   "enabledForSignedOutUsers":true
     * }
     */
    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.
    Add({"enable-features=AutofillAssistantUrlHeuristic1<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:json_parameters/"
              +"%7B%22intent%22%3A%22COWIN_VACCINATION%22%2C%22heuristics%22%3A%5B%7B%22"
              +"conditionSet%22%3A%7B%22urlMatches%22%3A%22.%2Aform.%2A%22%7D%7D%5D%2C%22"
              +"enabledInCustomTabs%22%3Atrue%2C%22enabledForSignedOutUsers%22%3Atrue%7D"})
    // clang-format on
    public void
    triggerImplicitlyOnSupportedSiteUsingFinchConfig() {
        AutofillAssistantTestServiceRequestSender testServiceRequestSender =
                new AutofillAssistantTestServiceRequestSender();
        testServiceRequestSender.setNextResponse(
                /* httpStatus = */ 200, createDefaultTriggerScriptResponse("TriggerScript"));
        testServiceRequestSender.scheduleForInjection();

        mTestRule.loadUrl(getTargetWebsiteUrl(TEST_PAGE_UNSUPPORTED));
        onView(withText("TriggerScript")).check(doesNotExist());

        mTestRule.loadUrl(getTargetWebsiteUrl(TEST_PAGE_FORM));
        // Note: allow for some extra time here to account for both the navigation and the start.
        waitUntilViewMatchesCondition(
                withText("TriggerScript"), isDisplayed(), 2 * DEFAULT_MAX_TIME_TO_POLL);
    }

    @Test
    @MediumTest
    @CommandLineFlags.
    Add({"variations-override-country=us", "disable-features=AutofillAssistantInCctTriggering"})
    public void doesNotTriggerIfFeatureDisabled() {
        AutofillAssistantTestServiceRequestSender testServiceRequestSender =
                new AutofillAssistantTestServiceRequestSender();
        testServiceRequestSender.scheduleForInjection();

        mTestRule.loadUrl(getTargetWebsiteUrl(TEST_PAGE_SUPPORTED));

        // The test service should not receive a request while waiting here. If it does, it will
        // assert-fail because no response has been configured.
        mTestRule.loadUrl(getTargetWebsiteUrl(TEST_PAGE_SUPPORTED));
        waitAtLeast(DEFAULT_MAX_TIME_TO_POLL);
    }
}
