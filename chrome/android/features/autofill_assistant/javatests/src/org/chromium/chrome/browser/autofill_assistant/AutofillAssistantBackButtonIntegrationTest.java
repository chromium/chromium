// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilKeyboardMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto.Rectangle;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowCastProto;
import org.chromium.chrome.browser.autofill_assistant.proto.StopProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TellProto;
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
public class AutofillAssistantBackButtonIntegrationTest {
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
        startAutofillAssistant(mTestRule.getActivity(),
                new AutofillAssistantTestService(Arrays.asList(scripts)), getURL(pageToLoad));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1220426")
    public void backButtonWithSnackbarDestroysAutofillAssistantUi() throws Exception {
        ChromeTabUtils.loadUrlOnUiThread(
                mTestRule.getActivity().getActivityTab(), getURL(TEST_PAGE_B));

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
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        startAutofillAssistantOnTab(TEST_PAGE_B, script);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // Force the keyboard to open.
        tapElement(mTestRule, "profile_name");
        waitUntilKeyboardMatchesCondition(mTestRule, /* isShowing= */ true);

        // First press on back button closes the keyboard.
        Espresso.pressBack();
        waitUntilKeyboardMatchesCondition(mTestRule, /* isShowing= */ false);
        onView(withText("Prompt")).check(matches(isCompletelyDisplayed()));

        // Second press on back button destroys Autofill Assistant UI.
        Espresso.pressBack();
        waitUntilViewMatchesCondition(withText(R.string.undo), isCompletelyDisplayed());
        onView(withId(R.id.autofill_assistant)).check(doesNotExist());

        assertThat(
                ChromeTabUtils.getUrlOnUiThread(mTestRule.getActivity().getActivityTab()).getSpec(),
                is(getURL(TEST_PAGE_B)));

        // Third press on back button navigates back.
        Espresso.pressBack();
        waitUntil(
                ()
                        -> ChromeTabUtils.getUrlOnUiThread(mTestRule.getActivity().getActivityTab())
                                   .getSpec()
                                   .equals(getURL(TEST_PAGE_A)));
    }

    @Test
    @MediumTest
    public void backButtonInStoppedAutofillAssistantState() {
        ChromeTabUtils.loadUrlOnUiThread(
                mTestRule.getActivity().getActivityTab(), getURL(TEST_PAGE_B));

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
        startAutofillAssistantOnTab(TEST_PAGE_B, script);

        waitUntilViewMatchesCondition(withText("Shutdown"), isCompletelyDisplayed());

        // First press on back button fully destroys Autofill Assistant UI, without Undo and
        // navigates.
        Espresso.pressBack();
        waitUntilViewAssertionTrue(
                withId(R.id.autofill_assistant), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        onView(withText("Shutdown")).check(doesNotExist());
        onView(withText(R.string.undo)).check(doesNotExist());
        waitUntil(
                ()
                        -> ChromeTabUtils.getUrlOnUiThread(mTestRule.getActivity().getActivityTab())
                                   .getSpec()
                                   .equals(getURL(TEST_PAGE_A)));
    }

    @Test
    @MediumTest
    public void backButtonIsIgnoredInBrowseMode() {
        // Same domain, different page, such that navigating back is allowed in the BROWSE state.
        ChromeTabUtils.loadUrlOnUiThread(
                mTestRule.getActivity().getActivityTab(), getURL(TEST_PAGE_B));

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Prompt")
                                            .setBrowseMode(true)
                                            .addChoices(PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        startAutofillAssistantOnTab(TEST_PAGE_B, script);

        // BROWSE state must not automatically collapse the UI.
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        // First press on back button collapses Autofill Assistant UI.
        Espresso.pressBack();
        waitUntilViewMatchesCondition(
                withId(R.id.status_message), allOf(withText("Prompt"), not(isDisplayed())));
        onView(withId(R.id.autofill_assistant)).check(matches(isDisplayed()));

        // Second press on back button navigates back, without removing the Autofill Assistannt UI.
        Espresso.pressBack();
        waitUntil(
                ()
                        -> ChromeTabUtils.getUrlOnUiThread(mTestRule.getActivity().getActivityTab())
                                   .getSpec()
                                   .equals(getURL(TEST_PAGE_A)));
        onView(withId(R.id.autofill_assistant)).check(matches(isDisplayed()));
        onView(withId(R.id.status_message)).check(matches(withText("Prompt")));
    }
}
