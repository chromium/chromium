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

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CheckElementIsOnTopProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ClickProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ClickType;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientIdProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.KeyboardValueFillStrategy;
import org.chromium.chrome.browser.autofill_assistant.proto.OptionalStep;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionStatusProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.ReleaseElementsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ScrollIntoViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectOptionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto.Filter;
import org.chromium.chrome.browser.autofill_assistant.proto.SendChangeEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SendClickEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SendKeystrokeEventsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetElementAttributeProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetFormFieldValueProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetFormFieldValueProto.KeyPress;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextFilter;
import org.chromium.chrome.browser.autofill_assistant.proto.TextValue;
import org.chromium.chrome.browser.autofill_assistant.proto.WaitForDocumentToBecomeInteractiveProto;
import org.chromium.chrome.browser.autofill_assistant.proto.WaitForDomProto;
import org.chromium.chrome.browser.autofill_assistant.proto.WaitForElementToBecomeStableProto;
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
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "autofill_assistant_target_website.html";

    private static final SupportedScriptProto TEST_SCRIPT =
            SupportedScriptProto.newBuilder()
                    .setPath(TEST_PAGE)
                    .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                            ChipProto.newBuilder().setText("Done")))
                    .build();

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(
                AutofillAssistantUiTestUtil.createMinimalCustomTabIntentForAutobot(
                        mTestRule.getTestServer().getURL(TEST_PAGE),
                        /* startImmediately = */ true));
    }

    @Test
    @MediumTest
    public void fillFormFieldWithValue() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto element_set_value =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#input1"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element_set_value)
                                         .addValue(KeyPress.newBuilder().setText("Value"))
                                         .setFillStrategy(KeyboardValueFillStrategy.SET_VALUE))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
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

        SelectorProto element_set_value =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#input1"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element_set_value)
                                         .addValue(KeyPress.newBuilder().setText("Value"))
                                         .setFillStrategy(
                                                 KeyboardValueFillStrategy.SIMULATE_KEY_PRESSES))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
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

        SelectorProto element_set_value =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#input1"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element_set_value)
                                         .addValue(KeyPress.newBuilder().setText("Value"))
                                         .setFillStrategy(KeyboardValueFillStrategy
                                                                  .SIMULATE_KEY_PRESSES_FOCUS))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
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
    @DisabledTest(message = "crbug.com/1194363")
    public void fillFormFieldWithKeystrokesAndSelect() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto element_set_value =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#input1"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element_set_value)
                                         .addValue(KeyPress.newBuilder().setText("Value"))
                                         .setFillStrategy(
                                                 KeyboardValueFillStrategy
                                                         .SIMULATE_KEY_PRESSES_SELECT_VALUE))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
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

        SelectorProto element_set_value =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#input1"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element_set_value)
                                         .addValue(KeyPress.newBuilder().setText(""))
                                         .setFillStrategy(KeyboardValueFillStrategy.SET_VALUE))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Clear value")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        SelectorProto element_keystrokes =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#input2"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element_keystrokes)
                                         .addValue(KeyPress.newBuilder().setText(""))
                                         .setFillStrategy(
                                                 KeyboardValueFillStrategy.SIMULATE_KEY_PRESSES))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
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

        SelectorProto element_set_value =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#input1"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element_set_value)
                                         .addValue(KeyPress.newBuilder().setText(""))
                                         .setFillStrategy(
                                                 KeyboardValueFillStrategy
                                                         .SIMULATE_KEY_PRESSES_SELECT_VALUE))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Empty value")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        SelectorProto element_keystrokes =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#input2"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element_keystrokes)
                                         .addValue(KeyPress.newBuilder().setText("\b"))
                                         .setFillStrategy(
                                                 KeyboardValueFillStrategy
                                                         .SIMULATE_KEY_PRESSES_SELECT_VALUE))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Backspace")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is("helloworld2"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Empty value"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is(""));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("Backspace"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is(""));
    }

    @Test
    @MediumTest
    public void selectOptionFromDropdown() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto element = (SelectorProto) SelectorProto.newBuilder()
                                        .addFilters(Filter.newBuilder().setCssSelector("#select"))
                                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSelectOption(
                                 SelectOptionProto.newBuilder()
                                         .setElement(element)
                                         .setTextFilterValue(TextFilter.newBuilder().setRe2("one"))
                                         .setOptionComparisonAttribute(
                                                 SelectOptionProto.OptionComparisonAttribute.VALUE))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Value Match")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setSelectOption(
                                SelectOptionProto.newBuilder()
                                        .setElement(element)
                                        .setTextFilterValue(TextFilter.newBuilder().setRe2("Three"))
                                        .setOptionComparisonAttribute(
                                                SelectOptionProto.OptionComparisonAttribute.LABEL))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Label Match")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSelectOption(
                                 SelectOptionProto.newBuilder()
                                         .setElement(element)
                                         .setTextFilterValue(
                                                 TextFilter.newBuilder().setRe2("^Zürich"))
                                         .setOptionComparisonAttribute(
                                                 SelectOptionProto.OptionComparisonAttribute.LABEL))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
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

        SelectorProto element_click =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#touch_area_one"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setClick(ClickProto.newBuilder()
                                           .setElementToClick(element_click)
                                           .setClickType(ClickType.CLICK))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Click").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder()
                                                 .setType(ChipType.HIGHLIGHTED_ACTION)
                                                 .setText("Continue"))))
                         .build());
        SelectorProto element_tap =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#touch_area_five"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setClick(ClickProto.newBuilder()
                                           .setElementToClick(element_tap)
                                           .setClickType(ClickType.TAP))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Tap").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder()
                                                 .setType(ChipType.HIGHLIGHTED_ACTION)
                                                 .setText("Continue"))))
                         .build());
        SelectorProto element_js =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#touch_area_six"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setClick(ClickProto.newBuilder()
                                           .setElementToClick(element_js)
                                           .setClickType(ClickType.JAVASCRIPT))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
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
    @DisabledTest(message = "crbug.com/1172648")
    public void clickOnButtonCoveredByOverlay() throws Exception {
        checkElementExists(mTestRule.getWebContents(), "button");
        checkElementExists(mTestRule.getWebContents(), "overlay");
        showOverlay();

        // This script attempts to click 3 times on #button:
        // 1. the first click action clicks without checking for overlays
        // 2. the second click action checks, finds an overlay, but clicks anyway
        // 3. the third click action finds an overlay and fails
        SelectorProto.Builder button = SelectorProto.newBuilder().addFilters(
                Filter.newBuilder().setCssSelector("#button"));
        ClickProto.Builder click = ClickProto.newBuilder().setElementToClick(button);
        ArrayList<ActionProto> actions = new ArrayList<>();
        actions.add(
                ActionProto.newBuilder().setClick(click.setOnTop(OptionalStep.SKIP_STEP)).build());
        actions.add(ActionProto.newBuilder()
                            .setClick(click.setOnTop(OptionalStep.REPORT_STEP_RESULT))
                            .build());
        actions.add(ActionProto.newBuilder()
                            .setClick(click.setOnTop(OptionalStep.REQUIRE_STEP_SUCCESS))
                            .build());

        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(new AutofillAssistantTestScript(TEST_SCRIPT, actions)));
        startAutofillAssistant(mTestRule.getActivity(), testService);
        testService.waitUntilGetNextActions(1);

        List<ProcessedActionProto> processed = testService.getProcessedActions();
        assertThat(processed, hasSize(3));
        assertThat(processed.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        assertThat(processed.get(1).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        assertThat(processed.get(1).getStatusDetails().getOriginalStatus(),
                is(ProcessedActionStatusProto.ELEMENT_NOT_ON_TOP));
        assertThat(processed.get(2).getStatus(), is(ProcessedActionStatusProto.ELEMENT_NOT_ON_TOP));
    }

    @Test
    @MediumTest
    public void setTextWithMiniActions() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto element = SelectorProto.newBuilder()
                                        .addFilters(Filter.newBuilder().setCssSelector("#input1"))
                                        .build();
        ClientIdProto clientId = ClientIdProto.newBuilder().setIdentifier("e").build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setWaitForDom(
                                 WaitForDomProto.newBuilder().setTimeoutMs(1000).setWaitCondition(
                                         ElementConditionProto.newBuilder()
                                                 .setMatch(element)
                                                 .setClientId(clientId)))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setWaitForDocumentToBecomeInteractive(
                                 WaitForDocumentToBecomeInteractiveProto.newBuilder()
                                         .setClientId(clientId)
                                         .setTimeoutInMs(1000))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setWaitForElementToBecomeStable(
                                 WaitForElementToBecomeStableProto.newBuilder()
                                         .setClientId(clientId)
                                         .setStableCheckMaxRounds(10)
                                         .setStableCheckIntervalMs(200))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setScrollIntoView(ScrollIntoViewProto.newBuilder().setClientId(clientId))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCheckElementIsOnTop(
                                 CheckElementIsOnTopProto.newBuilder().setClientId(clientId))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSetElementAttribute(
                                 SetElementAttributeProto.newBuilder()
                                         .setClientId(clientId)
                                         .addAttribute("value")
                                         .setValue(TextValue.newBuilder().setText("")))
                         .build());
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setSendChangeEvent(SendChangeEventProto.newBuilder().setClientId(clientId))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSendClickEvent(SendClickEventProto.newBuilder().setClientId(clientId))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSendKeystrokeEvents(
                                 SendKeystrokeEventsProto.newBuilder()
                                         .setClientId(clientId)
                                         .setDelayInMs(0)
                                         .setValue(TextValue.newBuilder().setText("Value")))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setReleaseElements(
                                 ReleaseElementsProto.newBuilder().addClientIds(clientId))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Done").addChoices(
                                 Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);

        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Done"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("Value"));
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
