// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.anyIntent;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasData;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.hasTextColor;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.iterableWithSize;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.hasTintColor;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.hasTypefaceSpan;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.withParentIndex;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.graphics.Typeface;
import android.net.Uri;
import android.widget.RadioButton;

import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.CounterInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CounterInputProto.Counter;
import org.chromium.chrome.browser.autofill_assistant.proto.FormInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.FormProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InfoPopupProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InfoPopupProto.DialogButton;
import org.chromium.chrome.browser.autofill_assistant.proto.InfoPopupProto.DialogButton.OpenUrlInCCT;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionStatusProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectionInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowFormProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests autofill assistant bottomsheet.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantFormActionTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "autofill_assistant_target_website.html";

    @Before
    public void setUp() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(
                AutofillAssistantUiTestUtil.createMinimalCustomTabIntentForAutobot(
                        mTestRule.getTestServer().getURL(TEST_PAGE),
                        /* startImmediately = */ true));
        mTestRule.getActivity()
                .getRootUiCoordinatorForTesting()
                .getScrimCoordinator()
                .disableAnimationForTesting(true);
    }

    /**
     * Creates a close-to-real example of a form action with multiple counters and choices,
     * interacts with those widgets, and then checks the response to the server.
     */
    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testFormAction() {
        ArrayList<ActionProto> list = new ArrayList<>();
        // FromProto.Builder, extracted to avoid excessive line widths.
        FormProto.Builder formProto =
                FormProto.newBuilder()
                        .addInputs(FormInputProto.newBuilder().setCounter(
                                CounterInputProto.newBuilder()
                                        .addCounters(CounterInputProto.Counter.newBuilder()
                                                             .setMinValue(0)
                                                             .setMaxValue(1)
                                                             .setLabel("Counter 1")
                                                             .setDescriptionLine1("$34.00 per item")
                                                             .setDescriptionLine2(
                                                                     "<link1>Details</link1>"))
                                        .addCounters(
                                                CounterInputProto.Counter.newBuilder()
                                                        .setMinValue(1)
                                                        .setMaxValue(9)
                                                        .setLabel("Counter 2")
                                                        .setDescriptionLine1("$387.00 per item"))
                                        .setMinimizedCount(1)
                                        .setMinCountersSum(2)
                                        .setMinimizeText("Minimize")
                                        .setExpandText("Expand")))
                        .addInputs(FormInputProto.newBuilder().setSelection(
                                SelectionInputProto.newBuilder()
                                        .addChoices(SelectionInputProto.Choice.newBuilder()
                                                            .setLabel("Choice 1")
                                                            .setDescriptionLine1("$10.00 option")
                                                            .setDescriptionLine2(
                                                                    "<link1>Details</link1>"))
                                        .addChoices(SelectionInputProto.Choice.newBuilder()
                                                            .setLabel("Choice 2")
                                                            .setDescriptionLine1("$20.00 option")
                                                            .setDescriptionLine2(
                                                                    "<link1>Details</link1>"))
                                        .setAllowMultiple(false)))
                        .addInputs(FormInputProto.newBuilder().setCounter(
                                CounterInputProto.newBuilder().addCounters(
                                        CounterInputProto.Counter.newBuilder()
                                                .setMinValue(1)
                                                .setMaxValue(9)
                                                .setLabel("Counter 3")
                                                .setDescriptionLine1("$20.00 per item")
                                                .setDescriptionLine2("<link1>Details</link1>"))));
        // FormAction.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowForm(ShowFormProto.newBuilder()
                                              .setChip(ChipProto.newBuilder()
                                                               .setType(ChipType.HIGHLIGHTED_ACTION)
                                                               .setText("Continue"))
                                              .setForm(formProto))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        // TODO(b/144690738) Remove the isDisplayed() condition.
        onView(allOf(withId(R.id.value), withEffectiveVisibility(VISIBLE),
                       hasSibling(hasDescendant(withText("Counter 1")))))
                .check(matches(hasTextColor(R.color.modern_grey_800_alpha_38)));
        onView(allOf(withId(R.id.increase_button), withEffectiveVisibility(VISIBLE),
                       hasSibling(hasDescendant(withText("Counter 1")))))
                .check(matches(hasTintColor(R.color.modern_blue_600)));
        onView(allOf(withId(R.id.decrease_button), withEffectiveVisibility(VISIBLE),
                       hasSibling(hasDescendant(withText("Counter 1")))))
                .check(matches(hasTintColor(R.color.modern_grey_800_alpha_38)));
        // Click on Counter 1 +, increase from 0 to 1.
        onView(allOf(withId(R.id.increase_button), withEffectiveVisibility(VISIBLE),
                       hasSibling(hasDescendant(withText("Counter 1")))))
                .perform(scrollTo(), click());
        onView(allOf(withId(R.id.value), withEffectiveVisibility(VISIBLE),
                       hasSibling(hasDescendant(withText("Counter 1")))))
                .check(matches(hasTextColor(R.color.modern_blue_600)));
        onView(allOf(withId(R.id.increase_button), withEffectiveVisibility(VISIBLE),
                       hasSibling(hasDescendant(withText("Counter 1")))))
                .check(matches(hasTintColor(R.color.modern_grey_800_alpha_38)));
        // Decrease button is still disabled due to the minCountersSum requirement.

        // Click expand label to make Counter 2 visible.
        onView(allOf(withId(R.id.expand_label), withEffectiveVisibility(VISIBLE)))
                .perform(scrollTo(), click());
        // Click on Counter 3 +, increase from 0 to 1.
        onView(allOf(withId(R.id.increase_button), withEffectiveVisibility(VISIBLE),
                       hasSibling(hasDescendant(withText("Counter 3")))))
                .perform(scrollTo(), click());

        // Click on Choice 1, then Choice 2, then back to Choice 1.
        onView(allOf(withClassName(is(RadioButton.class.getName())), withParentIndex(0),
                       withEffectiveVisibility(VISIBLE)))
                .perform(scrollTo(), click());
        onView(allOf(withClassName(is(RadioButton.class.getName())), withParentIndex(3),
                       withEffectiveVisibility(VISIBLE)))
                .perform(scrollTo(), click());
        onView(allOf(withClassName(is(RadioButton.class.getName())), withParentIndex(0),
                       withEffectiveVisibility(VISIBLE)))
                .perform(scrollTo(), click());

        // Check that choice 1 is visually selected and choice 2 is de-selected.
        onView(allOf(withClassName(is(RadioButton.class.getName())), withParentIndex(0),
                       withEffectiveVisibility(VISIBLE)))
                .check(matches(isChecked()));
        onView(allOf(withClassName(is(RadioButton.class.getName())), withParentIndex(3),
                       withEffectiveVisibility(VISIBLE)))
                .check(matches(not(isChecked())));

        // Click on Counter 2 +, increase from 0 to 1.
        onView(allOf(withId(R.id.increase_button), withEffectiveVisibility(VISIBLE),
                       hasSibling(hasDescendant(withText("Counter 2")))))
                .perform(scrollTo(), click());

        // Finish form action, wait for response and prepare next set of actions.
        List<ActionProto> nextActions = new ArrayList<>();
        nextActions.add((ActionProto) ActionProto.newBuilder()
                                .setPrompt(PromptProto.newBuilder()
                                                   .setMessage("Finished")
                                                   .addChoices(Choice.newBuilder().setChip(
                                                           ChipProto.newBuilder()
                                                                   .setType(ChipType.DONE_ACTION)
                                                                   .setText("End"))))
                                .build());
        testService.setNextActions(nextActions);
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withText("Continue")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        assertThat(processedActions.get(0).getResultDataCase(),
                is(ProcessedActionProto.ResultDataCase.FORM_RESULT));

        List<FormInputProto.Result> formResult =
                processedActions.get(0).getFormResult().getInputResultsList();
        assertThat(formResult.size(), is(3));
        assertThat(formResult.get(0).getInputTypeCase(),
                is(FormInputProto.Result.InputTypeCase.COUNTER));
        assertThat(formResult.get(0).getCounter().getValuesCount(), is(2));
        // Counter 1
        assertThat(formResult.get(0).getCounter().getValues(0), is(1));
        // Counter 2
        assertThat(formResult.get(0).getCounter().getValues(1), is(1));

        // Choice 1
        assertThat(formResult.get(1).getInputTypeCase(),
                is(FormInputProto.Result.InputTypeCase.SELECTION));
        assertThat(formResult.get(1).getSelection().getSelectedCount(), is(2));
        assertThat(formResult.get(1).getSelection().getSelected(0), is(true));
        assertThat(formResult.get(1).getSelection().getSelected(1), is(false));

        // Counter 3
        assertThat(formResult.get(2).getInputTypeCase(),
                is(FormInputProto.Result.InputTypeCase.COUNTER));
        assertThat(formResult.get(2).getCounter().getValuesCount(), is(1));
        assertThat(formResult.get(2).getCounter().getValues(0), is(1));

        waitUntilViewMatchesCondition(withText("End"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testFormActionClickLink() {
        ArrayList<ActionProto> list = new ArrayList<>();
        // FromProto.Builder, extracted to avoid excessive line widths.
        FormProto.Builder formProto =
                FormProto.newBuilder().addInputs(FormInputProto.newBuilder().setCounter(
                        CounterInputProto.newBuilder()
                                .addCounters(CounterInputProto.Counter.newBuilder()
                                                     .setMinValue(1)
                                                     .setMaxValue(9)
                                                     .setLabel("Counter 1")
                                                     .setDescriptionLine1("$34.00 per item")
                                                     .setDescriptionLine2("<link4>Details</link4>")
                                                     .setInitialValue(1))
                                .setMinimizedCount(1)
                                .setMinCountersSum(2)
                                .setMinimizeText("Minimize")
                                .setExpandText("Expand")));
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowForm(ShowFormProto.newBuilder()
                                              .setChip(ChipProto.newBuilder()
                                                               .setType(ChipType.HIGHLIGHTED_ACTION)
                                                               .setText("Continue"))
                                              .setForm(formProto))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        // Click on Counter 1 +, increase from 1 to 2.
        onView(allOf(withId(R.id.increase_button), withEffectiveVisibility(VISIBLE),
                       hasSibling(hasDescendant(withText("Counter 1")))))
                .perform(scrollTo(), click());

        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(allOf(isDisplayed(), withText("Details"))).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        assertThat(processedActions.get(0).getResultDataCase(),
                is(ProcessedActionProto.ResultDataCase.FORM_RESULT));

        List<FormInputProto.Result> formResult =
                processedActions.get(0).getFormResult().getInputResultsList();
        assertThat(formResult.size(), is(1));
        assertThat(processedActions.get(0).getFormResult().getLink(), is(4));
        assertThat(formResult.get(0).getInputTypeCase(),
                is(FormInputProto.Result.InputTypeCase.COUNTER));
        assertThat(formResult.get(0).getCounter().getValuesCount(), is(1));
        // Counter 1
        assertThat(formResult.get(0).getCounter().getValues(0), is(2));
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testInfoPopup() {
        ArrayList<ActionProto> list = new ArrayList<>();
        // FromProto.Builder, extracted to avoid excessive line widths.
        FormProto.Builder formProto =
                FormProto.newBuilder()
                        .addInputs(FormInputProto.newBuilder().setCounter(
                                CounterInputProto.newBuilder().addCounters(
                                        Counter.newBuilder()
                                                .setLabel("Counter 1")
                                                .setDescriptionLine1("$20.00 per tick")
                                                .setDescriptionLine2("<link1>Details</link1>"))))
                        .addInputs(FormInputProto.newBuilder().setCounter(
                                CounterInputProto.newBuilder().addCounters(
                                        Counter.newBuilder()
                                                .setLabel("Counter 2")
                                                .setDescriptionLine1("$20.00 per tick")
                                                .setDescriptionLine2("<link1>Details</link1>"))))
                        .setInfoLabel("<b>Info label with bold text</b>")
                        .setInfoPopup(
                                InfoPopupProto.newBuilder()
                                        .setTitle("Prompt title")
                                        .setText("Prompt text")
                                        .setNeutralButton(
                                                DialogButton.newBuilder()
                                                        .setLabel("Go to url")
                                                        .setOpenUrlInCct(
                                                                OpenUrlInCCT.newBuilder().setUrl(
                                                                        "https://www.google.com"))));
        // FormAction.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowForm(ShowFormProto.newBuilder()
                                              .setChip(ChipProto.newBuilder()
                                                               .setType(ChipType.HIGHLIGHTED_ACTION)
                                                               .setText("Continue"))
                                              .setForm(formProto))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        onView(withText("Info label with bold text"))
                .check(matches(withEffectiveVisibility(VISIBLE)));
        onView(withText("Info label with bold text"))
                .check(matches(hasTypefaceSpan(
                        0, "Info label with bold text".length() - 1, Typeface.BOLD)));

        onView(withId(R.id.info_button)).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt title"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(withText("Prompt text"), isCompletelyDisplayed());

        Intent intent = new Intent();
        ActivityResult intentResult = new ActivityResult(Activity.RESULT_OK, intent);

        Intents.init();
        intending(anyIntent()).respondWith(intentResult);

        onView(withText("Go to url")).perform(click());

        intended(
                allOf(hasAction(Intent.ACTION_VIEW), hasData(Uri.parse("https://www.google.com"))));
        Intents.release();
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testInfoPopupNoButtons() {
        ArrayList<ActionProto> list = new ArrayList<>();
        // FromProto.Builder, extracted to avoid excessive line widths.
        FormProto.Builder formProto =
                FormProto.newBuilder()
                        .addInputs(FormInputProto.newBuilder().setCounter(
                                CounterInputProto.newBuilder().addCounters(
                                        Counter.newBuilder()
                                                .setLabel("Counter")
                                                .setDescriptionLine1("$20.00 per tick")
                                                .setDescriptionLine2("<link1>Details</link1>"))))
                        .setInfoLabel("Info label")
                        .setInfoPopup(InfoPopupProto.newBuilder()
                                              .setTitle("Prompt title")
                                              .setText("Prompt text"));
        // FormAction.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowForm(ShowFormProto.newBuilder()
                                              .setChip(ChipProto.newBuilder()
                                                               .setType(ChipType.HIGHLIGHTED_ACTION)
                                                               .setText("Continue"))
                                              .setForm(formProto))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        onView(withId(R.id.info_button)).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt title"), isCompletelyDisplayed());
        // If no button is set in the proto a "Close" button should be added by default.
        onView(withText("Close")).perform(click());
        onView(withText("Prompt title")).check(doesNotExist());
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_less_than = 21)
    public void testMultipleForms() {
        ArrayList<ActionProto> list = new ArrayList<>();
        // FromProto.Builder, extracted to avoid excessive line widths.
        FormProto.Builder formProtoWithInfo =
                FormProto.newBuilder()
                        .addInputs(FormInputProto.newBuilder().setCounter(
                                CounterInputProto.newBuilder().addCounters(
                                        Counter.newBuilder()
                                                .setLabel("Counter 1")
                                                .setDescriptionLine1("$20.00 per tick")
                                                .setDescriptionLine2("<link1>Details</link1>"))))
                        .setInfoLabel("Info label");
        // FormAction.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowForm(ShowFormProto.newBuilder()
                                              .setChip(ChipProto.newBuilder()
                                                               .setType(ChipType.HIGHLIGHTED_ACTION)
                                                               .setText("Continue"))
                                              .setForm(formProtoWithInfo))
                         .build());

        FormProto.Builder formProto = FormProto.newBuilder().addInputs(
                FormInputProto.newBuilder().setCounter(CounterInputProto.newBuilder().addCounters(
                        Counter.newBuilder()
                                .setLabel("Counter 1")
                                .setDescriptionLine1("$20.00 per tick")
                                .setDescriptionLine2("<link1>Details</link1>"))));

        // FormAction.
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowForm(ShowFormProto.newBuilder()
                                              .setChip(ChipProto.newBuilder()
                                                               .setType(ChipType.HIGHLIGHTED_ACTION)
                                                               .setText("Finish"))
                                              .setForm(formProto))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        onView(withText("Info label")).check(matches(isCompletelyDisplayed()));
        onView(withId(R.id.info_button)).check(matches(not(isDisplayed())));
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Finish"), isCompletelyDisplayed());
        onView(withText("Info label")).check(matches(not(isDisplayed())));
        onView(withId(R.id.info_button)).check(matches(not(isDisplayed())));
    }
}
