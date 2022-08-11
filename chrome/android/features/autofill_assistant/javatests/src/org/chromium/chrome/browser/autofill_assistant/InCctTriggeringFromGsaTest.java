// Copyright 2021 The Chromium Authors. All rights reserved.
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
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.provider.Browser;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.autofill_assistant.proto.GetTriggerScriptsResponseProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.AutofillAssistantPreferencesUtil;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for heuristics-based triggering in tabs created by GSA.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class InCctTriggeringFromGsaTest {
    private static final String HTML_DIRECTORY = "/components/test/data/autofill_assistant/html/";
    private static final String TEST_PAGE_UNSUPPORTED = "autofill_assistant_target_website.html";
    private static final String TEST_PAGE_SUPPORTED = "cart.html";

    @Rule
    public final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private String getTargetWebsiteUrl(String testPage) {
        return mTestRule.getTestServer().getURL(HTML_DIRECTORY + testPage);
    }

    @Before
    public void setUp() {
        mTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils
                        .createMinimalCustomTabIntent(InstrumentationRegistry.getTargetContext(),
                                getTargetWebsiteUrl(TEST_PAGE_UNSUPPORTED))
                        .putExtra(Browser.EXTRA_APPLICATION_ID, IntentHandler.PACKAGE_GSA));

        // Enable MSBB.
        AutofillAssistantPreferencesUtil.setProactiveHelpPreference(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
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
     * Tests a simple trigger heuristic that checks URLs for the appearance of 'cart'.
     *
     * {
     *   "heuristics":[
     *     {
     *       "intent":"SHOPPING_ASSISTED_CHECKOUT",
     *       "conditionSet":{
     *         "urlMatches":".*cart.*"
     *       }
     *     }
     *   ]
     * }
     */
    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.
    Add({"enable-features=AutofillAssistantInCctTriggering<FakeStudyName,"
              +"AutofillAssistantUrlHeuristics<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:json_parameters/"
              +"%7B%22heuristics%22%3A%5B%7B%22intent%22%3A%22SHOPPING_ASSISTED_CHECKOUT"
              +"%22%2C%22conditionSet%22%3A%7B%22urlMatches%22%3A%22.*cart.*%22%7D%7D%5D%7D"})
    // clang-format on
    public void
    triggerImplicitlyOnSupportedSite() {
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
        AutofillAssistantPreferencesUtil.setProactiveHelpPreference(false);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillAssistantTabHelper
                                   .get(TabModelUtils.getCurrentTab(
                                           mTestRule.getActivity().getCurrentTabModel()))
                                   .forceSettingsChangeNotificationForTesting());
        waitUntilViewAssertionTrue(
                withText("TriggerScript"), doesNotExist(), DEFAULT_POLLING_INTERVAL);

        // Re-enabling the proactive help setting should restart the trigger script, since we are
        // still on a supported URL.
        testServiceRequestSender.setNextResponse(
                /* httpStatus = */ 200, createDefaultTriggerScriptResponse("TriggerScript"));
        AutofillAssistantPreferencesUtil.setProactiveHelpPreference(true);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillAssistantTabHelper
                                   .get(TabModelUtils.getCurrentTab(
                                           mTestRule.getActivity().getCurrentTabModel()))
                                   .forceSettingsChangeNotificationForTesting());
        waitUntilViewMatchesCondition(withText("TriggerScript"), isDisplayed());
    }
    /**
     * Tests a simple trigger heuristic that checks URL for the appearance of cart.
     *
     * {
     *   "intent":"COWIN_VACCINATION",
     *   "heuristics":[
     *     {
     *       "conditionSet":{
     *         "urlMatches":".*cart.*"
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
              +"conditionSet%22%3A%7B%22urlMatches%22%3A%22.%2Acart.%2A%22%7D%7D%5D%2C%22"
              +"enabledInCustomTabs%22%3Atrue%2C%22enabledForSignedOutUsers%22%3Atrue%7D"})
    // clang-format on
    public void
    triggerImplicitlyOnSupportedSiteNewConfig() {
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

        }
}
