// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientSettingsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientSettingsProto.BackButtonSettings;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientSettingsProto.DisplayString;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientSettingsProto.DisplayStringId;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientSettingsProto.IntegrationTestSettings;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientSettingsProto.SlowWarningSettings;
import org.chromium.chrome.browser.autofill_assistant.proto.FormInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.FormProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InfoPopupProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectionInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowFormProto;
import org.chromium.chrome.browser.autofill_assistant.proto.StopProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TellProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UpdateClientSettingsProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Tests autofill assistant when using UpdateClientSettingsAction.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public final class AutofillAssistantUpdateClientSettingsIntegrationTest {
    private static final String HTML_DIRECTORY = "/components/test/data/autofill_assistant/html/";
    private static final String TEST_PAGE_A = "autofill_assistant_target_website.html";
    private static final String TEST_PAGE_B = "form_target_website.html";

    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(
            new AutofillAssistantCustomTabTestRule(mTestRule, TEST_PAGE_A));

    private String getTargetWebsiteUrl(String testPage) {
        return mTestRule.getTestServer().getURL(HTML_DIRECTORY + testPage);
    }

    private ClientSettingsProto getValidClientSettingsForLocale(String locale) {
        ClientSettingsProto.Builder clientSettingsBuilder = ClientSettingsProto.newBuilder();
        clientSettingsBuilder
                .setIntegrationTestSettings(IntegrationTestSettings.newBuilder()
                                                    .setDisableHeaderAnimations(true)
                                                    .setDisableCarouselChangeAnimations(true))
                .setDisplayStringsLocale(locale)
                .setBackButtonSettings(BackButtonSettings.newBuilder()
                                               .setMessage(locale + " back_button_message")
                                               .setUndoLabel(locale + " undo"))
                .setSlowWarningSettings(
                        SlowWarningSettings.newBuilder()
                                .setSlowConnectionMessage(locale + " slow_connection_message")
                                .setSlowWebsiteMessage(locale + "slow_website_message"));
        for (ClientSettingsProto.DisplayStringId id :
                ClientSettingsProto.DisplayStringId.values()) {
            if (id == DisplayStringId.UNSPECIFIED) {
                continue;
            }
            clientSettingsBuilder.addDisplayStrings(
                    DisplayString.newBuilder().setId(id).setValue(locale + " " + id.name()));
        }
        return clientSettingsBuilder.build();
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1263730")
    public void backButtonFollowedByGiveUp() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setUpdateClientSettings(
                                 UpdateClientSettingsProto.newBuilder().setClientSettings(
                                         getValidClientSettingsForLocale("de-ZH")))
                         .build());

        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder()
                                                 .setType(ChipType.HIGHLIGHTED_ACTION)
                                                 .setText("de-ZH Continue"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("de-ZH Continue"), isCompletelyDisplayed());
        onView(withId(R.id.profile_image)).perform(click());
        waitUntilViewMatchesCondition(
                withText("de-ZH " + DisplayStringId.SETTINGS.name()), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(
                withText("de-ZH " + DisplayStringId.SEND_FEEDBACK.name()), isCompletelyDisplayed());

        // First press on back button closes the profile icon menu.
        Espresso.pressBack();

        // Second press on back button shows back button error messages.
        Espresso.pressBack();
        waitUntilViewMatchesCondition(
                withText("de-ZH back_button_message"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(withText("de-ZH undo"), isCompletelyDisplayed());
        onView(withText("de-ZH undo")).perform(click());
        waitUntilViewMatchesCondition(withText("de-ZH Continue"), isCompletelyDisplayed());

        // Check if |GIVE_UP| is displayed.
        mTestRule.loadUrl(getTargetWebsiteUrl(TEST_PAGE_B));
        waitUntilViewMatchesCondition(
                withText("de-ZH " + DisplayStringId.GIVE_UP.name()), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    public void checkingFeedbackWithUpdateClientSettings() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setUpdateClientSettings(
                                 UpdateClientSettingsProto.newBuilder().setClientSettings(
                                         getValidClientSettingsForLocale("de-ZH")))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setTell(TellProto.newBuilder().setMessage("de-ZH Shutdown"))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setStop(StopProto.newBuilder().setShowFeedbackChip(true))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("de-ZH Shutdown"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(
                withText("de-ZH " + DisplayStringId.SEND_FEEDBACK.name()), isDisplayed());
    }

    @Test
    @MediumTest
    public void checkingStoppedWithUpdateClientSettings() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setUpdateClientSettings(
                                 UpdateClientSettingsProto.newBuilder().setClientSettings(
                                         getValidClientSettingsForLocale("de-ZH")))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder()
                                                 .setType(ChipType.CANCEL_ACTION)
                                                 .setText("de-ZH Cancel"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("de-ZH Cancel"), isCompletelyDisplayed());
        onView(withText("de-ZH Cancel")).perform(click());
        waitUntilViewMatchesCondition(
                withText("de-ZH " + DisplayStringId.STOPPED.name()), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(
                withText("de-ZH " + DisplayStringId.UNDO.name()), isDisplayed());
    }

    @Test
    @MediumTest
    public void formInfoPopupWithLocalizedCloseButton() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setUpdateClientSettings(
                                 UpdateClientSettingsProto.newBuilder().setClientSettings(
                                         getValidClientSettingsForLocale("de-ZH")))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setShowForm(ShowFormProto.newBuilder().setForm(
                                 FormProto.newBuilder()
                                         .setInfoLabel("de-ZH Info")
                                         .setInfoPopup(InfoPopupProto.newBuilder()
                                                               .setText("de-ZH info_message")
                                                               .setTitle("de-ZH info_title"))
                                         .addInputs(FormInputProto.newBuilder().setSelection(
                                                 SelectionInputProto.newBuilder().addChoices(
                                                         SelectionInputProto.Choice.newBuilder()
                                                                 .setLabel("de-ZH Choice"))))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        // Continue chip in Form.
        waitUntilViewMatchesCondition(
                withText("de-ZH " + DisplayStringId.PAYMENT_INFO_CONFIRM.name()),
                isCompletelyDisplayed());
        onView(withId(R.id.info_button)).perform(click());
        waitUntilViewMatchesCondition(withText("de-ZH info_message"), isCompletelyDisplayed());
        onView(withText("de-ZH " + DisplayStringId.CLOSE.name())).perform(click());
    }
}
