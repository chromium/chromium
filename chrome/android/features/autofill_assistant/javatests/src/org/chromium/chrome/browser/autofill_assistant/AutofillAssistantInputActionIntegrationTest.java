// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementExists;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getElementValue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ClickProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ClickType;
import org.chromium.chrome.browser.autofill_assistant.proto.DropdownSelectStrategy;
import org.chromium.chrome.browser.autofill_assistant.proto.KeyboardValueFillStrategy;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectOptionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto.Filter;
import org.chromium.chrome.browser.autofill_assistant.proto.SetFormFieldValueProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetFormFieldValueProto.KeyPress;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Collections;

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

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
    }

    @Test
    @MediumTest
    public void fillFormFieldWithText() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto element_set_value =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#input1"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element_set_value)
                                         .addValue(KeyPress.newBuilder().setText("Set Value"))
                                         .setFillStrategy(KeyboardValueFillStrategy.SET_VALUE))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Set value")
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
                                         .addValue(KeyPress.newBuilder().setText("Keystrokes"))
                                         .setFillStrategy(
                                                 KeyboardValueFillStrategy.SIMULATE_KEY_PRESSES))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Keystrokes")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        SelectorProto element_keystrokes_select =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#input3"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element_keystrokes_select)
                                         .addValue(
                                                 KeyPress.newBuilder().setText("Keystrokes Select"))
                                         .setFillStrategy(
                                                 KeyboardValueFillStrategy
                                                         .SIMULATE_KEY_PRESSES_SELECT_VALUE))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Keystrokes with Select")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        SelectorProto element_keystrokes_focus =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#input4"))
                        .build();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setSetFormValue(
                                SetFormFieldValueProto.newBuilder()
                                        .setElement(element_keystrokes_focus)
                                        .addValue(KeyPress.newBuilder().setText("Keystrokes Focus"))
                                        .setFillStrategy(
                                                KeyboardValueFillStrategy.SIMULATE_KEY_PRESSES))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Keystrokes with Focus")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);

        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is("helloworld2"));
        assertThat(getElementValue(mTestRule.getWebContents(), "input3"), is("helloworld3"));
        assertThat(getElementValue(mTestRule.getWebContents(), "input4"), is("helloworld4"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Set value"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("Set Value"));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("Keystrokes"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is("Keystrokes"));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("Keystrokes with Select"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input3"), is("Keystrokes Select"));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("Keystrokes with Focus"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input4"), is("Keystrokes Focus"));
    }

    @Test
    @MediumTest
    public void clearFormFieldFromText() throws Exception {
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
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        SelectorProto element_keystrokes_select =
                (SelectorProto) SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder().setCssSelector("#input3"))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element_keystrokes_select)
                                         .addValue(KeyPress.newBuilder().setText(""))
                                         .setFillStrategy(
                                                 KeyboardValueFillStrategy
                                                         .SIMULATE_KEY_PRESSES_SELECT_VALUE))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Clear value Keystrokes with Select")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);

        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is("helloworld1"));
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is("helloworld2"));
        assertThat(getElementValue(mTestRule.getWebContents(), "input3"), is("helloworld3"));

        runScript(script);

        waitUntilViewMatchesCondition(withText("Clear value"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input1"), is(""));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("Clear value Keystrokes"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input2"), is(""));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(
                withText("Clear value Keystrokes with Select"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "input3"), is(""));
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
                                         .setSelectedOption("one")
                                         .setSelectStrategy(DropdownSelectStrategy.VALUE_MATCH))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Value Match")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setSelectOption(
                                 SelectOptionProto.newBuilder()
                                         .setElement(element)
                                         .setSelectedOption("Three")
                                         .setSelectStrategy(DropdownSelectStrategy.LABEL_MATCH))
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
                         .setSelectOption(SelectOptionProto.newBuilder()
                                                  .setElement(element)
                                                  .setSelectedOption("Zürich")
                                                  .setSelectStrategy(
                                                          DropdownSelectStrategy.LABEL_STARTS_WITH))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Label Starts With")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);

        runScript(script);

        waitUntilViewMatchesCondition(withText("Value Match"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "select"), is("one"));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("Label Match"), isCompletelyDisplayed());
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

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
                        .build(),
                list);

        checkElementExists(mTestRule.getWebContents(), "touch_area_one");
        checkElementExists(mTestRule.getWebContents(), "touch_area_five");
        checkElementExists(mTestRule.getWebContents(), "touch_area_six");

        runScript(script);

        waitUntilViewMatchesCondition(withText("Click"), isCompletelyDisplayed());
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("Tap"), isCompletelyDisplayed());
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_five"));
        onView(withText("Continue")).perform(click());

        waitUntilViewMatchesCondition(withText("JS"), isCompletelyDisplayed());
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_six"));
    }

    private void runScript(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }
}