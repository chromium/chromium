// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipIcon;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.DrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.Empty;
import org.chromium.chrome.browser.autofill_assistant.proto.GetTriggerScriptsResponseProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptConditionsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptProto.TriggerScriptAction;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptUIProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptUIProto.TriggerChip;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.signin.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Arrays;

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
    }

    private void setupRegularScripts(AutofillAssistantTestScript... scripts) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Arrays.asList(scripts));
        testService.scheduleForInjection();
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

    /**
     * Creates a default UI, similar to the intended experience. It comprises three chips:
     * 'Preferences', 'Not now', 'Continue'. 'Preferences' opens the cancel popup containing 'Not
     * for this session' and 'Never show again'. Optionally, a blue message bubble and a default
     * progress bar are shown.
     */
    private TriggerScriptUIProto.Builder createDefaultUI(
            String statusMessage, String bubbleMessage, boolean withProgressBar) {
        TriggerScriptUIProto.Builder builder =
                TriggerScriptUIProto.newBuilder()
                        .setStatusMessage(statusMessage)
                        .setCalloutMessage(bubbleMessage)
                        .addLeftAlignedChips(
                                TriggerChip.newBuilder()
                                        .setChip(ChipProto.newBuilder()
                                                         .setType(ChipType.NORMAL_ACTION)
                                                         .setIcon(ChipIcon.ICON_OVERFLOW))
                                        .setAction(TriggerScriptAction.SHOW_CANCEL_POPUP))
                        .addRightAlignedChips(
                                TriggerChip.newBuilder()
                                        .setChip(ChipProto.newBuilder()
                                                         .setType(ChipType.NORMAL_ACTION)
                                                         .setText("Not now"))
                                        .setAction(TriggerScriptAction.NOT_NOW))
                        .addRightAlignedChips(
                                TriggerChip.newBuilder()
                                        .setChip(ChipProto.newBuilder()
                                                         .setType(ChipType.HIGHLIGHTED_ACTION)
                                                         .setText("Continue"))
                                        .setAction(TriggerScriptAction.ACCEPT))
                        .setCancelPopup(
                                TriggerScriptUIProto.Popup.newBuilder()
                                        .addChoices(
                                                TriggerScriptUIProto.Popup.Choice.newBuilder()
                                                        .setText("Not for this session")
                                                        .setAction(
                                                                TriggerScriptAction.CANCEL_SESSION))
                                        .addChoices(TriggerScriptUIProto.Popup.Choice.newBuilder()
                                                            .setText("Never show again")
                                                            .setAction(TriggerScriptAction
                                                                               .CANCEL_FOREVER)));
        if (withProgressBar) {
            builder.setProgressBar(
                    TriggerScriptUIProto.ProgressBar.newBuilder()
                            .addStepIcons(DrawableProto.newBuilder().setIcon(
                                    DrawableProto.Icon.PROGRESSBAR_DEFAULT_INITIAL_STEP))
                            .addStepIcons(DrawableProto.newBuilder().setIcon(
                                    DrawableProto.Icon.PROGRESSBAR_DEFAULT_DATA_COLLECTION))
                            .addStepIcons(DrawableProto.newBuilder().setIcon(
                                    DrawableProto.Icon.PROGRESSBAR_DEFAULT_PAYMENT))
                            .addStepIcons(DrawableProto.newBuilder().setIcon(
                                    DrawableProto.Icon.PROGRESSBAR_DEFAULT_FINAL_STEP))
                            .setActiveStep(1));
        }
        return builder;
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    public void setReturningUserFlag() {
        TriggerScriptProto.Builder firstTimeTriggerScript =
                TriggerScriptProto.newBuilder()
                        .setTriggerCondition(
                                TriggerScriptConditionProto.newBuilder().setIsFirstTimeUser(
                                        Empty.newBuilder()))
                        .setUserInterface(createDefaultUI("First time user",
                                /* bubbleMessage = */ "First time message",
                                /* withProgressBar = */ true));

        TriggerScriptProto.Builder returningUserTriggerScript =
                TriggerScriptProto.newBuilder()
                        .setTriggerCondition(TriggerScriptConditionProto.newBuilder().setNoneOf(
                                TriggerScriptConditionsProto.newBuilder().addConditions(
                                        TriggerScriptConditionProto.newBuilder().setIsFirstTimeUser(
                                                Empty.newBuilder()))))
                        .setUserInterface(createDefaultUI("Returning user",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false));
        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(firstTimeTriggerScript)
                        .addTriggerScripts(returningUserTriggerScript)
                        .build();
        setupTriggerScripts(triggerScripts);
        startAutofillAssistantOnTab(TEST_PAGE);

        Assert.assertTrue(
                AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeLiteScriptUser());
        waitUntilViewMatchesCondition(withText("First time user"), isCompletelyDisplayed());
        Assert.assertFalse(
                AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeLiteScriptUser());

        onView(withText("Not now")).perform(click());
        waitUntilViewMatchesCondition(withText("Returning user"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    public void elementCondition() throws Exception {
        SelectorProto.Builder touch_area_four = SelectorProto.newBuilder().addFilters(
                SelectorProto.Filter.newBuilder().setCssSelector("#touch_area_four"));
        TriggerScriptProto.Builder buttonVisibleTriggerScript =
                TriggerScriptProto.newBuilder()
                        .setTriggerCondition(TriggerScriptConditionProto.newBuilder().setSelector(
                                touch_area_four))
                        .setUserInterface(createDefaultUI("Area visible",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true));

        TriggerScriptProto.Builder buttonInvisibleTriggerScript =
                TriggerScriptProto.newBuilder()
                        .setTriggerCondition(TriggerScriptConditionProto.newBuilder().setNoneOf(
                                TriggerScriptConditionsProto.newBuilder().addConditions(
                                        TriggerScriptConditionProto.newBuilder().setSelector(
                                                touch_area_four))))
                        .setUserInterface(createDefaultUI("Area invisible",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false));
        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(buttonVisibleTriggerScript)
                        .addTriggerScripts(buttonInvisibleTriggerScript)
                        .build();
        setupTriggerScripts(triggerScripts);
        startAutofillAssistantOnTab(TEST_PAGE);

        waitUntilViewMatchesCondition(withText("Area visible"), isCompletelyDisplayed());
        onView(withId(R.id.progress_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.step_progress_bar)).check(matches(isDisplayed()));

        tapElement(mTestRule, "touch_area_four");
        waitUntilViewMatchesCondition(withText("Area invisible"), isCompletelyDisplayed());
        onView(withId(R.id.progress_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.step_progress_bar)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    public void transitionToOnboardingAndRegularScript() throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true));

        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(triggerScript)
                        .build();
        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, false);
        startAutofillAssistantOnTab(TEST_PAGE);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        setupRegularScripts(script);

        onView(withText("Continue")).perform(click());
        // Wait for onboarding.
        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), isCompletelyDisplayed());
        // Accept onboarding.
        onView(withId(R.id.button_init_ok)).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    public void transitionToRegularScriptWithoutOnboarding() throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true));

        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(triggerScript)
                        .build();
        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, true);
        startAutofillAssistantOnTab(TEST_PAGE);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        setupRegularScripts(script);

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
    }
}
