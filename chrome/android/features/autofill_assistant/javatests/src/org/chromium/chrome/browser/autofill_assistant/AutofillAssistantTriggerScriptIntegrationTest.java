// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.createDefaultTriggerScriptUI;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistantWithParams;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilKeyboardMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;

import android.os.Build.VERSION_CODES;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.Empty;
import org.chromium.chrome.browser.autofill_assistant.proto.GetTriggerScriptsResponseProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptConditionsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TriggerScriptProto;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.autofill_assistant.AssistantFeatures;
import org.chromium.components.autofill_assistant.AutofillAssistantPreferencesUtil;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/** Integration tests for trigger scripts. */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantTriggerScriptIntegrationTest {
    private static final String TEST_PAGE_A = "autofill_assistant_target_website.html";

    private final ChromeTabbedActivityTestRule mTestRule = new ChromeTabbedActivityTestRule();
    private final AutofillAssistantChromeTabTestRule mTabTestRule =
            new AutofillAssistantChromeTabTestRule(mTestRule, TEST_PAGE_A);

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(mTabTestRule);

    private String getURL(String page) {
        return mTabTestRule.getURL(page);
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
        HashMap<String, Object> parameters = new HashMap(scriptParameters);
        parameters.put("START_IMMEDIATELY", false);

        startAutofillAssistantWithParams(mTestRule.getActivity(), getURL(pageToLoad), parameters);
    }

    @Before
    public void setUp() {
        // Enable MSBB.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                                Profile.getLastUsedRegularProfile(), true));
    }

    @Test
    @MediumTest
    @EnableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME)
    @DisableIf.
    Build(message = "See https://crbug.com/1199849", sdk_is_greater_than = VERSION_CODES.O_MR1)
    @FlakyTest(message = "crbug.com/1199416")
    public void setReturningUserFlag() {
        TriggerScriptProto.Builder firstTimeTriggerScript =
                TriggerScriptProto.newBuilder()
                        .setTriggerCondition(
                                TriggerScriptConditionProto.newBuilder().setIsFirstTimeUser(
                                        Empty.newBuilder()))
                        .setUserInterface(createDefaultTriggerScriptUI("First time user",
                                /* bubbleMessage = */ "First time message",
                                /* withProgressBar = */ true));

        TriggerScriptProto.Builder returningUserTriggerScript =
                TriggerScriptProto.newBuilder()
                        .setTriggerCondition(TriggerScriptConditionProto.newBuilder().setNoneOf(
                                TriggerScriptConditionsProto.newBuilder().addConditions(
                                        TriggerScriptConditionProto.newBuilder().setIsFirstTimeUser(
                                                Empty.newBuilder()))))
                        .setUserInterface(createDefaultTriggerScriptUI("Returning user",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false));
        GetTriggerScriptsResponseProto triggerScripts =
                GetTriggerScriptsResponseProto.newBuilder()
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
    @EnableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME)
    public void setCancelForeverFlag() {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultTriggerScriptUI("Hello world",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false));
        GetTriggerScriptsResponseProto triggerScripts = GetTriggerScriptsResponseProto.newBuilder()
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
        onView(withText("Never show again"))
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        waitUntilViewAssertionTrue(
                withText("Hello world"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertFalse(AutofillAssistantPreferencesUtil.isProactiveHelpOn());
    }

    @Test
    @MediumTest
    @EnableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME)
    // Disable translate to prevent the popup from covering part of the website.
    @DisableFeatures("Translate")
    public void elementCondition() throws Exception {
        SelectorProto touch_area_four = toCssSelector("#touch_area_one");
        TriggerScriptProto.Builder buttonVisibleTriggerScript =
                TriggerScriptProto.newBuilder()
                        .setTriggerCondition(TriggerScriptConditionProto.newBuilder().setSelector(
                                touch_area_four))
                        .setUserInterface(createDefaultTriggerScriptUI("Area visible",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true));

        TriggerScriptProto.Builder buttonInvisibleTriggerScript =
                TriggerScriptProto.newBuilder()
                        .setTriggerCondition(TriggerScriptConditionProto.newBuilder().setNoneOf(
                                TriggerScriptConditionsProto.newBuilder().addConditions(
                                        TriggerScriptConditionProto.newBuilder().setSelector(
                                                touch_area_four))))
                        .setUserInterface(createDefaultTriggerScriptUI("Area invisible",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false));
        GetTriggerScriptsResponseProto triggerScripts =
                GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(buttonVisibleTriggerScript)
                        .addTriggerScripts(buttonInvisibleTriggerScript)
                        .build();
        setupTriggerScripts(triggerScripts);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Area visible"), isCompletelyDisplayed());
        onView(withId(R.id.step_progress_bar)).check(matches(isDisplayed()));

        tapElement(mTestRule, "touch_area_one");
        waitUntilViewMatchesCondition(withText("Area invisible"), isCompletelyDisplayed());
        onView(withId(R.id.step_progress_bar)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @EnableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME)
    @DisableIf.Build(message = "Flaky on Android P, see https://crbug.com/1154682",
            sdk_is_greater_than = VERSION_CODES.O_MR1, sdk_is_less_than = VERSION_CODES.Q)
    public void
    transitionToOnboardingAndRegularScript() throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultTriggerScriptUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true)
                                                  .setRegularScriptLoadingStatusMessage(
                                                          "Loading regular script"));

        GetTriggerScriptsResponseProto triggerScripts = GetTriggerScriptsResponseProto.newBuilder()
                                                                .addTriggerScripts(triggerScript)
                                                                .build();
        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        AutofillAssistantPreferencesUtil.setOnboardingAcceptedPreference(false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Done"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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
        onView(withId(R.id.step_progress_bar)).check(matches(isDisplayed()));
        Assert.assertFalse(AutofillAssistantPreferencesUtil.getShowOnboarding());
    }

    @Test
    @MediumTest
    @EnableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME)
    @DisableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_DISABLE_ONBOARDING_FLOW_NAME)
    public void transitionToRegularScriptWithoutOnboarding() throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultTriggerScriptUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false)
                                                  .setRegularScriptLoadingStatusMessage(
                                                          "Loading regular script"));

        GetTriggerScriptsResponseProto triggerScripts = GetTriggerScriptsResponseProto.newBuilder()
                                                                .addTriggerScripts(triggerScript)
                                                                .build();
        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        AutofillAssistantPreferencesUtil.setOnboardingAcceptedPreference(true);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Done"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        setupRegularScripts(script);

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        onView(withText("Loading regular script")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME)
    public void dontShowOnboardingIfAcceptedInDifferentTab() {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultTriggerScriptUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true)
                                                  .setRegularScriptLoadingStatusMessage(
                                                          "Loading regular script"));

        GetTriggerScriptsResponseProto triggerScripts = GetTriggerScriptsResponseProto.newBuilder()
                                                                .addTriggerScripts(triggerScript)
                                                                .build();
        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        AutofillAssistantPreferencesUtil.setOnboardingAcceptedPreference(false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        // Simulate the user accepting the onboarding in a different tab.
        AutofillAssistantPreferencesUtil.setOnboardingAcceptedPreference(true);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Done"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        setupRegularScripts(script);

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        onView(withText("Loading regular script")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_DISABLE_ONBOARDING_FLOW_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME})
    public void
    transitionToRegularScriptWithoutOnboardingWithDisableOnboardingFlowFeatureOn()
            throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultTriggerScriptUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false)
                                                  .setRegularScriptLoadingStatusMessage(
                                                          "Loading regular script"));
        GetTriggerScriptsResponseProto triggerScripts = GetTriggerScriptsResponseProto.newBuilder()
                                                                .addTriggerScripts(triggerScript)
                                                                .build();

        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Done"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        setupRegularScripts(script);

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        onView(withText("Loading regular script")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME)
    @DisableIf.
    Build(message = "Fails on Lollipop and Marshmallow Tablet Tester, https://crbug.com/1158435",
            sdk_is_less_than = VERSION_CODES.N)
    public void
    switchToNewTabAndThenBack() {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultTriggerScriptUI("Hello world",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false));

        GetTriggerScriptsResponseProto triggerScripts = GetTriggerScriptsResponseProto.newBuilder()
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
    @EnableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_DISABLE_ONBOARDING_FLOW_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME})
    @DisabledTest(message = "https://crbug.com/1232703")
    public void
    testScrollToHide() throws Exception {
        GetTriggerScriptsResponseProto triggerScripts =
                GetTriggerScriptsResponseProto.newBuilder()
                        .addTriggerScripts(
                                TriggerScriptProto
                                        .newBuilder()
                                        /* no trigger condition */
                                        .setUserInterface(
                                                createDefaultTriggerScriptUI("Trigger script",
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
    @EnableFeatures({AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME,
            "AutofillAssistantDialogOnboarding"})
    @DisableIf.Build(message = "Flaky on Android P, see https://crbug.com/1154682",
            sdk_is_greater_than = VERSION_CODES.O_MR1, sdk_is_less_than = VERSION_CODES.Q)
    public void
    triggerScriptsPersistsForDialogOnboarding() throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultTriggerScriptUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true)
                                                  .setRegularScriptLoadingStatusMessage(
                                                          "Loading regular script"));

        GetTriggerScriptsResponseProto triggerScripts = GetTriggerScriptsResponseProto.newBuilder()
                                                                .addTriggerScripts(triggerScript)
                                                                .build();
        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        AutofillAssistantPreferencesUtil.setOnboardingAcceptedPreference(false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Done"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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
    @EnableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME)
    @DisableFeatures("AutofillAssistantDialogOnboarding")
    @DisableIf.Build(message = "Flaky on Android P, see https://crbug.com/1154682",
            sdk_is_greater_than = VERSION_CODES.O_MR1, sdk_is_less_than = VERSION_CODES.Q)
    public void
    triggerScriptsDoesNotPersistsAfterCancellingBottomSheetOnboarding() throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto
                        .newBuilder()
                        /* no trigger condition */
                        .setUserInterface(createDefaultTriggerScriptUI("Trigger script",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ true)
                                                  .setRegularScriptLoadingStatusMessage(
                                                          "Loading regular script"));

        GetTriggerScriptsResponseProto triggerScripts = GetTriggerScriptsResponseProto.newBuilder()
                                                                .addTriggerScripts(triggerScript)
                                                                .build();
        setupTriggerScripts(triggerScripts);
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        AutofillAssistantPreferencesUtil.setOnboardingAcceptedPreference(false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntilViewMatchesCondition(withText("Trigger script"), isCompletelyDisplayed());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Done"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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

    @Test
    @MediumTest
    @EnableFeatures(AssistantFeatures.AUTOFILL_ASSISTANT_PROACTIVE_HELP_NAME)
    public void triggerScriptHidesAndShowsForKeyboard() throws Exception {
        TriggerScriptProto.Builder triggerScript =
                TriggerScriptProto.newBuilder()
                        .setTriggerCondition(
                                TriggerScriptConditionProto.newBuilder().setKeyboardHidden(
                                        Empty.newBuilder()))
                        .setUserInterface(createDefaultTriggerScriptUI("Hello world",
                                /* bubbleMessage = */ "",
                                /* withProgressBar = */ false));

        GetTriggerScriptsResponseProto triggerScripts = GetTriggerScriptsResponseProto.newBuilder()
                                                                .addTriggerScripts(triggerScript)
                                                                .build();
        AutofillAssistantTestServiceRequestSender testServiceRequestSender =
                setupTriggerScripts(triggerScripts);
        startAutofillAssistantOnTab(TEST_PAGE_A);
        waitUntilViewMatchesCondition(withText("Hello world"), isCompletelyDisplayed());

        tapElement(mTestRule, "trigger-keyboard");
        waitUntilKeyboardMatchesCondition(mTestRule, /* isShowing= */ true);
        waitUntilViewAssertionTrue(
                withText("Hello World"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        Espresso.closeSoftKeyboard();
        waitUntilKeyboardMatchesCondition(mTestRule, /* isShowing= */ false);
        waitUntilViewMatchesCondition(withText("Hello world"), isCompletelyDisplayed());
    }
}
