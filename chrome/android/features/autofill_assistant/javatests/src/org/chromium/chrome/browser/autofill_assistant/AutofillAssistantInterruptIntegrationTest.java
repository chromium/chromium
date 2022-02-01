// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayingAtLeast;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementExists;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addClickSteps;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantTestService.ScriptsReturnMode;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.BooleanNotProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CallbackProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ComputeValueProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureUiStateProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureUiStateProto.OverlayBehavior;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto.Rectangle;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.EndActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.EventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.GenericUserInterfaceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InteractionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InteractionsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ModelProto;
import org.chromium.chrome.browser.autofill_assistant.proto.OnModelValueChangedEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.OnUserActionCalled;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionStatusProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ScriptPreconditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetUserActionsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowCastProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowGenericUiProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowGenericUiProto.PeriodicElementChecks;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowGenericUiProto.PeriodicElementChecks.ElementCheck;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UserActionList;
import org.chromium.chrome.browser.autofill_assistant.proto.UserActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ValueProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ValueReferenceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ViewProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests autofill assistant's interrupts.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantInterruptIntegrationTest {
    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain =
            RuleChain.outerRule(mTestRule).around(new AutofillAssistantCustomTabTestRule(
                    mTestRule, "autofill_assistant_target_website.html"));

    private static final String MAIN_SCRIPT_PATH = "main_script";
    private static final String INTERRUPT_SCRIPT_PATH = "interrupt_script";

    private AutofillAssistantCollectUserDataTestHelper mHelper;

    @Before
    public void setUp() throws Exception {
        mHelper = new AutofillAssistantCollectUserDataTestHelper();
    }

    @Test
    @MediumTest
    public void testInterruptClicksElementDuringPrompt() throws Exception {
        ArrayList<AutofillAssistantTestScript> scripts = new ArrayList<>();
        SelectorProto touch_area_one = toCssSelector("#touch_area_one");
        SelectorProto touch_area_four = toCssSelector("#touch_area_four");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setConfigureUiState(ConfigureUiStateProto.newBuilder().setOverlayBehavior(
                                 OverlayBehavior.HIDDEN))
                         .build());

        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setAllowInterrupt(true).addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Prompt"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(MAIN_SCRIPT_PATH)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        scripts.add(script);

        ArrayList<ActionProto> interruptActionList = new ArrayList<>();
        addClickSteps(touch_area_one, interruptActionList);
        interruptActionList.add(
                ActionProto.newBuilder()
                        .setPrompt(PromptProto.newBuilder().addChoices(
                                PromptProto.Choice.newBuilder().setChip(
                                        ChipProto.newBuilder().setText("Interrupt"))))
                        .build());

        // The interrupt triggers when touch_area_one is present but touch_area_four is gone, so
        // that we can trigger it manually.
        ScriptPreconditionProto interruptPrecondition =
                ScriptPreconditionProto.newBuilder()
                        .setElementCondition(ElementConditionProto.newBuilder().setAllOf(
                                ElementConditionsProto.newBuilder()
                                        .addConditions(ElementConditionProto.newBuilder().setNoneOf(
                                                ElementConditionsProto.newBuilder().addConditions(
                                                        ElementConditionProto.newBuilder().setMatch(
                                                                touch_area_four))))
                                        .addConditions(ElementConditionProto.newBuilder().setMatch(
                                                touch_area_one))))
                        .build();

        AutofillAssistantTestScript interruptScript = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(INTERRUPT_SCRIPT_PATH)
                        .setPresentation(
                                PresentationProto.newBuilder().setInterrupt(true).setPrecondition(
                                        interruptPrecondition))
                        .build(),
                interruptActionList);
        scripts.add(interruptScript);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(scripts, ScriptsReturnMode.ALL_AT_ONCE);
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Prompt"), isDisplayingAtLeast(90));

        // Tapping touch_area_four will make it disappear, which triggers the interrupt.
        tapElement(mTestRule, "touch_area_four");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_four"));

        // The interrupt should click on touch_area_one, making it disappear.
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));

        // The main action chip should disappear during the interrupt.
        waitUntilViewAssertionTrue(withText("Prompt"), doesNotExist(), DEFAULT_POLLING_INTERVAL);

        // Click the chip to end the interrupt and go back to the main script.
        waitUntilViewMatchesCondition(withText("Interrupt"), isDisplayingAtLeast(90));
        onView(withText("Interrupt")).perform(click());

        // Once the interrupt is done, the prompt chip should appear again.
        waitUntilViewMatchesCondition(withText("Prompt"), isDisplayingAtLeast(90));
    }

    @Test
    @MediumTest
    public void testInterruptCicksElementDuringShowGenericUi() throws Exception {
        ArrayList<AutofillAssistantTestScript> scripts = new ArrayList<>();
        SelectorProto touch_area_one = toCssSelector("#touch_area_one");
        SelectorProto touch_area_four = toCssSelector("#touch_area_four");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setConfigureUiState(ConfigureUiStateProto.newBuilder().setOverlayBehavior(
                                 OverlayBehavior.HIDDEN))
                         .build());

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        interactions.add(InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnUserActionCalled(
                                         OnUserActionCalled.newBuilder().setUserActionIdentifier(
                                                 "done_chip")))
                                 .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                         EndActionProto.newBuilder().setStatus(
                                                 ProcessedActionStatusProto.ACTION_APPLIED)))
                                 .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add(
                ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Done")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("done_chip"))))
                        .build());

        GenericUserInterfaceProto genericUserInterface =
                GenericUserInterfaceProto.newBuilder()
                        .setRootView(
                                ViewProto.newBuilder()
                                        .setTextView(TextViewProto.newBuilder().setText("Text"))
                                        .setIdentifier("textView"))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();
        list.add(ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder()
                                                   .setAllowInterrupt(true)
                                                   .setGenericUserInterface(genericUserInterface))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(MAIN_SCRIPT_PATH)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        scripts.add(script);

        ArrayList<ActionProto> interruptActionList = new ArrayList<>();
        addClickSteps(touch_area_one, interruptActionList);
        interruptActionList.add(
                ActionProto.newBuilder()
                        .setPrompt(PromptProto.newBuilder().addChoices(
                                PromptProto.Choice.newBuilder().setChip(
                                        ChipProto.newBuilder().setText("Interrupt"))))
                        .build());

        // The interrupt triggers when touch_area_one is present but touch_area_four is gone, so
        // that we can trigger it manually.
        ScriptPreconditionProto precondition =
                ScriptPreconditionProto.newBuilder()
                        .setElementCondition(ElementConditionProto.newBuilder().setAllOf(
                                ElementConditionsProto.newBuilder()
                                        .addConditions(ElementConditionProto.newBuilder().setNoneOf(
                                                ElementConditionsProto.newBuilder().addConditions(
                                                        ElementConditionProto.newBuilder().setMatch(
                                                                touch_area_four))))
                                        .addConditions(ElementConditionProto.newBuilder().setMatch(
                                                touch_area_one))))
                        .build();

        AutofillAssistantTestScript interruptScript = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(INTERRUPT_SCRIPT_PATH)
                        .setPresentation(
                                PresentationProto.newBuilder().setInterrupt(true).setPrecondition(
                                        precondition))
                        .build(),
                interruptActionList);
        scripts.add(interruptScript);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(scripts, ScriptsReturnMode.ALL_AT_ONCE);
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Done"), isDisplayingAtLeast(90));

        // Tapping touch_area_four will make it disappear, which triggers the interrupt.
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_four"), is(true));
        tapElement(mTestRule, "touch_area_four");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_four"));

        // The interrupt should click on touch_area_one, making it disappear.
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));

        // The main action chip should disappear during the interrupt.
        waitUntilViewAssertionTrue(withText("Done"), doesNotExist(), DEFAULT_POLLING_INTERVAL);

        // Click the chip to end the interrupt and go back to the main script.
        waitUntilViewMatchesCondition(withText("Interrupt"), isDisplayingAtLeast(90));
        onView(withText("Interrupt")).perform(click());

        // Once the interrupt is done, the prompt chip should appear again.
        waitUntilViewMatchesCondition(withText("Done"), isDisplayingAtLeast(90));
    }

    @Test
    @MediumTest
    public void testInterruptClearsUi() throws Exception {
        ArrayList<AutofillAssistantTestScript> scripts = new ArrayList<>();
        SelectorProto touch_area_one = toCssSelector("#touch_area_one");
        SelectorProto touch_area_four = toCssSelector("#touch_area_four");

        // Main script
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setConfigureUiState(ConfigureUiStateProto.newBuilder().setOverlayBehavior(
                                 OverlayBehavior.HIDDEN))
                         .build());

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        interactions.add(InteractionProto.newBuilder()
                                 .addTriggerEvent(EventProto.newBuilder().setOnUserActionCalled(
                                         OnUserActionCalled.newBuilder().setUserActionIdentifier(
                                                 "done_chip")))
                                 .addCallbacks(CallbackProto.newBuilder().setEndAction(
                                         EndActionProto.newBuilder().setStatus(
                                                 ProcessedActionStatusProto.ACTION_APPLIED)))
                                 .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add(
                (ModelProto.ModelValue) ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Done")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("done_chip"))))
                        .build());

        GenericUserInterfaceProto genericUserInterface =
                GenericUserInterfaceProto.newBuilder()
                        .setRootView(
                                ViewProto.newBuilder()
                                        .setTextView(TextViewProto.newBuilder().setText("Text"))
                                        .setIdentifier("textView"))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();
        list.add(ActionProto.newBuilder()
                         .setShowGenericUi(ShowGenericUiProto.newBuilder()
                                                   .setAllowInterrupt(true)
                                                   .setGenericUserInterface(genericUserInterface))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("End"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(MAIN_SCRIPT_PATH)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        scripts.add(script);

        // Interrupt script
        ArrayList<ActionProto> interruptActionList = new ArrayList<>();
        interruptActionList.add(
                ActionProto.newBuilder()
                        .setPrompt(PromptProto.newBuilder().addChoices(
                                PromptProto.Choice.newBuilder().setChip(
                                        ChipProto.newBuilder().setText("Interrupt"))))
                        .build());

        // The interrupt triggers when touch_area_one is present but touch_area_four is gone, so
        // that we can trigger it manually.
        ScriptPreconditionProto precondition =
                ScriptPreconditionProto.newBuilder()
                        .setElementCondition(ElementConditionProto.newBuilder().setAllOf(
                                ElementConditionsProto.newBuilder()
                                        .addConditions(ElementConditionProto.newBuilder().setNoneOf(
                                                ElementConditionsProto.newBuilder().addConditions(
                                                        ElementConditionProto.newBuilder().setMatch(
                                                                touch_area_four))))
                                        .addConditions(ElementConditionProto.newBuilder().setMatch(
                                                touch_area_one))))
                        .build();

        AutofillAssistantTestScript interruptScript = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(INTERRUPT_SCRIPT_PATH)
                        .setPresentation(
                                PresentationProto.newBuilder().setInterrupt(true).setPrecondition(
                                        precondition))
                        .build(),
                interruptActionList);
        scripts.add(interruptScript);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(scripts, ScriptsReturnMode.ALL_AT_ONCE);
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Done"), isDisplayingAtLeast(90));
        onView(withText("Text")).check(matches(isDisplayed()));

        // Tapping touch_area_four will make it disappear, which triggers the interrupt.
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_four"), is(true));
        tapElement(mTestRule, "touch_area_four");

        // The interrupt prompt appears.
        waitUntilViewMatchesCondition(withText("Interrupt"), isDisplayingAtLeast(90));
        // The UI should be gone at this point.
        onView(withText("Text")).check(doesNotExist());

        // Hide element one so that the interrupt does not trigger again right away after it
        // finishes.
        tapElement(mTestRule, "touch_area_one");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));

        // End interrupt
        onView(withText("Interrupt")).perform(click());

        // Once the interrupt is done, the chip and the UI should appear again.
        waitUntilViewMatchesCondition(withText("Done"), isDisplayingAtLeast(90));
        onView(withText("Text")).check(matches(isDisplayed()));

        // Clicking "Done" should end the action.
        onView(withText("Done")).perform(click());
        waitUntilViewMatchesCondition(withText("End"), isDisplayingAtLeast(90));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1272997")
    public void testStateRestoredAfterInterrupt() throws Exception {
        ArrayList<AutofillAssistantTestScript> scripts = new ArrayList<>();
        SelectorProto touch_area_one = toCssSelector("#touch_area_one");
        SelectorProto touch_area_four = toCssSelector("#touch_area_four");
        SelectorProto touch_area_three = toCssSelector("#touch_area_three");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setShowCast(
                                 ShowCastProto.newBuilder()
                                         .setElementToPresent(touch_area_one)
                                         .setTouchableElementArea(
                                                 ElementAreaProto.newBuilder().addTouchable(
                                                         Rectangle.newBuilder()
                                                                 .addElements(touch_area_one)
                                                                 .addElements(touch_area_four)
                                                                 .addElements(touch_area_three))))
                         .build());

        List<InteractionProto> interactions = new ArrayList<>();
        interactions.add(
                InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "chips")))
                        .addCallbacks(CallbackProto.newBuilder().setSetUserActions(
                                SetUserActionsProto.newBuilder().setUserActions(
                                        ValueReferenceProto.newBuilder().setModelIdentifier(
                                                "chips"))))
                        .build());
        // When element three disappears, the ShowGenericUiAction ends.
        CallbackProto notCallback =
                CallbackProto.newBuilder()
                        .setComputeValue(
                                ComputeValueProto.newBuilder()
                                        .setBooleanNot(BooleanNotProto.newBuilder().setValue(
                                                ValueReferenceProto.newBuilder().setModelIdentifier(
                                                        "touch_area_three_present")))
                                        .setResultModelIdentifier("end_action"))
                        .build();
        interactions.add(
                InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "touch_area_three_present")))
                        .addCallbacks(notCallback)
                        .build());
        interactions.add(
                InteractionProto.newBuilder()
                        .addTriggerEvent(EventProto.newBuilder().setOnValueChanged(
                                OnModelValueChangedEventProto.newBuilder().setModelIdentifier(
                                        "end_action")))
                        .addCallbacks(CallbackProto.newBuilder()
                                              .setEndAction(EndActionProto.newBuilder().setStatus(
                                                      ProcessedActionStatusProto.ACTION_APPLIED))
                                              .setConditionModelIdentifier("end_action"))
                        .build());

        List<ModelProto.ModelValue> modelValues = new ArrayList<>();
        modelValues.add(
                ModelProto.ModelValue.newBuilder()
                        .setIdentifier("chips")
                        .setValue(ValueProto.newBuilder().setUserActions(
                                UserActionList.newBuilder().addValues(
                                        UserActionProto.newBuilder()
                                                .setChip(ChipProto.newBuilder()
                                                                 .setText("Done")
                                                                 .setType(ChipType.NORMAL_ACTION))
                                                .setIdentifier("done_chip"))))
                        .build());

        ElementCheck touch_area_three_present =
                ElementCheck.newBuilder()
                        .setModelIdentifier("touch_area_three_present")
                        .setElementCondition(
                                ElementConditionProto.newBuilder().setMatch(touch_area_three))
                        .build();

        GenericUserInterfaceProto genericUserInterface =
                GenericUserInterfaceProto.newBuilder()
                        .setRootView(
                                ViewProto.newBuilder()
                                        .setTextView(TextViewProto.newBuilder().setText("Text"))
                                        .setIdentifier("textView"))
                        .setInteractions(
                                InteractionsProto.newBuilder().addAllInteractions(interactions))
                        .setModel(ModelProto.newBuilder().addAllValues(modelValues))
                        .build();
        list.add(
                ActionProto.newBuilder()
                        .setShowGenericUi(
                                ShowGenericUiProto.newBuilder()
                                        .setAllowInterrupt(true)
                                        .setGenericUserInterface(genericUserInterface)
                                        .setPeriodicElementChecks(
                                                PeriodicElementChecks.newBuilder().addElementChecks(
                                                        touch_area_three_present)))
                        .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("End"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(MAIN_SCRIPT_PATH)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        scripts.add(script);

        ArrayList<ActionProto> interruptActionList = new ArrayList<>();
        addClickSteps(touch_area_one, interruptActionList);

        // The interrupt triggers when touch_area_one is present but touch_area_four is gone, so
        // that we can trigger it manually.
        ScriptPreconditionProto interruptPrecondition =
                ScriptPreconditionProto.newBuilder()
                        .setElementCondition(ElementConditionProto.newBuilder().setAllOf(
                                ElementConditionsProto.newBuilder()
                                        .addConditions(ElementConditionProto.newBuilder().setNoneOf(
                                                ElementConditionsProto.newBuilder().addConditions(
                                                        ElementConditionProto.newBuilder().setMatch(
                                                                touch_area_four))))
                                        .addConditions(ElementConditionProto.newBuilder().setMatch(
                                                touch_area_one))))
                        .build();

        AutofillAssistantTestScript interruptScript = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(INTERRUPT_SCRIPT_PATH)
                        .setPresentation(
                                PresentationProto.newBuilder().setInterrupt(true).setPrecondition(
                                        interruptPrecondition))
                        .build(),
                interruptActionList);
        scripts.add(interruptScript);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(scripts, ScriptsReturnMode.ALL_AT_ONCE);
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Text"), isDisplayingAtLeast(90));

        // Tapping touch_area_four will make it disappear, which triggers the interrupt.
        tapElement(mTestRule, "touch_area_four");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_four"));

        // The interrupt should click on touch_area_one, making it disappear.
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));

        // Once the interrupt is done, the UI should appear again.
        waitUntilViewMatchesCondition(withText("Text"), isDisplayingAtLeast(90));

        // If the state has been correctly set to PROMPT again, we should have the touchable window
        // in the overlay again. In that case, tapping this element will end the action.
        tapElement(mTestRule, "touch_area_three");
        waitUntilViewMatchesCondition(withText("End"), isDisplayingAtLeast(90));
    }
}
