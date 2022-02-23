// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getElementValue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilKeyboardMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addClickSteps;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addKeyboardSteps;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addKeyboardWithSelectSteps;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toIFrameCssSelector;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto.Rectangle;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowCastProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Tests autofill assistant's interaction with the keyboard.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantKeyboardIntegrationTest {
    private static final String TEST_PAGE = "form_target_website.html";

    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(
            new AutofillAssistantCustomTabTestRule(mTestRule, TEST_PAGE));

    private void runAutofillAssistant(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }

    private boolean isKeyboardVisible() {
        CustomTabActivity activity = mTestRule.getActivity();
        return activity.getWindowAndroid().getKeyboardDelegate().isKeyboardShowing(
                activity, activity.getCompositorViewHolderForTesting());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1272997, https://crbug.com/1273143")
    public void keyboardDoesNotShowOnElementClick() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto nameSelector = toCssSelector("#profile_name");
        addClickSteps(nameSelector, list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Clicked").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder()
                                                 .setType(ChipType.HIGHLIGHTED_ACTION)
                                                 .setText("Continue"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(nameSelector)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      nameSelector))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Highlighted")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        runAutofillAssistant(script);

        // Autofill Assistant clicking an <input> element does not show the keyboard.
        waitUntilViewMatchesCondition(withText("Clicked"), isCompletelyDisplayed());
        assertThat(isKeyboardVisible(), is(false));

        // A user's click on an <input> element does show the keyboard.
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Highlighted"), isCompletelyDisplayed());
        tapElement(mTestRule, "profile_name");
        waitUntilKeyboardMatchesCondition(mTestRule, /* isShowing= */ true);
    }

    @Test
    @MediumTest
    @DisableIf.Build(message = "Fails on Marshmallow, https://crbug.com/1272863",
            sdk_is_greater_than = VERSION_CODES.LOLLIPOP_MR1, sdk_is_less_than = VERSION_CODES.N)
    public void
    keyboardDoesNotShowOnKeyStrokes() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto nameSelector = toCssSelector("#profile_name");
        addKeyboardSteps(nameSelector, "John Doe", list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Filled").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder()
                                                 .setType(ChipType.HIGHLIGHTED_ACTION)
                                                 .setText("Continue"))))
                         .build());
        addKeyboardWithSelectSteps(nameSelector, "Jane Doe", list);
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Overwritten")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(nameSelector)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      nameSelector))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Highlighted")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        runAutofillAssistant(script);

        // Autofill Assistant clicking an <input> element does not show the keyboard.
        waitUntilViewMatchesCondition(withText("Filled"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "profile_name"), is("John Doe"));
        assertThat(isKeyboardVisible(), is(false));

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Overwritten"), isCompletelyDisplayed());
        assertThat(getElementValue(mTestRule.getWebContents(), "profile_name"), is("Jane Doe"));
        assertThat(isKeyboardVisible(), is(false));

        // A user's click on an <input> element does show the keyboard.
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Highlighted"), isCompletelyDisplayed());
        tapElement(mTestRule, "profile_name");
        waitUntilKeyboardMatchesCondition(mTestRule, /* isShowing= */ true);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1272997")
    public void keyboardDoesNotShowOnElementClickInIFrame() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto nameSelector = toIFrameCssSelector("#iframe", "#name");
        addClickSteps(nameSelector, list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Clicked").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder()
                                                 .setType(ChipType.HIGHLIGHTED_ACTION)
                                                 .setText("Continue"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(nameSelector)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      nameSelector))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Highlighted")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        runAutofillAssistant(script);

        // Autofill Assistant clicking an <input> element does not show the keyboard.
        waitUntilViewMatchesCondition(withText("Clicked"), isCompletelyDisplayed());
        assertThat(isKeyboardVisible(), is(false));

        // A user's click on an <input> element does show the keyboard.
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Highlighted"), isCompletelyDisplayed());
        tapElement(mTestRule, "iframe", "name");
        waitUntilKeyboardMatchesCondition(mTestRule, /* isShowing= */ true);
    }

    // When the keyboard is showing to type in the website, nothing should happen to the chips.
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1263727")
    public void doNotHideChipsWhileKeyboardShowingForWebsiteTextInput() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        SelectorProto nameSelector = toCssSelector("#profile_name");
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(nameSelector)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      nameSelector))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(
                                 PromptProto.newBuilder()
                                         .setMessage("Highlighted")
                                         .addChoices(
                                                 Choice.newBuilder()
                                                         .setChip(ChipProto.newBuilder().setText(
                                                                 "Done"))
                                                         .setShowOnlyWhen(
                                                                 ElementConditionProto.newBuilder()
                                                                         .setMatch(nameSelector)))
                                         .addChoices(Choice.newBuilder().setChip(
                                                 ChipProto.newBuilder()
                                                         .setType(ChipType.CANCEL_ACTION)
                                                         .setText("Cancel"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        runAutofillAssistant(script);

        waitUntilViewMatchesCondition(withText("Highlighted"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(withText("Done"), isDisplayed());
        onView(withText("Cancel")).check(matches(isDisplayed()));

        tapElement(mTestRule, "profile_name");
        waitUntilKeyboardMatchesCondition(mTestRule, /* isShowing= */ true);
        onView(withText("Done")).check(matches(isDisplayed()));
        onView(withText("Cancel")).check(matches(isDisplayed()));

        // Clicking on a cancel chip while the keyboard is showing hides the keyboard instead of
        // closing Autofill Assistant.
        onView(withText("Cancel")).perform(click());
        waitUntilKeyboardMatchesCondition(mTestRule, /* isShowing= */ false);
        onView(withText("Done")).check(matches(isDisplayed()));
        onView(withText("Cancel")).check(matches(isDisplayed()));
    }
}
