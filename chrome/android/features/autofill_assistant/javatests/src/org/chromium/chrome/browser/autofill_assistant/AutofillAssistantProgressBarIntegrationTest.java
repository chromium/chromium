// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.hasTintColor;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.test.util.ViewUtils.hasBackgroundColor;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DrawableProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DrawableProto.Icon;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowProgressBarProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowProgressBarProto.StepProgressBarConfiguration;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowProgressBarProto.StepProgressBarIcon;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.widget.ChromeImageView;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Locale;

/**
 * Tests autofill assistant's progress bar.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantProgressBarIntegrationTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "form_target_website.html";

    private void runScript(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(
                AutofillAssistantUiTestUtil.createMinimalCustomTabIntentForAutobot(
                        mTestRule.getTestServer().getURL(TEST_PAGE),
                        /* startImmediately = */ true));
    }

    private StepProgressBarConfiguration getDefaultStepProgressBarConfiguration() {
        return (StepProgressBarConfiguration) StepProgressBarConfiguration.newBuilder()
                .setUseStepProgressBar(true)
                .addAnnotatedStepIcons(StepProgressBarIcon.newBuilder()
                                               .setIcon(DrawableProto.newBuilder().setIcon(
                                                       Icon.PROGRESSBAR_DEFAULT_INITIAL_STEP))
                                               .setIdentifier("icon_1"))
                .addAnnotatedStepIcons(StepProgressBarIcon.newBuilder()
                                               .setIcon(DrawableProto.newBuilder().setIcon(
                                                       Icon.PROGRESSBAR_DEFAULT_DATA_COLLECTION))
                                               .setIdentifier("icon_2"))
                .addAnnotatedStepIcons(StepProgressBarIcon.newBuilder()
                                               .setIcon(DrawableProto.newBuilder().setIcon(
                                                       Icon.PROGRESSBAR_DEFAULT_PAYMENT))
                                               .setIdentifier("icon_3"))
                .addAnnotatedStepIcons(StepProgressBarIcon.newBuilder()
                                               .setIcon(DrawableProto.newBuilder().setIcon(
                                                       Icon.PROGRESSBAR_DEFAULT_FINAL_STEP))
                                               .setIdentifier("icon_4"))
                .build();
    }

    @Test
    @MediumTest
    public void testSwitchingProgressBar() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Initial Progress")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(ShowProgressBarProto.newBuilder().setProgress(10))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("More Progress")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(
                                 ShowProgressBarProto.newBuilder().setStepProgressBarConfiguration(
                                         getDefaultStepProgressBarConfiguration()))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Step Progress")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(ShowProgressBarProto.newBuilder().setActiveStep(1))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Another Step")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(withText("Initial Progress"), isCompletelyDisplayed());
        onView(withId(R.id.progress_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.step_progress_bar)).check(matches(not(isDisplayed())));
        onView(withText("Next")).perform(click());

        waitUntilViewMatchesCondition(withText("More Progress"), isCompletelyDisplayed());
        onView(withId(R.id.progress_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.step_progress_bar)).check(matches(not(isDisplayed())));
        onView(withText("Next")).perform(click());

        waitUntilViewMatchesCondition(withText("Step Progress"), isCompletelyDisplayed());
        onView(withId(R.id.progress_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.step_progress_bar)).check(matches(isDisplayed()));
        onView(withText("Next")).perform(click());

        waitUntilViewMatchesCondition(withText("Another Step"), isCompletelyDisplayed());
        onView(withId(R.id.progress_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.step_progress_bar)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testStepProgressBar() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(
                                 ShowProgressBarProto.newBuilder().setStepProgressBarConfiguration(
                                         getDefaultStepProgressBarConfiguration()))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Initial Step")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setShowProgressBar(
                                ShowProgressBarProto.newBuilder().setActiveStepIdentifier("icon_2"))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Next Step")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(ShowProgressBarProto.newBuilder().setActiveStep(4))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Final Step")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(withText("Initial Step"), isCompletelyDisplayed());
        for (int i = 0; i < 4; ++i) {
            onView(withTagValue(is(String.format(
                           Locale.getDefault(), AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i))))
                    .check(matches(isDisplayed()));
            onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                                 AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i)))),
                           withClassName(is(ChromeImageView.class.getName()))))
                    .check(matches(allOf(
                            not(isEnabled()), hasTintColor(R.color.modern_grey_800_alpha_38))));
        }
        for (int i = 0; i < 3; ++i) {
            onView(withTagValue(is(String.format(
                           Locale.getDefault(), AssistantTagsForTesting.PROGRESSBAR_LINE_TAG, i))))
                    .check(matches(isDisplayed()));
        }
        onView(withText("Next")).perform(click());

        waitUntilViewMatchesCondition(withText("Next Step"), isCompletelyDisplayed());
        onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                             AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, 0)))),
                       withClassName(is(ChromeImageView.class.getName()))))
                .check(matches(allOf(isEnabled(), hasTintColor(R.color.modern_blue_600))));
        onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                             AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, 1)))),
                       withClassName(is(ChromeImageView.class.getName()))))
                .check(matches(
                        allOf(not(isEnabled()), hasTintColor(R.color.modern_grey_800_alpha_38))));
        onView(withText("Next")).perform(click());

        waitUntilViewMatchesCondition(withText("Final Step"), isCompletelyDisplayed());
        for (int i = 0; i < 4; ++i) {
            onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                                 AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i)))),
                           withClassName(is(ChromeImageView.class.getName()))))
                    .check(matches(allOf(isEnabled(), hasTintColor(R.color.modern_blue_600))));
        }
    }

    @Test
    @MediumTest
    public void testStepProgressBarError() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(
                                 ShowProgressBarProto.newBuilder().setStepProgressBarConfiguration(
                                         getDefaultStepProgressBarConfiguration()))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Initial Step")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(ShowProgressBarProto.newBuilder().setActiveStep(1))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Next Step")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(
                                 ShowProgressBarProto.newBuilder().setActiveStep(3).setErrorState(
                                         true))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Final Step")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(withText("Initial Step"), isCompletelyDisplayed());
        for (int i = 0; i < 4; ++i) {
            onView(withTagValue(is(String.format(
                           Locale.getDefault(), AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i))))
                    .check(matches(isDisplayed()));
            onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                                 AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i)))),
                           withClassName(is(ChromeImageView.class.getName()))))
                    .check(matches(allOf(
                            not(isEnabled()), hasTintColor(R.color.modern_grey_800_alpha_38))));
        }
        for (int i = 0; i < 3; ++i) {
            onView(withTagValue(is(String.format(
                           Locale.getDefault(), AssistantTagsForTesting.PROGRESSBAR_LINE_TAG, i))))
                    .check(matches(isDisplayed()));
        }
        onView(withText("Next")).perform(click());

        waitUntilViewMatchesCondition(withText("Next Step"), isCompletelyDisplayed());
        onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                             AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, 0)))),
                       withClassName(is(ChromeImageView.class.getName()))))
                .check(matches(allOf(isEnabled(), hasTintColor(R.color.modern_blue_600))));
        onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                             AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, 1)))),
                       withClassName(is(ChromeImageView.class.getName()))))
                .check(matches(
                        allOf(not(isEnabled()), hasTintColor(R.color.modern_grey_800_alpha_38))));
        onView(withText("Next")).perform(click());

        waitUntilViewMatchesCondition(withText("Final Step"), isCompletelyDisplayed());
        for (int i = 0; i < 3; ++i) {
            onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                                 AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i)))),
                           withClassName(is(ChromeImageView.class.getName()))))
                    .check(matches(allOf(isEnabled(), hasTintColor(R.color.modern_blue_600))));
        }
        onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                             AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, 3)))),
                       withClassName(is(ChromeImageView.class.getName()))))
                .check(matches(allOf(not(isEnabled()), hasTintColor(R.color.default_red))));
    }

    @Test
    @MediumTest
    public void testStepProgressBarErrorOnlyAction() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(
                                 ShowProgressBarProto.newBuilder().setStepProgressBarConfiguration(
                                         getDefaultStepProgressBarConfiguration()))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Initial Step")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(ShowProgressBarProto.newBuilder().setActiveStep(3))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(ShowProgressBarProto.newBuilder().setErrorState(true))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Error State")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(withText("Initial Step"), isCompletelyDisplayed());
        for (int i = 0; i < 4; ++i) {
            onView(withTagValue(is(String.format(
                           Locale.getDefault(), AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i))))
                    .check(matches(isDisplayed()));
            onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                                 AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i)))),
                           withClassName(is(ChromeImageView.class.getName()))))
                    .check(matches(allOf(
                            not(isEnabled()), hasTintColor(R.color.modern_grey_800_alpha_38))));
        }
        for (int i = 0; i < 3; ++i) {
            onView(withTagValue(is(String.format(
                           Locale.getDefault(), AssistantTagsForTesting.PROGRESSBAR_LINE_TAG, i))))
                    .check(matches(isDisplayed()));
        }
        onView(withText("Next")).perform(click());

        waitUntilViewMatchesCondition(withText("Error State"), isCompletelyDisplayed());
        for (int i = 0; i < 3; ++i) {
            onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                                 AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i)))),
                           withClassName(is(ChromeImageView.class.getName()))))
                    .check(matches(allOf(isEnabled(), hasTintColor(R.color.modern_blue_600))));
        }
        onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                             AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, 3)))),
                       withClassName(is(ChromeImageView.class.getName()))))
                .check(matches(allOf(not(isEnabled()), hasTintColor(R.color.default_red))));
    }

    @Test
    @MediumTest
    public void testStepProgressBarErrorAfterCompletion() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(
                                 ShowProgressBarProto.newBuilder().setStepProgressBarConfiguration(
                                         getDefaultStepProgressBarConfiguration()))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Initial Step")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(ShowProgressBarProto.newBuilder().setActiveStep(1))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Next Step")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(
                                 ShowProgressBarProto.newBuilder().setActiveStep(4).setErrorState(
                                         true))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Final Step")
                                            .addChoices(Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder().setText("Next"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(withText("Initial Step"), isCompletelyDisplayed());
        for (int i = 0; i < 4; ++i) {
            onView(withTagValue(is(String.format(
                           Locale.getDefault(), AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i))))
                    .check(matches(isDisplayed()));
            onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                                 AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i)))),
                           withClassName(is(ChromeImageView.class.getName()))))
                    .check(matches(allOf(
                            not(isEnabled()), hasTintColor(R.color.modern_grey_800_alpha_38))));
        }
        for (int i = 0; i < 3; ++i) {
            onView(withTagValue(is(String.format(
                           Locale.getDefault(), AssistantTagsForTesting.PROGRESSBAR_LINE_TAG, i))))
                    .check(matches(isDisplayed()));
        }
        onView(withText("Next")).perform(click());

        waitUntilViewMatchesCondition(withText("Next Step"), isCompletelyDisplayed());
        onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                             AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, 0)))),
                       withClassName(is(ChromeImageView.class.getName()))))
                .check(matches(allOf(isEnabled(), hasTintColor(R.color.modern_blue_600))));
        onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                             AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, 1)))),
                       withClassName(is(ChromeImageView.class.getName()))))
                .check(matches(
                        allOf(not(isEnabled()), hasTintColor(R.color.modern_grey_800_alpha_38))));
        onView(withText("Next")).perform(click());

        waitUntilViewMatchesCondition(withText("Final Step"), isCompletelyDisplayed());
        for (int i = 0; i < 3; ++i) {
            onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                                 AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, i)))),
                           withClassName(is(ChromeImageView.class.getName()))))
                    .check(matches(allOf(isEnabled(), hasTintColor(R.color.default_red))));
        }
        for (int i = 0; i < 3; ++i) {
            onView(withTagValue(is(String.format(
                           Locale.getDefault(), AssistantTagsForTesting.PROGRESSBAR_LINE_TAG, i))))
                    .check(matches(isEnabled()));
            onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                                 AssistantTagsForTesting.PROGRESSBAR_LINE_TAG, i)))),
                           withTagValue(
                                   is(AssistantTagsForTesting.PROGRESSBAR_LINE_FOREGROUND_TAG))))
                    .check(matches(hasBackgroundColor(R.color.default_red)));
        }
    }

    @Test
    @MediumTest
    public void updatingIconsRestoresActiveState() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(
                                 ShowProgressBarProto.newBuilder().setStepProgressBarConfiguration(
                                         StepProgressBarConfiguration.newBuilder()
                                                 .setUseStepProgressBar(true)))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(ShowProgressBarProto.newBuilder().setActiveStep(1))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Update"))))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowProgressBar(
                                 ShowProgressBarProto.newBuilder().setStepProgressBarConfiguration(
                                         getDefaultStepProgressBarConfiguration()))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Updated").addChoices(
                                 Choice.newBuilder().setChip(ChipProto.newBuilder())))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                             AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, 0)))),
                       withClassName(is(ChromeImageView.class.getName()))))
                .check(matches(allOf(isEnabled(), hasTintColor(R.color.modern_blue_600))));
        onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                             AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, 1)))),
                       withClassName(is(ChromeImageView.class.getName()))))
                .check(matches(
                        allOf(not(isEnabled()), hasTintColor(R.color.modern_grey_800_alpha_38))));

        onView(withText("Update")).perform(click());
        waitUntilViewMatchesCondition(withText("Updated"), isCompletelyDisplayed());
        onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                             AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, 0)))),
                       withClassName(is(ChromeImageView.class.getName()))))
                .check(matches(allOf(isEnabled(), hasTintColor(R.color.modern_blue_600))));
        onView(allOf(isDescendantOfA(withTagValue(is(String.format(Locale.getDefault(),
                             AssistantTagsForTesting.PROGRESSBAR_ICON_TAG, 1)))),
                       withClassName(is(ChromeImageView.class.getName()))))
                .check(matches(
                        allOf(not(isEnabled()), hasTintColor(R.color.modern_grey_800_alpha_38))));
    }
}
