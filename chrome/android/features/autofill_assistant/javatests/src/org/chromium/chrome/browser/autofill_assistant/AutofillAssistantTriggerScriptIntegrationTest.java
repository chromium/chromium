// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.os.Build.VERSION_CODES;
import android.support.test.InstrumentationRegistry;
import android.util.Base64;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
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
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Map;

/** Integration tests for trigger scripts. */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantTriggerScriptIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mTestRule = new ChromeTabbedActivityTestRule();

    private static final String HTML_DIRECTORY = "/components/test/data/autofill_assistant/html/";
    private static final String TEST_PAGE_A = "autofill_assistant_target_website.html";

    private EmbeddedTestServer mTestServer;

    private String getURL(String page) {
        return mTestServer.getURL(HTML_DIRECTORY + page);
    }

    private AutofillAssistantTestServiceRequestSender setupTriggerScripts(
            GetTriggerScriptsResponseProto triggerScripts) {
        AutofillAssistantTestServiceRequestSender testServiceRequestSender =
                new AutofillAssistantTestServiceRequestSender();
        testServiceRequestSender.setNextResponse(/* httpStatus = */ 200, triggerScripts);
        testServiceRequestSender.scheduleForInjection();
        return testServiceRequestSender;
    }

    private void setupRegularScripts(AutofillAssistantTestScript... scripts) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Arrays.asList(scripts));
        testService.scheduleForInjection();
    }

    private void startAutofillAssistantOnTab(String pageToLoad) {
        startAutofillAssistantOnTabWithParams(
                pageToLoad, Collections.singletonMap("REQUEST_TRIGGER_SCRIPT", true));
    }

    private void startAutofillAssistantOnTabWithParams(
            String pageToLoad, Map<String, Object> scriptParameters) {
        TriggerContext.Builder argsBuilder =
                TriggerContext.newBuilder().fromBundle(null).withInitialUrl(getURL(pageToLoad));
        for (Map.Entry<String, Object> param : scriptParameters.entrySet()) {
            argsBuilder.addParameter(param.getKey(), param.getValue());
        }
        argsBuilder.addParameter(TriggerContext.PARAMETER_START_IMMEDIATELY, false);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> AutofillAssistantFacade.start(mTestRule.getActivity(), argsBuilder.build()));
    }

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startMainActivityWithURL(getURL(TEST_PAGE_A));

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
        startAutofillAssistantOnTab(TEST_PAGE_A);

        Assert.assertTrue(
                AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeTriggerScriptUser());
        waitUntilViewMatchesCondition(withText("First time user"), isCompletelyDisplayed());
        Assert.assertFalse(
                AutofillAssistantPreferencesUtil.isAutofillAssistantFirstTimeTriggerScriptUser());

        onView(withText("Not now")).perform(click());
        waitUntilViewMatchesCondition(withText("Returning user"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    public void setCancelForeverFlag() {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultUI("Hello world",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true));
        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(triggerScript)
                        .build();
        setupTriggerScripts(triggerScripts);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        Assert.assertTrue(AutofillAssistantPreferencesUtil.isProactiveHelpOn());
        waitUntilViewMatchesCondition(
                withContentDescription(R.string.autofill_assistant_overflow_options),
                isCompletelyDisplayed());
        onView(withContentDescription(R.string.autofill_assistant_overflow_options))
                .perform(click());
        waitUntilViewMatchesCondition(withText("Never show again"), isCompletelyDisplayed());
        onView(withText("Never show again")).perform(click());
        waitUntilViewAssertionTrue(
                withText("Hello world"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertFalse(AutofillAssistantPreferencesUtil.isProactiveHelpOn());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    // Disable translate to prevent the popup from covering part of the website.
    @Features.DisableFeatures("Translate")
    public void elementCondition() throws Exception {
        SelectorProto.Builder touch_area_four = SelectorProto.newBuilder().addFilters(
                SelectorProto.Filter.newBuilder().setCssSelector("#touch_area_one"));
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
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Area visible"), isCompletelyDisplayed());
        onView(withId(R.id.progress_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.step_progress_bar)).check(matches(isDisplayed()));

        tapElement(mTestRule, "touch_area_one");
        waitUntilViewMatchesCondition(withText("Area invisible"), isCompletelyDisplayed());
        onView(withId(R.id.progress_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.step_progress_bar)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    @DisableIf.Build(message = "Flaky on Android P, see https://crbug.com/1154682",
            sdk_is_greater_than = VERSION_CODES.O_MR1, sdk_is_less_than = VERSION_CODES.Q)
    public void
    transitionToOnboardingAndRegularScript() throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true)
                                                  .setRegularScriptLoadingStatusMessage(
                                                          "Loading regular script"));

        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(triggerScript)
                        .build();
        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Done"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
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
        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        onView(withText("Loading regular script")).check(matches(isDisplayed()));
        onView(withId(R.id.progress_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.step_progress_bar)).check(matches(isDisplayed()));
        Assert.assertFalse(AutofillAssistantPreferencesUtil.getShowOnboarding());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    @Features.DisableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_DISABLE_ONBOARDING_FLOW)
    public void transitionToRegularScriptWithoutOnboarding() throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false)
                                                  .setRegularScriptLoadingStatusMessage(
                                                          "Loading regular script"));

        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(triggerScript)
                        .build();
        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, true);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Done"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        setupRegularScripts(script);

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        onView(withText("Loading regular script")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    public void base64TriggerScriptsDontRequireMSBB() {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                                AutofillAssistantUiController.getProfile(), false));
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);

        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false));
        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(triggerScript)
                        .build();
        byte[] triggerScriptsResponse = triggerScripts.toByteArray();
        String base64Response = Base64.encodeToString(triggerScriptsResponse, /* offset = */ 0,
                triggerScriptsResponse.length, Base64.URL_SAFE | Base64.NO_WRAP);
        Assert.assertEquals(0, base64Response.length() % 4);
        startAutofillAssistantOnTabWithParams(
                TEST_PAGE_A, Collections.singletonMap("TRIGGER_SCRIPTS_BASE64", base64Response));

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    public void dontShowOnboardingIfAcceptedInDifferentTab() {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true)
                                                  .setRegularScriptLoadingStatusMessage(
                                                          "Loading regular script"));

        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(triggerScript)
                        .build();
        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        // Simulate the user accepting the onboarding in a different tab.
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, true);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Done"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        setupRegularScripts(script);

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        onView(withText("Loading regular script")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT_DISABLE_ONBOARDING_FLOW,
            ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP})
    public void
    transitionToRegularScriptWithoutOnboardingWithDisableOnboardingFlowFeatureOn()
            throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false)
                                                  .setRegularScriptLoadingStatusMessage(
                                                          "Loading regular script"));
        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(triggerScript)
                        .build();

        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Done"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        setupRegularScripts(script);

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        onView(withText("Loading regular script")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    @DisableIf.
    Build(message = "Fails on Lollipop and Marshmallow Tablet Tester, https://crbug.com/1158435",
            sdk_is_less_than = VERSION_CODES.N)
    public void switchToNewTabAndThenBack() {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultUI("Hello world",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false));

        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(triggerScript)
                        .build();
        AutofillAssistantTestServiceRequestSender testServiceRequestSender =
                setupTriggerScripts(triggerScripts);
        startAutofillAssistantOnTab(TEST_PAGE_A);
        waitUntilViewMatchesCondition(withText("Hello world"), isCompletelyDisplayed());

        onView(withId(R.id.tab_switcher_button)).perform(click());
        waitUntilViewAssertionTrue(
                withText("Hello world"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        // Upon returning to the original tab, trigger scripts will be fetched again and restarted.
        testServiceRequestSender.setNextResponse(/* httpStatus = */ 200, triggerScripts);
        Espresso.pressBack();
        waitUntilViewMatchesCondition(withText("Hello world"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT_DISABLE_ONBOARDING_FLOW,
            ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP})
    public void
    testScrollToHide() throws Exception {
        GetTriggerScriptsResponseProto triggerScripts =
                GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(
                                TriggerScriptProto
                                        .newBuilder()
                                        /* no trigger condition */
                                        .setUserInterface(createDefaultUI("Trigger script",
                                                /* bubbleMessage = */ "",
                                                /* withProgressBar = */ false)
                                                                  .setScrollToHide(true)))
                        .build();

        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        BottomSheetController bottomSheetController =
                AutofillAssistantUiTestUtil.getBottomSheetController(mTestRule.getActivity());

        CallbackHelper waitForScroll = new CallbackHelper();
        WebContents webContents = mTestRule.getWebContents();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            GestureListenerManager.fromWebContents(webContents)
                    .addListener(new GestureStateListener() {
                        @Override
                        public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                            waitForScroll.notifyCalled();
                        }
                    });
        });
        int webContentX = TestThreadUtils.runOnUiThreadBlocking(
                () -> webContents.getViewAndroidDelegate().getContainerView().getWidth() / 2);
        int webContentY = TestThreadUtils.runOnUiThreadBlocking(
                () -> webContents.getViewAndroidDelegate().getContainerView().getHeight() / 3);

        int offsetBeforeScroll = TestThreadUtils.runOnUiThreadBlocking(
                () -> bottomSheetController.getCurrentOffset());
        assertThat(offsetBeforeScroll, greaterThan(0));

        // Scroll more than the bottom sheet height, to be sure it's going to be completely hidden
        // or shown due to the scroll.
        int scrollDistance = (int) (bottomSheetController.getCurrentOffset() * 1.5f);
        TouchCommon.performDrag(mTestRule.getActivity(), webContentX, webContentX, webContentY,
                webContentY - scrollDistance,
                /* stepCount*/ 10, /* duration in ms */ 250);
        waitForScroll.waitForCallback("scroll down expected", /* currentCallCount= */ 0);

        // After scroll down, the bottom sheet is completely hidden.
        int offsetAfterScrollDown = TestThreadUtils.runOnUiThreadBlocking(
                () -> bottomSheetController.getCurrentOffset());
        Assert.assertEquals(0, offsetAfterScrollDown);

        TouchCommon.performDrag(mTestRule.getActivity(), webContentX, webContentX, webContentY,
                webContentY + scrollDistance, /* stepCount*/ 10, /* duration in ms */ 250);

        waitForScroll.waitForCallback("scroll up expected", /* currentCallCount= */ 1);

        // Wait until the bottom sheet is fully back on the screen again before capturing one last
        // offset.
        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        int offsetAfterScrollUp = TestThreadUtils.runOnUiThreadBlocking(
                () -> bottomSheetController.getCurrentOffset());

        // After scroll up, the bottom sheet is visible again.
        Assert.assertEquals(offsetBeforeScroll, offsetAfterScrollUp);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP,
            "AutofillAssistantDialogOnboarding"})
    @DisableIf.Build(message = "Flaky on Android P, see https://crbug.com/1154682",
            sdk_is_greater_than = VERSION_CODES.O_MR1, sdk_is_less_than = VERSION_CODES.Q)
    public void
    triggerScriptsPersistsForDialogOnboarding() throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true)
                                                  .setRegularScriptLoadingStatusMessage(
                                                          "Loading regular script"));

        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(triggerScript)
                        .build();
        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Done"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        setupRegularScripts(script);

        onView(withText("Continue")).perform(click());
        // Wait for onboarding.
        waitUntilViewMatchesCondition(withId(R.id.button_init_not_ok), isCompletelyDisplayed());
        onView(withId(R.id.button_init_not_ok))
                .check(matches(withContentDescription(R.string.cancel)));
        onView(withId(R.id.button_init_ok))
                .check(matches(withContentDescription(R.string.init_ok)));
        // Cancel onboarding.
        onView(withId(R.id.button_init_not_ok)).perform(click());
        onView(withText("Continue")).perform(click());
        // Wait for onboarding.
        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), isCompletelyDisplayed());
        // Accept onboarding.
        onView(withId(R.id.button_init_ok)).perform(click());
        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        onView(withText("Loading regular script")).check(matches(isDisplayed()));
        Assert.assertFalse(AutofillAssistantPreferencesUtil.getShowOnboarding());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)
    @Features.DisableFeatures("AutofillAssistantDialogOnboarding")
    @DisableIf.Build(message = "Flaky on Android P, see https://crbug.com/1154682",
            sdk_is_greater_than = VERSION_CODES.O_MR1, sdk_is_less_than = VERSION_CODES.Q)
    public void
    triggerScriptsDoesNotPersistsAfterCancellingBottomSheetOnboarding() throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true)
                                                  .setRegularScriptLoadingStatusMessage(
                                                          "Loading regular script"));

        GetTriggerScriptsResponseProto triggerScripts =
                (GetTriggerScriptsResponseProto) GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(triggerScript)
                        .build();
        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Done"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);
        setupRegularScripts(script);

        onView(withText("Continue")).perform(click());
        // Wait for onboarding.
        waitUntilViewMatchesCondition(withId(R.id.button_init_not_ok), isCompletelyDisplayed());
        // Cancel onboarding.
        onView(withId(R.id.button_init_not_ok)).perform(click());
        waitUntilViewAssertionTrue(withText("Continue"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertTrue(AutofillAssistantPreferencesUtil.getShowOnboarding());
    }
}