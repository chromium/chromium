// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.typeText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addTapSteps;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.NavigateProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.StopProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TellProto;
import org.chromium.chrome.browser.autofill_assistant.proto.WaitForNavigationProto;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.autofill_assistant.R;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Tests autofill assistant in a normal Chrome tab.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantNavigationIntegrationTest {
    private static final String TEST_PAGE_A = "autofill_assistant_target_website.html";
    private static final String TEST_PAGE_B = "form_target_website.html";

    private final ChromeTabbedActivityTestRule mTestRule = new ChromeTabbedActivityTestRule();
    private final AutofillAssistantChromeTabTestRule mTabTestRule =
            new AutofillAssistantChromeTabTestRule(mTestRule, TEST_PAGE_A);

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(mTabTestRule);

    private String getURL(String page) {
        return mTabTestRule.getURL(page);
    }

    private void startAutofillAssistantOnTab(
            String pageToLoad, AutofillAssistantTestScript... scripts) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Arrays.asList(scripts));
        startAutofillAssistant(mTestRule.getActivity(), testService, getURL(pageToLoad));
    }

    @Test
    @MediumTest
    public void navigatingWithLocationBarShowsError() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        startAutofillAssistantOnTab(TEST_PAGE_A, script);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Committing URL shows error.
        onView(withId(org.chromium.chrome.R.id.url_bar))
                .perform(click(), typeText(getURL(TEST_PAGE_B)), pressImeActionButton());
        waitUntilViewMatchesCondition(withText(containsString("Sorry")), isCompletelyDisplayed());
        waitUntil(
                ()
                        -> ChromeTabUtils.getUrlOnUiThread(mTestRule.getActivity().getActivityTab())
                                   .getSpec()
                                   .equals(getURL(TEST_PAGE_B)));
    }

    @Test
    @MediumTest
    public void clickingLinkDoesNotCauseError() {
        ArrayList<ActionProto> list = new ArrayList<>();
        addTapSteps(toCssSelector("#form_target_website_link"), list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        startAutofillAssistantOnTab(TEST_PAGE_A, script);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        waitUntil(
                ()
                        -> ChromeTabUtils.getUrlOnUiThread(mTestRule.getActivity().getActivityTab())
                                   .getSpec()
                                   .equals(getURL(TEST_PAGE_B)));
    }

    @Test
    @MediumTest
    public void javaScriptNavigationDoesNotCauseError() {
        ArrayList<ActionProto> list = new ArrayList<>();
        addTapSteps(toCssSelector("#form_target_navigation_action"), list);
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        startAutofillAssistantOnTab(TEST_PAGE_A, script);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        waitUntil(
                ()
                        -> ChromeTabUtils.getUrlOnUiThread(mTestRule.getActivity().getActivityTab())
                                   .getSpec()
                                   .equals(getURL(TEST_PAGE_B)));
    }

    @Test
    @MediumTest
    public void navigateActionDoesNotCauseError() {
        // Push something to navigation stack so we can use back and forth.
        ChromeTabUtils.loadUrlOnUiThread(
                mTestRule.getActivity().getActivityTab(), getURL(TEST_PAGE_B));

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Page B").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Navigate"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setNavigate(NavigateProto.newBuilder().setUrl(getURL(TEST_PAGE_A)))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setWaitForNavigation(WaitForNavigationProto.newBuilder())
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Page A").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Go back"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setNavigate(NavigateProto.newBuilder().setGoBackward(true))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setWaitForNavigation(WaitForNavigationProto.newBuilder())
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Page B").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Go forward"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setNavigate(NavigateProto.newBuilder().setGoForward(true))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setWaitForNavigation(WaitForNavigationProto.newBuilder())
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Page A").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_B)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        startAutofillAssistantOnTab(TEST_PAGE_B, script);

        waitUntilViewMatchesCondition(withText("Page B"), isCompletelyDisplayed());
        onView(withText("Navigate")).perform(click());

        waitUntilViewMatchesCondition(withText("Page A"), isCompletelyDisplayed());

        waitUntil(
                ()
                        -> ChromeTabUtils.getUrlOnUiThread(mTestRule.getActivity().getActivityTab())
                                   .getSpec()
                                   .equals(getURL(TEST_PAGE_A)));
        onView(withText("Go back")).perform(click());

        waitUntilViewMatchesCondition(withText("Page B"), isCompletelyDisplayed());
        waitUntil(
                ()
                        -> ChromeTabUtils.getUrlOnUiThread(mTestRule.getActivity().getActivityTab())
                                   .getSpec()
                                   .equals(getURL(TEST_PAGE_B)));
        onView(withText("Go forward")).perform(click());

        waitUntilViewMatchesCondition(withText("Page A"), isCompletelyDisplayed());
        waitUntil(
                ()
                        -> ChromeTabUtils.getUrlOnUiThread(mTestRule.getActivity().getActivityTab())
                                   .getSpec()
                                   .equals(getURL(TEST_PAGE_A)));
    }

    @Test
    @MediumTest
    public void navigatingInStoppedAutofillAssistantStateRemovesUI() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setTell(TellProto.newBuilder().setMessage("Shutdown"))
                         .build());
        list.add(ActionProto.newBuilder().setStop(StopProto.newBuilder()).build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        startAutofillAssistantOnTab(TEST_PAGE_A, script);

        waitUntilViewMatchesCondition(withText("Shutdown"), isCompletelyDisplayed());

        onView(withId(org.chromium.chrome.R.id.url_bar))
                .perform(click(), typeText(getURL(TEST_PAGE_B)));
        onView(withId(org.chromium.chrome.R.id.url_bar)).perform(pressImeActionButton());
        waitUntilViewAssertionTrue(
                withId(R.id.autofill_assistant), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
    }
}
