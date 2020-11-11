// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill_assistant.proto.Empty;
import org.chromium.chrome.browser.autofill_assistant.proto.GetTriggerScriptsResponseProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptUIProto;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/** Integration tests for trigger scripts. */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantTriggerScriptIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mTestRule = new ChromeTabbedActivityTestRule();

    private static final String HTML_DIRECTORY = "/components/test/data/autofill_assistant/html/";
    private static final String TEST_PAGE = "autofill_assistant_target_website.html";

    private EmbeddedTestServer mTestServer;

    private String getURL(String page) {
        return mTestServer.getURL(HTML_DIRECTORY + page);
    }

    private void setupTriggerScripts(GetTriggerScriptsResponseProto triggerScripts) {
        AutofillAssistantTestServiceRequestSender testServiceRequestSender =
                new AutofillAssistantTestServiceRequestSender();
        testServiceRequestSender.setNextResponse(/* httpStatus = */ 200, triggerScripts);
        testServiceRequestSender.scheduleForInjection();
        ;
    }

    private void startAutofillAssistantOnTab(String pageToLoad) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillAssistantFacade.start(mTestRule.getActivity(),
                                AutofillAssistantArguments.newBuilder()
                                        .fromBundle(null)
                                        .withInitialUrl(getURL(pageToLoad))
                                        .addParameter("REQUEST_TRIGGER_SCRIPT", true)
                                        .build()));
    }

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startMainActivityWithURL(getURL(TEST_PAGE));

        // Enable MSBB.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                                AutofillAssistantUiController.getProfile(), true));
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    public void triggerScriptSmokeTest() {
        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(
                                TriggerScriptProto.newBuilder()
                                        .setTriggerCondition(
                                                TriggerScriptConditionProto.newBuilder()
                                                        .setIsFirstTimeUser(Empty.newBuilder()))
                                        .setUserInterface(
                                                TriggerScriptUIProto.newBuilder().setStatusMessage(
                                                        "Hello World!")))
                        .build();
        setupTriggerScripts(triggerScripts);
        startAutofillAssistantOnTab(TEST_PAGE);

        waitUntilViewMatchesCondition(withText("Hello World!"), isCompletelyDisplayed());
    }
}
