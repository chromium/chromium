// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.hasSize;
import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementExists;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getElementValue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addClickSteps;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addJsClickSteps;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addKeyboardSteps;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addKeyboardWithFocusSteps;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addKeyboardWithSelectSteps;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addSetValueSteps;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addTapSteps;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toClientId;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CheckElementIsOnTopProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientIdProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionStatusProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.ReleaseElementsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ScrollIntoViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectOptionElementProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectOptionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SendChangeEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SendClickEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextFilter;
import org.chromium.chrome.browser.autofill_assistant.proto.WaitForDocumentToBecomeInteractiveProto;
import org.chromium.chrome.browser.autofill_assistant.proto.WaitForDomProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests autofill assistant's input actions such as keyboard and clicking.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantInputActionIntegrationTest {
    private static final String TEST_PAGE = "autofill_assistant_target_website.html";
    private static final SupportedScriptProto TEST_SCRIPT =
            SupportedScriptProto.newBuilder()
                    .setPath(TEST_PAGE)
                    .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                    .build();

    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(
            new AutofillAssistantCustomTabTestRule(mTestRule, TEST_PAGE));

    @Test
    @MediumTest
    public void fillFormFieldWithValue() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        addSetValueSteps(toCssSelector("#input1"), "Value", list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Set value")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Set value"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("Value"));
    }

    @Test
    @MediumTest
    public void fillFormFieldWithKeystrokes() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        addKeyboardSteps(toCssSelector("#input1"), "Value", list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Set value")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Set value"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("Value"));
    }

    @Test
    @MediumTest
    public void fillFormFieldWithKeystrokesAndFocus() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        addKeyboardWithFocusSteps(toCssSelector("#input1"), "Value", list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Set value")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Set value"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("Value"));
    }

    @Test
    @MediumTest
    public void fillFormFieldWithKeystrokesAndSelect() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        addKeyboardWithSelectSteps(toCssSelector("#input1"), "Value", list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Set value")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Set value"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("Value"));
    }

    @Test
    @MediumTest
    public void clearFormFieldWithValue() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        addSetValueSteps(toCssSelector("#input1"), "", list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Clear value")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        addKeyboardSteps(toCssSelector("#input2"), "", list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Clear value Keystrokes")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is("helloworld2"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Clear value"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is(""));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("Clear value Keystrokes"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is(""));
    }

    @Test
    @MediumTest
    public void clearFormFieldWithKeystrokes() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        addKeyboardWithSelectSteps(toCssSelector("#input1"), "", list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Empty value")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Empty value"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is(""));
    }

    @Test
    @MediumTest
    public void selectOptionFromDropdown() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto element = toCssSelector("#select");
        list.add(ActionProto.newBuilder()
                         .setSelectOption(
                                 SelectOptionProto.newBuilder()
                                         .setElement(element)
                                         .setTextFilterValue(TextFilter.newBuilder().setRe2("one"))
                                         .setOptionComparisonAttribute(
                                                 SelectOptionProto.OptionComparisonAttribute.VALUE))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Value Match")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        list.add(
                ActionProto.newBuilder()
                        .setSelectOption(
                                SelectOptionProto.newBuilder()
                                        .setElement(element)
                                        .setTextFilterValue(TextFilter.newBuilder().setRe2("Three"))
                                        .setOptionComparisonAttribute(
                                                SelectOptionProto.OptionComparisonAttribute.LABEL))
                        .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Label Match")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setSelectOption(
                                 SelectOptionProto.newBuilder()
                                         .setElement(element)
                                         .setTextFilterValue(
                                                 TextFilter.newBuilder().setRe2("^Zürich"))
                                         .setOptionComparisonAttribute(
                                                 SelectOptionProto.OptionComparisonAttribute.LABEL))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Label Starts With")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        runScript(script);

        waitUntilViewMatchesCondition(withText("Value Match"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("one"));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("Label Match"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("three"));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("Label Starts With"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("two"));
    }

    @Test
    @MediumTest
    public void clickingOnElementToHide() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        addClickSteps(toCssSelector("#touch_area_one"), list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Click").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder()
                                                 .setType(ChipType.HIGHLIGHTED_ACTION)
                                                 .setText("Continue"))))
                         .build());
        addTapSteps(toCssSelector("#touch_area_five"), list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Tap").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder()
                                                 .setType(ChipType.HIGHLIGHTED_ACTION)
                                                 .setText("Continue"))))
                         .build());
        addJsClickSteps(toCssSelector("#touch_area_six"), list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("JS").addChoices(
                                 Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        checkElementExists(mTestRule.getWebContents(), "touch_area_one");
        checkElementExists(mTestRule.getWebContents(), "touch_area_five");
        checkElementExists(mTestRule.getWebContents(), "touch_area_six");

        runScript(script);

        waitUntilViewMatchesCondition(withText("Click"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("Tap"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_five"));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("JS"), isCompletelyDisplayed());
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_six"));
    }

    @Test
    @MediumTest
    public void clickOnButtonCoveredByOverlay() throws Exception {
        checkElementExists(mTestRule.getWebContents(), "button");
        checkElementExists(mTestRule.getWebContents(), "overlay");
        showOverlay();

        // This script attempts to click 2 times on #button:
        // 1. the first click chain clicks without checking for overlays
        // 2. the second click chain finds an overlay and fails
        SelectorProto button = toCssSelector("#button");
        ClientIdProto clientId = toClientId("e");
        ArrayList<ActionProto> actions = new ArrayList<>();
        actions.add(
                ActionProto.newBuilder()
                        .setWaitForDom(
                                WaitForDomProto.newBuilder().setTimeoutMs(1000).setWaitCondition(
                                        ElementConditionProto.newBuilder()
                                                .setMatch(button)
                                                .setClientId(clientId)))
                        .build());
        actions.add(
                ActionProto.newBuilder()
                        .setScrollIntoView(ScrollIntoViewProto.newBuilder().setClientId(clientId))
                        .build());
        actions.add(
                ActionProto.newBuilder()
                        .setSendClickEvent(SendClickEventProto.newBuilder().setClientId(clientId))
                        .build());
        actions.add(ActionProto.newBuilder()
                            .setCheckElementIsOnTop(
                                    CheckElementIsOnTopProto.newBuilder().setClientId(clientId))
                            .build());
        actions.add(
                ActionProto.newBuilder()
                        .setSendClickEvent(SendClickEventProto.newBuilder().setClientId(clientId))
                        .build());
        actions.add(ActionProto.newBuilder()
                            .setReleaseElements(
                                    ReleaseElementsProto.newBuilder().addClientIds(clientId))
                            .build());

        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(new AutofillAssistantTestScript(TEST_SCRIPT, actions)));
        startAutofillAssistant(mTestRule.getActivity(), testService);
        testService.waitUntilGetNextActions(1);

        List<ProcessedActionProto> processed = testService.getProcessedActions();
        assertThat(processed, hasSize(4));
        assertThat(processed.get(/* WaitForDom */ 0).getStatus(),
                is(ProcessedActionStatusProto.ACTION_APPLIED));
        assertThat(processed.get(/* ScrollIntoView */ 1).getStatus(),
                is(ProcessedActionStatusProto.ACTION_APPLIED));
        assertThat(processed.get(/* SendClickEvent */ 2).getStatus(),
                is(ProcessedActionStatusProto.ACTION_APPLIED));
        assertThat(processed.get(/* CheckOnTop */ 3).getStatus(),
                is(ProcessedActionStatusProto.ELEMENT_NOT_ON_TOP));
        // No SendClickEvent
    }

    @Test
    @MediumTest
    public void selectOptionWithMiniActions() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto select = toCssSelector("#select");
        ClientIdProto selectId = toClientId("s");
        SelectorProto option = toCssSelector("#select option:nth-child(3)");
        ClientIdProto optionId = toClientId("o");
        list.add(ActionProto.newBuilder()
                         .setWaitForDom(
                                 WaitForDomProto.newBuilder().setTimeoutMs(1000).setWaitCondition(
                                         ElementConditionProto.newBuilder()
                                                 .setMatch(select)
                                                 .setClientId(selectId)))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setWaitForDom(
                                 WaitForDomProto.newBuilder().setTimeoutMs(1000).setWaitCondition(
                                         ElementConditionProto.newBuilder()
                                                 .setMatch(option)
                                                 .setClientId(optionId)))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setWaitForDocumentToBecomeInteractive(
                                 WaitForDocumentToBecomeInteractiveProto.newBuilder()
                                         .setClientId(selectId)
                                         .setTimeoutInMs(1000))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setScrollIntoView(ScrollIntoViewProto.newBuilder().setClientId(selectId))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setSelectOptionElement(SelectOptionElementProto.newBuilder()
                                                         .setSelectId(selectId)
                                                         .setOptionId(optionId))
                         .build());
        list.add(
                ActionProto.newBuilder()
                        .setSendChangeEvent(SendChangeEventProto.newBuilder().setClientId(selectId))
                        .build());
        list.add(ActionProto.newBuilder()
                         .setReleaseElements(ReleaseElementsProto.newBuilder()
                                                     .addClientIds(selectId)
                                                     .addClientIds(optionId))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Done").addChoices(
                                 Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("one"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("three"));
    }

    @Test
    @MediumTest
    public void fillTextFieldWithNativeMethod() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto selector = toCssSelector("#input2");

        MiniActionTestUtil.addSetNativeValueSteps(selector, "New Value", list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Set Value")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is("helloworld2"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Set Value"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is("New Value"));
    }

    @Test
    @MediumTest
    public void fillTextareaWithNativeMethod() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        SelectorProto selector = toCssSelector("#textarea1");

        MiniActionTestUtil.addSetNativeValueSteps(selector, "New Value", list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Set Value")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        assertThat(getElementValue(mTestRule.getWebContents(), "textarea1"),
                is("Initial textarea value."));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Set Value"), isCompletelyDisplayed());

        assertThat(getElementValue(mTestRule.getWebContents(), "textarea1"), is("New Value"));
    }

    @Test
    @MediumTest
    public void fillDropdownWithNativeMethod() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto selector = toCssSelector("#select");

        MiniActionTestUtil.addSetNativeValueSteps(selector, "three", list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Set Value")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("one"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Set Value"), isCompletelyDisplayed());

        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("three"));
    }

    private void runScript(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }

    private void showOverlay() throws Exception {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(mTestRule.getWebContents(),
                "document.getElementById('overlay').style.visibility = 'visible'");
        javascriptHelper.waitUntilHasValue();
    }
}
