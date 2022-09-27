// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitAtLeast;

import android.provider.Browser;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill_assistant.proto.GetTriggerScriptsResponseProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for heuristics-based triggering in tabs created *not* by GSA.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class InCctTriggeringFromNonGsaTest {
    private static final String HTML_DIRECTORY = "/components/test/data/autofill_assistant/html/";
    private static final String TEST_PAGE_UNSUPPORTED = "autofill_assistant_target_website.html";
    private static final String TEST_PAGE_SUPPORTED = "cart.html";

    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private String getTargetWebsiteUrl(String testPage) {
        return mTestRule.getTestServer().getURL(HTML_DIRECTORY + testPage);
    }

    private void setupTriggerScripts(GetTriggerScriptsResponseProto triggerScripts) {
        AutofillAssistantTestServiceRequestSender testServiceRequestSender =
                new AutofillAssistantTestServiceRequestSender();
        testServiceRequestSender.setNextResponse(/* httpStatus = */ 200, triggerScripts);
        testServiceRequestSender.scheduleForInjection();
    }

    private void enableMsbb() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .setBoolean(Pref.AUTOFILL_ASSISTANT_TRIGGER_SCRIPTS_ENABLED, true);
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    Profile.getLastUsedRegularProfile(), true);

            // Force native to pick up the changes we made to Chrome preferences.
            AutofillAssistantTabHelper
                    .get(TabModelUtils.getCurrentTab(mTestRule.getActivity().getCurrentTabModel()))
                    .forceSettingsChangeNotificationForTesting();
        });
    }

    /**
     * Tests a simple trigger heuristic that checks URLs for the appearance of 'cart'. The activity
     * is started by a fake third-party app and thus should not trigger autofill assistant.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"variations-override-country=us"})
    public void doNotTriggerForExternalCct() throws InterruptedException {
        mTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils
                        .createMinimalCustomTabIntent(InstrumentationRegistry.getTargetContext(),
                                getTargetWebsiteUrl(TEST_PAGE_UNSUPPORTED))
                        .putExtra(Browser.EXTRA_APPLICATION_ID, "com.example"));
        enableMsbb();

        AutofillAssistantTestServiceRequestSender testServiceRequestSender =
                new AutofillAssistantTestServiceRequestSender();
        testServiceRequestSender.scheduleForInjection();

        // The test service should not receive a request while waiting here. If it does, it will
        // assert-fail because no response has been configured.
        mTestRule.loadUrl(getTargetWebsiteUrl(TEST_PAGE_SUPPORTED));
        waitAtLeast(DEFAULT_MAX_TIME_TO_POLL);
    }

    /**
     * Tests a simple trigger heuristic that checks URLs for the appearance of 'cart'. The activity
     * does not identify its origin and thus should not trigger autofill assistant.
     */
    @Test
    @MediumTest
    @CommandLineFlags.Add({"variations-override-country=us"})
    public void doNotTriggerForCctWithUnknownOrigin() throws InterruptedException {
        mTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        mTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        getTargetWebsiteUrl(TEST_PAGE_UNSUPPORTED)));
        enableMsbb();

        AutofillAssistantTestServiceRequestSender testServiceRequestSender =
                new AutofillAssistantTestServiceRequestSender();
        testServiceRequestSender.scheduleForInjection();

        // The test service should not receive a request while waiting here. If it does, it will
        // assert-fail because no response has been configured.
        mTestRule.loadUrl(getTargetWebsiteUrl(TEST_PAGE_SUPPORTED));
        waitAtLeast(DEFAULT_MAX_TIME_TO_POLL);
    }
}
