// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.action.ViewActions.typeText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayingAtLeast;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getAbsoluteBoundingRect;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode.HANDLE;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode.HANDLE_HEADER;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode.HANDLE_HEADER_CAROUSELS;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.ViewportResizing.NO_RESIZE;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.ViewportResizing.RESIZE_LAYOUT_VIEWPORT;
import static org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.ViewportResizing.RESIZE_VISUAL_VIEWPORT;
import static org.chromium.components.autofill_assistant.AssistantTagsForTesting.RECYCLER_VIEW_TAG;

import android.graphics.Rect;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipIcon;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientSettingsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.ViewportResizing;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureUiStateProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureUiStateProto.OverlayBehavior;
import org.chromium.chrome.browser.autofill_assistant.proto.DetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowCastProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowDetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputSectionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UserFormSectionProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.R;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests autofill assistant bottomsheet.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantBottomsheetTest {
    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain =
            RuleChain.outerRule(mTestRule).around(new AutofillAssistantCustomTabTestRule(
                    mTestRule, "bottomsheet_behaviour_target_website.html"));

    private AutofillAssistantTestScript makeScriptWithActionArray(
            ArrayList<ActionProto> actionsList) {
        return new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("bottomsheet_behaviour_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                actionsList);
    }

    private AutofillAssistantTestScript makeScript(
            ViewportResizing resizing, PeekMode peekMode, boolean withDetails) {
        ArrayList<ActionProto> list = new ArrayList<>();
        // Prompt.
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Hello world!")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.DONE_ACTION)
                                                            .setText("Focus element"))))
                         .build());
        // Set viewport resizing and peek mode.
        list.add(ActionProto.newBuilder()
                         .setConfigureBottomSheet(ConfigureBottomSheetProto.newBuilder()
                                                          .setViewportResizing(resizing)
                                                          .setPeekMode(peekMode))
                         .build());
        // Focus on the bottom element.
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder().setElementToPresent(
                                 toCssSelector("div.bottom")))
                         .build());
        // Hides the overlay
        list.add(ActionProto.newBuilder()
                         .setConfigureUiState(ConfigureUiStateProto.newBuilder().setOverlayBehavior(
                                 OverlayBehavior.HIDDEN))
                         .build());
        if (withDetails) {
            // ShowDetails.
            list.add(ActionProto.newBuilder()
                             .setShowDetails(ShowDetailsProto.newBuilder().setDetails(
                                     DetailsProto.newBuilder()
                                             .setTitle("Details title")
                                             .setPlaceholders(DetailsProto.PlaceholdersConfiguration
                                                                      .newBuilder()
                                                                      .setShowImagePlaceholder(true)
                                                                      .build())))
                             .build());
        }
        // First prompt, at this point the sheet is expanded.
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 Choice.newBuilder().setChip(ChipProto.newBuilder()
                                                                     .setType(ChipType.DONE_ACTION)
                                                                     .setText("Collapse"))))
                         .build());
        // Collapse the sheet.
        list.add(ActionProto.newBuilder()
                         .setConfigureBottomSheet(
                                 ConfigureBottomSheetProto.newBuilder().setCollapse(true))
                         .build());
        // Second prompt, at this point the sheet should be collapsed.
        // Since the sheet is collapsed we can't click a button to finish the action, so we add an
        // element condition to be able to finish by clicking an element in the website.
        SelectorProto touchArea = toCssSelector("#touch_area");
        ElementConditionProto autoSelectCondition =
                ElementConditionProto.newBuilder()
                        .setNoneOf(ElementConditionsProto.newBuilder().addConditions(
                                ElementConditionProto.newBuilder().setMatch(touchArea)))
                        .build();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .addChoices(
                                                    Choice.newBuilder()
                                                            .setChip(ChipProto.newBuilder().setText(
                                                                    "Expand"))
                                                            .setAutoSelectWhen(autoSelectCondition))
                                            .setDisableForceExpandSheet(true))
                         .build());
        // Expand the sheet.
        list.add(ActionProto.newBuilder()
                         .setConfigureBottomSheet(
                                 ConfigureBottomSheetProto.newBuilder().setExpand(true))
                         .build());
        // Final prompt, the sheet should be expanded again.
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 Choice.newBuilder().setChip(ChipProto.newBuilder()
                                                                     .setType(ChipType.DONE_ACTION)
                                                                     .setText("Done"))))
                         .build());

        return makeScriptWithActionArray(list);
    }

    @Test
    @MediumTest
    public void testNoResize() {
        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(makeScript(NO_RESIZE, HANDLE, false)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Focus element"), isDisplayingAtLeast(90));
        onView(withText("Focus element")).perform(click());
        waitUntilViewMatchesCondition(withText("Collapse"), isDisplayingAtLeast(90));
        checkElementIsCoveredByBottomsheet("bottom", true);
        onView(withText("Collapse")).perform(click());
        waitUntilViewMatchesCondition(withText("Hello world!"), not(isDisplayed()));
        // Since no resizing of the viewport happens in this mode, the element is partially covered
        // even when the bottom sheet is minimized
        checkElementIsCoveredByBottomsheet("bottom", true);
    }

    @Test
    @MediumTest
    public void testResizeLayoutViewport() throws Exception {
        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(makeScript(RESIZE_LAYOUT_VIEWPORT, HANDLE, false)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Focus element"), isDisplayingAtLeast(90));
        onView(withText("Focus element")).perform(click());
        waitUntilViewMatchesCondition(withText("Collapse"), isDisplayingAtLeast(90));
        // The viewport should be resized so that the bottom element is not covered by the bottom
        // sheet.
        checkElementIsCoveredByBottomsheet("bottom", false);
        onView(withText("Collapse")).perform(click());
        // Minimizing the bottomsheet should completely uncover the bottom element.
        waitUntilViewMatchesCondition(withText("Hello world!"), not(isDisplayed()));
        checkElementIsCoveredByBottomsheet("bottom", false);
        onView(withText("Expand")).check(matches(not(isDisplayed())));
        // We tap the element as a way of ending the action without having to manually expand the
        // sheet.
        tapElement(mTestRule, "touch_area");
        checkElementIsCoveredByBottomsheet("bottom", true);
        waitUntilViewMatchesCondition(withText("Hello world!"), isDisplayed());
    }

    @Test
    @MediumTest
    public void testResizeVisualViewport() throws Exception {
        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(makeScript(RESIZE_VISUAL_VIEWPORT, HANDLE, false)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Focus element"), isDisplayingAtLeast(90));
        onView(withText("Focus element")).perform(click());
        waitUntilViewMatchesCondition(withText("Collapse"), isDisplayingAtLeast(90));
        // The viewport should be resized so that the bottom element is not covered by the bottom
        // sheet.
        checkElementIsCoveredByBottomsheet("bottom", false);
        onView(withText("Collapse")).perform(click());
        waitUntilViewMatchesCondition(withText("Hello world!"), not(isDisplayed()));
        checkElementIsCoveredByBottomsheet("bottom", false);
        onView(withText("Expand")).check(matches(not(isDisplayed())));
        // We tap the element as a way of ending the action without having to manually expand the
        // sheet.
        tapElement(mTestRule, "touch_area");
        checkElementIsCoveredByBottomsheet("bottom", true);
        waitUntilViewMatchesCondition(withText("Hello world!"), isDisplayed());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1236142")
    public void testHandleHeader() throws Exception {
        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(makeScript(RESIZE_LAYOUT_VIEWPORT, HANDLE_HEADER, true)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Focus element"), isDisplayingAtLeast(90));
        onView(withText("Focus element")).perform(click());
        waitUntilViewMatchesCondition(withText("Collapse"), isDisplayingAtLeast(90));
        checkElementIsCoveredByBottomsheet("bottom", true);
        onView(withText("Collapse")).perform(click());
        checkElementIsCoveredByBottomsheet("bottom", false);
        // The header should be visible even when minimized
        onView(withText("Hello world!")).check(matches(isDisplayingAtLeast(90)));
        onView(withText("Details title")).check(matches(not(isDisplayed())));
        onView(withText("Expand")).check(matches(not(isDisplayed())));
        // We tap the element as a way of ending the action without having to manually expand the
        // sheet.
        tapElement(mTestRule, "touch_area");
        checkElementIsCoveredByBottomsheet("bottom", true);
        waitUntilViewMatchesCondition(withText("Details title"), isDisplayingAtLeast(90));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1236142")
    public void testHandleHeaderCarousels() {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(
                        makeScript(RESIZE_LAYOUT_VIEWPORT, HANDLE_HEADER_CAROUSELS, true)));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Focus element"), isDisplayingAtLeast(90));
        onView(withText("Focus element")).perform(click());
        waitUntilViewMatchesCondition(withText("Collapse"), isDisplayingAtLeast(90));
        checkElementIsCoveredByBottomsheet("bottom", true);
        onView(withText("Collapse")).perform(click());
        checkElementIsCoveredByBottomsheet("bottom", false);
        // The header should be visible even when minimized
        onView(withText("Hello world!")).check(matches(isDisplayingAtLeast(90)));
        // The button gets initially hidden while swiping down but should reappear shortly after.
        waitUntilViewMatchesCondition(withText("Expand"), isDisplayingAtLeast(90));
        onView(withText("Details title")).check(matches(not(isDisplayed())));
        onView(withText("Expand")).perform(click());
        checkElementIsCoveredByBottomsheet("bottom", true);
        onView(withText("Details title")).check(matches(isDisplayingAtLeast(90)));
    }

    @Test
    @MediumTest
    public void testBottomSheetDoesNotObstructNavBar() {
        // Create enough additional sections to fill up more than the height of the screen.
        List<UserFormSectionProto> additionalSections = new ArrayList<>();
        for (int i = 0; i < 20; ++i) {
            additionalSections.add(
                    UserFormSectionProto.newBuilder()
                            .setTextInputSection(TextInputSectionProto.newBuilder().addInputFields(
                                    TextInputProto.newBuilder()
                                            .setHint("Text input " + i)
                                            .setClientMemoryKey("input_" + i)
                                            .setInputType(TextInputProto.InputType.INPUT_TEXT)))
                            .setTitle("Title " + i)
                            .build());
        }

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .addAllAdditionalAppendedSections(additionalSections)
                                         .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("bottomsheet_behaviour_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
        waitUntilViewMatchesCondition(withText("Continue"), isDisplayingAtLeast(90));
        onView(withId(R.id.control_container)).check(matches(isDisplayingAtLeast(90)));
        onView(withText("Title 0")).perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Text input 0"), isDisplayingAtLeast(90));
        // Typing text will show the soft keyboard, leading to resize of the Chrome window.
        onView(withContentDescription("Text input 0")).perform(typeText("Hello World!"));
        onView(withId(R.id.control_container)).check(matches(isDisplayingAtLeast(90)));
        onView(allOf(withContentDescription("Close"), isDisplayed()))
                .check(matches(isDisplayingAtLeast(90)));
        // Closing the soft keyboard will restore the window size.
        Espresso.closeSoftKeyboard();
        onView(withContentDescription("Text input 0")).check(matches(isDisplayed()));
        onView(withId(R.id.control_container)).check(matches(isDisplayingAtLeast(90)));
        onView(withText("Continue")).check(matches(isDisplayingAtLeast(90)));

        // Scroll down.
        onView(withText("Title 19")).check(matches(not(isDisplayed())));
        onView(withText("Title 19")).perform(scrollTo()).check(matches(isDisplayed()));

        // Scroll up.
        onView(withText("Title 0")).check(matches(not(isDisplayed())));
        onView(withText("Title 0")).perform(scrollTo()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testCancelSnackbarUndo() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(Choice.newBuilder().setChip(
                                 ChipProto.newBuilder()
                                         .setType(ChipType.CANCEL_ACTION)
                                         .setIcon(ChipIcon.ICON_CLEAR)
                                         .setText("Cancel"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("bottomsheet_behaviour_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
        waitUntilViewMatchesCondition(withText("Cancel"), isDisplayingAtLeast(90));
        onView(withText("Cancel")).perform(click());
        waitUntilViewMatchesCondition(withText(R.string.undo), isDisplayingAtLeast(90));
        onView(withText("Cancel")).check(doesNotExist());
        onView(withText(R.string.undo)).perform(click());
        waitUntilViewMatchesCondition(withText("Cancel"), isDisplayed());
    }

    @Test
    @MediumTest
    public void testCancelSnackbarWithStringUndo() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(Choice.newBuilder().setChip(
                                 ChipProto.newBuilder()
                                         .setType(ChipType.CANCEL_ACTION)
                                         .setIcon(ChipIcon.ICON_CLEAR)
                                         .setText("Cancel"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("bottomsheet_behaviour_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService = new AutofillAssistantTestService(
                Collections.singletonList(script),
                ClientSettingsProto.newBuilder()
                        .setIntegrationTestSettings(
                                ClientSettingsProto.IntegrationTestSettings.newBuilder()
                                        .setDisableHeaderAnimations(true)
                                        .setDisableCarouselChangeAnimations(true))
                        .setDisplayStringsLocale("fr-FR")
                        .addDisplayStrings(ClientSettingsProto.DisplayString.newBuilder()
                                                   .setId(ClientSettingsProto.DisplayStringId.UNDO)
                                                   .setValue("fr_undo"))
                        .build());
        startAutofillAssistant(mTestRule.getActivity(), testService);
        waitUntilViewMatchesCondition(withText("Cancel"), isDisplayingAtLeast(90));
        onView(withText("Cancel")).perform(click());
        waitUntilViewMatchesCondition(withText("fr_undo"), isDisplayingAtLeast(90));
        onView(withText("Cancel")).check(doesNotExist());
        onView(withText("fr_undo")).perform(click());
        waitUntilViewMatchesCondition(withText("Cancel"), isDisplayed());
    }

    @Test
    @MediumTest
    public void testCancelSnackbarTimeout() {
        ClientSettingsProto clientSettings =
                ClientSettingsProto.newBuilder().setCancelDelayMs(2000).build();
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(Choice.newBuilder().setChip(
                                 ChipProto.newBuilder()
                                         .setType(ChipType.CANCEL_ACTION)
                                         .setIcon(ChipIcon.ICON_CLEAR)
                                         .setText("Cancel"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("bottomsheet_behaviour_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script), clientSettings);
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Cancel"), isDisplayingAtLeast(90));
        onView(withText("Cancel")).perform(click());
        waitUntilViewMatchesCondition(withText(R.string.undo), isDisplayingAtLeast(90));
        onView(withText("Cancel")).check(doesNotExist());
        waitUntilViewAssertionTrue(withText(R.string.undo), doesNotExist(), 3000L);
        onView(withId(R.id.autofill_assistant)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky test.  crbug.com/1114818")
    public void testBottomSheetAutoCollapseAndExpand() {
        ArrayList<ActionProto> list = new ArrayList<>();
        // Prompt.
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Hello world!")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.DONE_ACTION)
                                                            .setText("Focus element"))))
                         .build());
        // Focus on the bottom element.
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder().setElementToPresent(
                                 toCssSelector("p.bottom")))
                         .build());
        // Set handle and header peek mode and auto collapse to that state.
        list.add(ActionProto.newBuilder()
                         .setConfigureBottomSheet(ConfigureBottomSheetProto.newBuilder()
                                                          .setViewportResizing(NO_RESIZE)
                                                          .setPeekMode(HANDLE_HEADER)
                                                          .setCollapse(true))
                         .build());
        // Add sticky "Next" button. Disable auto expanding the sheet for prompt actions.
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.DONE_ACTION)
                                                            .setSticky(true)
                                                            .setText("Sticky next")))
                                            .setDisableForceExpandSheet(true))
                         .build());
        // Expand the sheet.
        list.add(ActionProto.newBuilder()
                         .setConfigureBottomSheet(ConfigureBottomSheetProto.newBuilder()
                                                          .setViewportResizing(NO_RESIZE)
                                                          .setExpand(true))
                         .build());
        // Add "Done" button.
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 Choice.newBuilder().setChip(ChipProto.newBuilder()
                                                                     .setType(ChipType.DONE_ACTION)
                                                                     .setText("Done"))))
                         .build());

        AutofillAssistantTestScript script = makeScriptWithActionArray(list);
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Focus element"), isDisplayingAtLeast(90));
        onView(withText("Focus element")).perform(click());

        // Check that the sheet is in peek state and has a sticky button. There is
        // a second button still in the hidden carousel.
        waitUntilViewMatchesCondition(
                allOf(withText("Sticky next"), isDescendantOfA(withId(R.id.header))),
                isDisplayingAtLeast(90));
        onView(allOf(withText("Sticky next"), isDescendantOfA(withTagValue(is(RECYCLER_VIEW_TAG)))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withText("Sticky next"), isDescendantOfA(withId(R.id.header))))
                .perform(click());

        // Check that the sheet is now expanded and the done button is part of the recycler view,
        // not the header.
        waitUntilViewMatchesCondition(
                allOf(withText("Done"), isDescendantOfA(withTagValue(is(RECYCLER_VIEW_TAG)))),
                isDisplayingAtLeast(90));
    }

    private void checkElementIsCoveredByBottomsheet(String elementId, boolean shouldBeCovered) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            float y = GeneralLocation.TOP_CENTER.calculateCoordinates(
                    mTestRule.getActivity().findViewById(
                            R.id.autofill_assistant_bottom_sheet_toolbar))[1];
            Rect el = null;
            try {
                el = getAbsoluteBoundingRect(mTestRule, elementId);
            } catch (Exception ex) {
                throw new RuntimeException(ex);
            }
            String errorMsg = "Timeout while waiting for element '" + elementId + "' to become "
                    + (shouldBeCovered ? "covered" : "not covered") + " by the bottomsheet";
            if (shouldBeCovered) {
                Criteria.checkThat(errorMsg, (float) el.bottom, Matchers.greaterThan(y));
            } else {
                Criteria.checkThat(errorMsg, (float) el.bottom, Matchers.lessThanOrEqualTo(y));
            }
        });
    }
}
