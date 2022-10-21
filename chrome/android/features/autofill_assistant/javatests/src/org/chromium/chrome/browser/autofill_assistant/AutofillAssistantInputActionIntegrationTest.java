// Copyright 2020 The Chromium Authors
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
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getElementChecked;
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
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toIFrameCssSelector;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
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
@Batch(Batch.PER_CLASS)
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

        addSetValueSteps(toCssSelector("#input1"), "new value", list);
        list.add(createWaitForValuePrompt("#input1", "new value", "script done"));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));
        runScript(script);

        waitUntilViewMatchesCondition(withText("script done"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("new value"));
    }

    @Test
    @MediumTest
    public void fillFormFieldWithKeystrokes() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        addKeyboardSteps(toCssSelector("#input1"), "new value", list);
        list.add(createWaitForValuePrompt("#input1", "new value", "script done"));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));
        runScript(script);

        waitUntilViewMatchesCondition(withText("script done"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("new value"));
    }

    @Test
    @MediumTest
    public void fillFormFieldWithKeystrokesAndFocus() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        addKeyboardWithFocusSteps(toCssSelector("#input1"), "new value", list);
        list.add(createWaitForValuePrompt("#input1", "new value", "script done"));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));
        runScript(script);

        waitUntilViewMatchesCondition(withText("script done"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("new value"));
    }

    @Test
    @MediumTest
    public void fillFormFieldWithKeystrokesAndSelect() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        addKeyboardWithSelectSteps(toCssSelector("#input1"), "new value", list);
        list.add(createWaitForValuePrompt("#input1", "new value", "script done"));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));
        runScript(script);

        waitUntilViewMatchesCondition(withText("script done"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("new value"));
    }

    @Test
    @MediumTest
    public void clearFormFieldWithValue() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        addSetValueSteps(toCssSelector("#input1"), "", list);
        list.add(createWaitForValuePrompt("#input1", "", "#input1 cleared"));

        addKeyboardSteps(toCssSelector("#input2"), "", list);
        list.add(createWaitForValuePrompt("#input2", "", "#input2 cleared"));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is("helloworld2"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("#input1 cleared"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is(""));
        onView(withText("#input1 cleared")).perform(click());

        waitUntilViewMatchesCondition(withText("#input2 cleared"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is(""));
    }

    @Test
    @MediumTest
    public void clearFormFieldWithKeystrokes() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        addKeyboardWithSelectSteps(toCssSelector("#input1"), "", list);
        list.add(createWaitForValuePrompt("#input1", "", "script done"));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));
        runScript(script);

        waitUntilViewMatchesCondition(withText("script done"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is(""));
    }

    @Test
    @MediumTest
    public void selectOptionFromDropdown() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto element = toCssSelector("#select");
        // Select 'one' by value match.
        list.add(ActionProto.newBuilder()
                         .setSelectOption(
                                 SelectOptionProto.newBuilder()
                                         .setElement(element)
                                         .setTextFilterValue(TextFilter.newBuilder().setRe2("one"))
                                         .setOptionComparisonAttribute(
                                                 SelectOptionProto.OptionComparisonAttribute.VALUE))
                         .build());
        list.add(createWaitForValuePrompt("#select", "one", "#select is 'one'"));
        // Select 'three' by label match.
        list.add(
                ActionProto.newBuilder()
                        .setSelectOption(
                                SelectOptionProto.newBuilder()
                                        .setElement(element)
                                        .setTextFilterValue(TextFilter.newBuilder().setRe2("Three"))
                                        .setOptionComparisonAttribute(
                                                SelectOptionProto.OptionComparisonAttribute.LABEL))
                        .build());
        list.add(createWaitForValuePrompt("#select", "three", "#select is 'three'"));
        // Select option 'two' by label matching the option starting with "Zürich".
        list.add(ActionProto.newBuilder()
                         .setSelectOption(
                                 SelectOptionProto.newBuilder()
                                         .setElement(element)
                                         .setTextFilterValue(
                                                 TextFilter.newBuilder().setRe2("^Zürich"))
                                         .setOptionComparisonAttribute(
                                                 SelectOptionProto.OptionComparisonAttribute.LABEL))
                         .build());
        list.add(createWaitForValuePrompt("#select", "two", "#select is 'two'"));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        runScript(script);

        waitUntilViewMatchesCondition(withText("#select is 'one'"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("one"));
        onView(withText("#select is 'one'")).perform(click());

        waitUntilViewMatchesCondition(withText("#select is 'three'"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("three"));
        onView(withText("#select is 'three'")).perform(click());

        waitUntilViewMatchesCondition(withText("#select is 'two'"), isCompletelyDisplayed());
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

        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_one"), is(true));
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_five"), is(true));
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_six"), is(true));

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
        assertThat(checkElementExists(mTestRule.getWebContents(), "button"), is(true));
        assertThat(checkElementExists(mTestRule.getWebContents(), "overlay"), is(true));
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
        list.add(createWaitForValuePrompt("#select", "three", "script done"));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("one"));
        runScript(script);

        waitUntilViewMatchesCondition(withText("script done"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("three"));
    }

    @Test
    @MediumTest
    public void fillTextFieldWithNativeMethod() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto inputInIFrame = toIFrameCssSelector("#iframe", "#input");
        SelectorProto input = toCssSelector("#input2");

        MiniActionTestUtil.addSetNativeValueSteps(inputInIFrame, "Value 2", list);
        MiniActionTestUtil.addSetNativeValueSteps(input, "Value 1", list);
        list.add(createWaitForValuePrompt("#input2", "Value 1", "script done"));

        assertThat(getElementValue(mTestRule.getWebContents(), "iframe", "input"), is(""));
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is("helloworld2"));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        runScript(script);

        waitUntilViewMatchesCondition(withText("script done"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "iframe", "input"), is("Value 2"));
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is("Value 1"));
    }

    @Test
    @MediumTest
    public void fillTextareaWithNativeMethod() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        SelectorProto selector = toCssSelector("#textarea1");

        MiniActionTestUtil.addSetNativeValueSteps(selector, "new value", list);
        list.add(createWaitForValuePrompt("#textarea1", "new value", "script done"));

        assertThat(getElementValue(mTestRule.getWebContents(), "textarea1"),
                is("Initial textarea value."));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        runScript(script);

        waitUntilViewMatchesCondition(withText("script done"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "textarea1"), is("new value"));
    }

    @Test
    @MediumTest
    public void fillDropdownWithNativeMethod() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto selector = toCssSelector("#select");
        MiniActionTestUtil.addSetNativeValueSteps(selector, "three", list);
        list.add(createWaitForValuePrompt("#select", "three", "script done"));

        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("one"));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        runScript(script);

        waitUntilViewMatchesCondition(withText("script done"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("three"));
    }

    @Test
    @MediumTest
    public void fillCheckboxWithNativeMethod() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto selectorOption2 = toCssSelector("#option2");
        SelectorProto selectorOption3 = toCssSelector("#option3");

        MiniActionTestUtil.addSetNativeCheckedSteps(selectorOption2, true, list);
        MiniActionTestUtil.addSetNativeCheckedSteps(selectorOption3, false, list);
        list.add(createWaitForSelectorPrompt(
                SelectorProto.newBuilder()
                        .addFilters(SelectorProto.Filter.newBuilder().setCssSelector(
                                "#option3:not(:checked)"))
                        .build(),
                "script done"));

        assertThat(getElementChecked(mTestRule.getWebContents(), "option2"), is(false));
        assertThat(getElementChecked(mTestRule.getWebContents(), "option3"), is(true));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        runScript(script);
        waitUntilViewMatchesCondition(withText("script done"), isCompletelyDisplayed());

        assertThat(getElementChecked(mTestRule.getWebContents(), "option2"), is(true));
        assertThat(getElementChecked(mTestRule.getWebContents(), "option3"), is(false));
    }

    @Test
    @MediumTest
    public void fillRadioButtonWithNativeMethod() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto selectorRed = toCssSelector("#radio_red");

        MiniActionTestUtil.addSetNativeCheckedSteps(selectorRed, true, list);
        list.add(createWaitForSelectorPrompt(
                SelectorProto.newBuilder()
                        .addFilters(SelectorProto.Filter.newBuilder().setCssSelector(
                                "#radio_red:checked"))
                        .build(),
                "script done"));

        assertThat(getElementChecked(mTestRule.getWebContents(), "radio_red"), is(false));
        assertThat(getElementChecked(mTestRule.getWebContents(), "radio_blue"), is(false));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        runScript(script);
        waitUntilViewMatchesCondition(withText("script done"), isCompletelyDisplayed());

        assertThat(getElementChecked(mTestRule.getWebContents(), "radio_red"), is(true));
        assertThat(getElementChecked(mTestRule.getWebContents(), "radio_blue"), is(false));
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

    /**
     * Creates a Prompt action that automatically displays a chip for {@code
     * textOfChipToShowOnSuccess} once {@code selector} matches the current DOM.
     */
    private ActionProto createWaitForSelectorPrompt(
            SelectorProto selector, String textOfChipToShowOnSuccess) {
        return ActionProto.newBuilder()
                .setPrompt(PromptProto.newBuilder().addChoices(
                        Choice.newBuilder()
                                .setChip(ChipProto.newBuilder()
                                                 .setType(ChipType.HIGHLIGHTED_ACTION)
                                                 .setText(textOfChipToShowOnSuccess))
                                .setShowOnlyWhen(
                                        ElementConditionProto.newBuilder().setMatch(selector))))
                .build();
    }

    /**
     * Creates a Prompt action that automatically displays a chip for {@code
     * textOfChipToShowOnSuccess} once {@code cssElement} has a value of {@code valueToWaitFor} in
     * the current DOM.
     */
    private ActionProto createWaitForValuePrompt(
            String cssElement, String valueToWaitFor, String textOfChipToShowOnSuccess) {
        return createWaitForSelectorPrompt(
                SelectorProto.newBuilder()
                        .addFilters(SelectorProto.Filter.newBuilder().setCssSelector(cssElement))
                        .addFilters(SelectorProto.Filter.newBuilder().setValue(
                                TextFilter.newBuilder().setRe2(valueToWaitFor)))
                        .build(),
                textOfChipToShowOnSuccess);
    }
}
