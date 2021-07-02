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
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toClientId;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toIFrameCssSelector;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientIdProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto.Rectangle;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.KeyboardValueFillStrategy;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.ScrollIntoViewProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SendClickEventProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetFormFieldValueProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetFormFieldValueProto.KeyPress;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowCastProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.WaitForDomProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
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
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "form_target_website.html";

    private void runAutofillAssistant(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }

    private boolean isKeyboardVisible() {
        CustomTabActivity activity = mTestRule.getActivity();
        return activity.getWindowAndroid().getKeyboardDelegate().isKeyboardShowing(
                activity, activity.getCompositorViewHolder());
    }

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
    }

    @Test
    @MediumTest
    public void keyboardDoesNotShowOnElementClick() throws Exception {
        SelectorProto element = toCssSelector("#profile_name");
        ClientIdProto clientId = toClientId("e");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setWaitForDom(
                                 WaitForDomProto.newBuilder().setTimeoutMs(1000).setWaitCondition(
                                         ElementConditionProto.newBuilder()
                                                 .setMatch(element)
                                                 .setClientId(clientId)))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setScrollIntoView(ScrollIntoViewProto.newBuilder().setClientId(clientId))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setSendClickEvent(SendClickEventProto.newBuilder().setClientId(clientId))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Clicked").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder()
                                                 .setType(ChipType.HIGHLIGHTED_ACTION)
                                                 .setText("Continue"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(element)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      element))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Highlighted")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
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
    public void keyboardDoesNotShowOnKeyStrokes() throws Exception {
        SelectorProto element = toCssSelector("#profile_name");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element)
                                         .addValue(KeyPress.newBuilder().setText("John Doe"))
                                         .setFillStrategy(
                                                 KeyboardValueFillStrategy.SIMULATE_KEY_PRESSES))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Filled").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder()
                                                 .setType(ChipType.HIGHLIGHTED_ACTION)
                                                 .setText("Continue"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setSetFormValue(
                                 SetFormFieldValueProto.newBuilder()
                                         .setElement(element)
                                         .addValue(KeyPress.newBuilder().setText("Jane Doe"))
                                         .setFillStrategy(
                                                 KeyboardValueFillStrategy
                                                         .SIMULATE_KEY_PRESSES_SELECT_VALUE))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Overwritten")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(element)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      element))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Highlighted")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
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
    @DisabledTest(message = "https://crbug.com/1216453")
    public void keyboardDoesNotShowOnElementClickInIFrame() throws Exception {
        SelectorProto element = toIFrameCssSelector("#iframe", "#name");
        ClientIdProto clientId = toClientId("e");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setWaitForDom(
                                 WaitForDomProto.newBuilder().setTimeoutMs(1000).setWaitCondition(
                                         ElementConditionProto.newBuilder()
                                                 .setMatch(element)
                                                 .setClientId(clientId)))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setScrollIntoView(ScrollIntoViewProto.newBuilder().setClientId(clientId))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setSendClickEvent(SendClickEventProto.newBuilder().setClientId(clientId))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Clicked").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder()
                                                 .setType(ChipType.HIGHLIGHTED_ACTION)
                                                 .setText("Continue"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(element)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      element))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Highlighted")
                                            .addChoices(Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
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
    public void doNotHideChipsWhileKeyboardShowingForWebsiteTextInput() throws Exception {
        SelectorProto element = toCssSelector("#profile_name");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(element)
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      element))))
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
                                                                         .setMatch(element)))
                                         .addChoices(Choice.newBuilder().setChip(
                                                 ChipProto.newBuilder()
                                                         .setType(ChipType.CANCEL_ACTION)
                                                         .setText("Cancel"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Done")))
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