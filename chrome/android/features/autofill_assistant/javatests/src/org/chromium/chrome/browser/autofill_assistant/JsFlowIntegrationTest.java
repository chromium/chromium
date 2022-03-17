// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.iterableWithSize;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementExists;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addClickSteps;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;

import android.support.test.InstrumentationRegistry;
import android.util.Base64;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantTestService.ScriptsReturnMode;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureUiStateProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureUiStateProto.OverlayBehavior;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.JsFlowProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionStatusProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.ScriptPreconditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.StopProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TellProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.AutofillAssistantPreferencesUtil;
import org.chromium.components.autofill_assistant.R;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests JS flows.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class JsFlowIntegrationTest {
    private static final String HTML_DIRECTORY = "/components/test/data/autofill_assistant/html/";
    private static final String TEST_PAGE = "autofill_assistant_target_website.html";
    private static final String MAIN_SCRIPT_PATH = "main_script";
    private static final String INTERRUPT_SCRIPT_PATH = "interrupt_script";
    private static final SupportedScriptProto TEST_SCRIPT =
            SupportedScriptProto.newBuilder()
                    .setPath(MAIN_SCRIPT_PATH)
                    .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                    .build();

    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private String getTargetWebsiteUrl(String testPage) {
        return mTestRule.getTestServer().getURL(HTML_DIRECTORY + testPage);
    }

    private AutofillAssistantTestService runScript(AutofillAssistantTestScript script) {
        return runScript(Collections.singletonList(script));
    }

    private AutofillAssistantTestService runScript(List<AutofillAssistantTestScript> scripts) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(scripts, ScriptsReturnMode.ALL_AT_ONCE);
        startAutofillAssistant(mTestRule.getActivity(), testService);
        return testService;
    }

    @Before
    public void setUp() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), getTargetWebsiteUrl(TEST_PAGE)));
    }

    private byte[] getActionBytes(ActionProto action) {
        switch (action.getActionInfoCase()) {
            case SELECT_OPTION:
                return action.getSelectOption().toByteArray();
            case NAVIGATE:
                return action.getNavigate().toByteArray();
            case PROMPT:
                return action.getPrompt().toByteArray();
            case TELL:
                return action.getTell().toByteArray();
            case SHOW_CAST:
                return action.getShowCast().toByteArray();
            case WAIT_FOR_DOM:
                return action.getWaitForDom().toByteArray();
            case USE_CARD:
                return action.getUseCard().toByteArray();
            case USE_ADDRESS:
                return action.getUseAddress().toByteArray();
            case UPLOAD_DOM:
                return action.getUploadDom().toByteArray();
            case SHOW_PROGRESS_BAR:
                return action.getShowProgressBar().toByteArray();
            case SHOW_DETAILS:
                return action.getShowDetails().toByteArray();
            case STOP:
                return action.getStop().toByteArray();
            case COLLECT_USER_DATA:
                return action.getCollectUserData().toByteArray();
            case SET_ATTRIBUTE:
                return action.getSetAttribute().toByteArray();
            case SHOW_INFO_BOX:
                return action.getShowInfoBox().toByteArray();
            case EXPECT_NAVIGATION:
                return action.getExpectNavigation().toByteArray();
            case WAIT_FOR_NAVIGATION:
                return action.getWaitForNavigation().toByteArray();
            case CONFIGURE_BOTTOM_SHEET:
                return action.getConfigureBottomSheet().toByteArray();
            case SHOW_FORM:
                return action.getShowForm().toByteArray();
            case POPUP_MESSAGE:
                return action.getPopupMessage().toByteArray();
            case WAIT_FOR_DOCUMENT:
                return action.getWaitForDocument().toByteArray();
            case SHOW_GENERIC_UI:
                return action.getShowGenericUi().toByteArray();
            case GENERATE_PASSWORD_FOR_FORM_FIELD:
                return action.getGeneratePasswordForFormField().toByteArray();
            case SAVE_GENERATED_PASSWORD:
                return action.getSaveGeneratedPassword().toByteArray();
            case CONFIGURE_UI_STATE:
                return action.getConfigureUiState().toByteArray();
            case PRESAVE_GENERATED_PASSWORD:
                return action.getPresaveGeneratedPassword().toByteArray();
            case GET_ELEMENT_STATUS:
                return action.getGetElementStatus().toByteArray();
            case SCROLL_INTO_VIEW:
                return action.getScrollIntoView().toByteArray();
            case WAIT_FOR_DOCUMENT_TO_BECOME_INTERACTIVE:
                return action.getWaitForDocumentToBecomeInteractive().toByteArray();
            case WAIT_FOR_DOCUMENT_TO_BECOME_COMPLETE:
                return action.getWaitForDocumentToBecomeComplete().toByteArray();
            case SEND_CLICK_EVENT:
                return action.getSendClickEvent().toByteArray();
            case SEND_TAP_EVENT:
                return action.getSendTapEvent().toByteArray();
            case JS_CLICK:
                return action.getJsClick().toByteArray();
            case SEND_KEYSTROKE_EVENTS:
                return action.getSendKeystrokeEvents().toByteArray();
            case SEND_CHANGE_EVENT:
                return action.getSendChangeEvent().toByteArray();
            case SET_ELEMENT_ATTRIBUTE:
                return action.getSetElementAttribute().toByteArray();
            case SELECT_FIELD_VALUE:
                return action.getSelectFieldValue().toByteArray();
            case FOCUS_FIELD:
                return action.getFocusField().toByteArray();
            case WAIT_FOR_ELEMENT_TO_BECOME_STABLE:
                return action.getWaitForElementToBecomeStable().toByteArray();
            case CHECK_ELEMENT_IS_ON_TOP:
                return action.getCheckElementIsOnTop().toByteArray();
            case RELEASE_ELEMENTS:
                return action.getReleaseElements().toByteArray();
            case DISPATCH_JS_EVENT:
                return action.getDispatchJsEvent().toByteArray();
            case SEND_KEY_EVENT:
                return action.getSendKeyEvent().toByteArray();
            case SELECT_OPTION_ELEMENT:
                return action.getSelectOptionElement().toByteArray();
            case CHECK_ELEMENT_TAG:
                return action.getCheckElementTag().toByteArray();
            case CHECK_OPTION_ELEMENT:
                return action.getCheckOptionElement().toByteArray();
            case SET_PERSISTENT_UI:
                return action.getSetPersistentUi().toByteArray();
            case CLEAR_PERSISTENT_UI:
                return action.getClearPersistentUi().toByteArray();
            case SCROLL_INTO_VIEW_IF_NEEDED:
                return action.getScrollIntoViewIfNeeded().toByteArray();
            case SCROLL_WINDOW:
                return action.getScrollWindow().toByteArray();
            case SCROLL_CONTAINER:
                return action.getScrollContainer().toByteArray();
            case SET_TOUCHABLE_AREA:
                return action.getSetTouchableArea().toByteArray();
            case DELETE_PASSWORD:
                return action.getDeletePassword().toByteArray();
            case EDIT_PASSWORD:
                return action.getEditPassword().toByteArray();
            case BLUR_FIELD:
                return action.getBlurField().toByteArray();
            case RESET_PENDING_CREDENTIALS:
                return action.getResetPendingCredentials().toByteArray();
            case SAVE_SUBMITTED_PASSWORD:
                return action.getSaveSubmittedPassword().toByteArray();
            case UPDATE_CLIENT_SETTINGS:
                return action.getUpdateClientSettings().toByteArray();
            case JS_FLOW:
                return action.getJsFlow().toByteArray();
            case EXECUTE_JS:
                return action.getExecuteJs().toByteArray();
            default:
                assert false : "Unhandled action case, please add above!";
                return null;
        }
    }

    /**
     * Returns a JS flow action that will execute all actions in {@code actions} consecutively.
     */
    private ActionProto toJsFlowAction(List<ActionProto> actions) {
        assert (!actions.isEmpty());
        StringBuilder jsFlow = new StringBuilder();
        for (int i = 0; i < actions.size(); i++) {
            jsFlow.append("[client_status, value] = await runNativeAction(")
                    .append(actions.get(i).getActionInfoCase().getNumber())
                    .append(", '")
                    .append(Base64.encodeToString(getActionBytes(actions.get(i)), Base64.NO_WRAP))
                    .append("');\nif (client_status != 2) { return {status:client_status}; }\n");
        }
        jsFlow.append("return {status:client_status};");
        return ActionProto.newBuilder()
                .setJsFlow(JsFlowProto.newBuilder().setJsFlow(jsFlow.toString()))
                .build();
    }

    @Test
    @MediumTest
    public void runSimpleJsFlowWithNativeActions() throws Exception {
        // Clicking #touch_area_one will make it disappear, which makes for a convenient test.
        ArrayList<ActionProto> nestedActions = new ArrayList<>();
        addClickSteps(toCssSelector("#touch_area_one"), nestedActions);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(toJsFlowAction(nestedActions));
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(Choice.newBuilder().setChip(
                                 ChipProto.newBuilder()
                                         .setType(ChipType.HIGHLIGHTED_ACTION)
                                         .setText("After JS flow"))))
                         .build());

        Assert.assertTrue(checkElementExists(mTestRule.getWebContents(), "touch_area_one"));

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        runScript(script);

        waitUntilViewMatchesCondition(withText("After JS flow"), isCompletelyDisplayed());
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));
    }

    @Test
    @MediumTest
    public void runSimpleJsFlowWithReturnValue() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        String jsFlow = "return {"
                + "status:3,"
                + "result:[[1,2], null, {enum:5}]"
                + "}";
        list.add(ActionProto.newBuilder()
                         .setJsFlow(JsFlowProto.newBuilder().setJsFlow(jsFlow))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(Choice.newBuilder().setChip(
                                 ChipProto.newBuilder()
                                         .setType(ChipType.HIGHLIGHTED_ACTION)
                                         .setText("Should not run because JS flow returned "
                                                 + "OTHER_ACTION_STATUS != ACTION_APPLIED"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        AutofillAssistantTestService testService = runScript(script);

        testService.waitUntilGetNextActions(1);
        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(processedActions.get(0).getStatus(),
                is(ProcessedActionStatusProto.OTHER_ACTION_STATUS));
        // Note that the JS flow returns a JS object (no quotes around 'enum'), but the action
        // returns the JSON representation of that object (with quotes around 'enum').
        assertThat(processedActions.get(0).getJsFlowResult().getResultJson(),
                is("[[1,2],null,{\"enum\":5}]"));
    }

    @Test
    @MediumTest
    public void stopFlowFromJs() throws Exception {
        ArrayList<ActionProto> nestedActions = new ArrayList<>();
        nestedActions.add(ActionProto.newBuilder()
                                  .setTell(TellProto.newBuilder().setMessage("Bye!"))
                                  .build());
        nestedActions.add(ActionProto.newBuilder().setStop(StopProto.newBuilder()).build());

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(Choice.newBuilder().setChip(
                                 ChipProto.newBuilder()
                                         .setType(ChipType.HIGHLIGHTED_ACTION)
                                         .setText("Stop"))))
                         .build());
        list.add(toJsFlowAction(nestedActions));
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(Choice.newBuilder().setChip(
                                 ChipProto.newBuilder()
                                         .setType(ChipType.HIGHLIGHTED_ACTION)
                                         .setText("Should not run!"))))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(TEST_SCRIPT, list);
        runScript(script);

        waitUntilViewMatchesCondition(withText("Stop"), isCompletelyDisplayed());
        onView(withText("Stop")).perform(click());
        waitUntilViewAssertionTrue(
                withId(R.id.autofill_assistant), doesNotExist(), DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void runInterruptDuringFlow() throws Exception {
        ArrayList<AutofillAssistantTestScript> scripts = new ArrayList<>();
        ArrayList<ActionProto> nestedActions = new ArrayList<>();
        nestedActions.add(
                ActionProto.newBuilder()
                        .setConfigureUiState(ConfigureUiStateProto.newBuilder().setOverlayBehavior(
                                OverlayBehavior.HIDDEN))
                        .build());

        nestedActions.add(
                ActionProto.newBuilder()
                        .setPrompt(PromptProto.newBuilder().setAllowInterrupt(true).addChoices(
                                PromptProto.Choice.newBuilder().setChip(
                                        ChipProto.newBuilder().setText("Continue"))))
                        .build());
        nestedActions.add(ActionProto.newBuilder()
                                  .setPrompt(PromptProto.newBuilder().addChoices(
                                          PromptProto.Choice.newBuilder().setChip(
                                                  ChipProto.newBuilder().setText("Finished"))))
                                  .build());

        // The main script runs the above actions as a JS flow, i.e., it contains only a single
        // action.
        AutofillAssistantTestScript mainScript = new AutofillAssistantTestScript(
                TEST_SCRIPT, Collections.singletonList(toJsFlowAction(nestedActions)));
        scripts.add(mainScript);

        ArrayList<ActionProto> interruptActionList = new ArrayList<>();
        SelectorProto touchAreaOne = toCssSelector("#touch_area_one");
        addClickSteps(touchAreaOne, interruptActionList);
        interruptActionList.add(
                ActionProto.newBuilder()
                        .setPrompt(PromptProto.newBuilder().addChoices(
                                PromptProto.Choice.newBuilder().setChip(
                                        ChipProto.newBuilder().setText("Interrupt"))))
                        .build());

        // The interrupt triggers when touch_area_one is present but touch_area_four is gone, so
        // that we can trigger it manually.
        SelectorProto touchAreaFour = toCssSelector("#touch_area_four");
        ScriptPreconditionProto interruptPrecondition =
                ScriptPreconditionProto.newBuilder()
                        .setElementCondition(ElementConditionProto.newBuilder().setAllOf(
                                ElementConditionsProto.newBuilder()
                                        .addConditions(ElementConditionProto.newBuilder().setNoneOf(
                                                ElementConditionsProto.newBuilder().addConditions(
                                                        ElementConditionProto.newBuilder().setMatch(
                                                                touchAreaFour))))
                                        .addConditions(ElementConditionProto.newBuilder().setMatch(
                                                touchAreaOne))))
                        .build();

        // The interrupt is also run as a JS flow.
        AutofillAssistantTestScript interruptScript = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(INTERRUPT_SCRIPT_PATH)
                        .setPresentation(
                                PresentationProto.newBuilder().setInterrupt(true).setPrecondition(
                                        interruptPrecondition))
                        .build(),
                Collections.singletonList(toJsFlowAction(interruptActionList)));
        scripts.add(interruptScript);

        runScript(scripts);

        waitUntilViewMatchesCondition(withText("Continue"), isDisplayed());

        // Tapping touch_area_four will make it disappear, which triggers the interrupt.
        tapElement(mTestRule, "touch_area_four");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_four"));

        // The interrupt should click on touch_area_one, making it disappear.
        waitUntilViewMatchesCondition(withText("Interrupt"), isDisplayed());
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));

        // Click the chip to end the interrupt and go back to the main script.
        onView(withText("Interrupt")).perform(click());

        // Once the interrupt is done, the prompt chip should appear again.
        waitUntilViewMatchesCondition(withText("Continue"), isDisplayed());

        // The flow should continue after the prompt finishes.
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Finished"), isDisplayed());
    }
}
