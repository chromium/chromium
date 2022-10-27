// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayingAtLeast;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto.Filter;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto.SemanticFilter;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TellProto;
import org.chromium.chrome.browser.autofill_assistant.proto.WaitForDomProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests for the Autofill Assistant semantic prediction model.
 *
 * The Semantic Prediction model is overridden via a command line flag. This
 * is done so because otherwise the model would have to be downloaded at Chrome
 * startup, which adds latency and flakiness to the tests (network call).
 *
 * The model file is located in |Environment.getExternalStorageDirectory()|,
 * however that directory is hardcoded in @CommandLineFlags because method
 * invocations are not allowed in annotations. Since this is an Android test,
 * this external storage directory is /storage/emulated/0/chromium_tests_root.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-features=OptimizationTargetPrediction:fetch_startup_delay_ms/50",
        "optimization-guide-model-override=OPTIMIZATION_TARGET_AUTOFILL_ASSISTANT:"
                + "/storage/emulated/0/chromium_tests_root/components/test/data/autofill_assistant/model/model.tflite"})
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AutofillAssistantSemanticPredictionIntegrationTest {
    private static final String TEST_PAGE = "model/happy_path.html";

    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(
            new AutofillAssistantCustomTabTestRule(mTestRule, TEST_PAGE));

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1379094")
    public void happyPath() throws Exception {
        ArrayList<ActionProto> actions = new ArrayList<>();

        showMessage(actions, "Begin ...");
        waitForDom(actions, "#foot");
        choice(actions, "Find city");
        waitForDom(actions, "#city", 49, 7);
        showMessage(actions, "Element found");

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("happy_path.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                actions);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Begin ..."), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(withText("Find city"), isDisplayingAtLeast(90));
        onView(withText("Find city")).perform(click());
        waitUntilViewMatchesCondition(withText("Element found"), isCompletelyDisplayed(), 5_000);
    }

    private void waitForDom(List<ActionProto> actions, String cssSelector) {
        actions.add(
                ActionProto.newBuilder()
                        .setWaitForDom(
                                WaitForDomProto.newBuilder().setTimeoutMs(3_000).setWaitCondition(
                                        ElementConditionProto.newBuilder().setMatch(
                                                toCssSelector(cssSelector))))
                        .build());
    }

    private void waitForDom(
            List<ActionProto> actions, String cssSelector, int role, int objective) {
        SelectorProto moonracerSelector =
                SelectorProto.newBuilder()
                        .addFilters(Filter.newBuilder()
                                            .setCssSelector(cssSelector)
                                            .setSemantic(SemanticFilter.newBuilder()
                                                                 .setRole(role)
                                                                 .setObjective(objective)))
                        .build();
        actions.add(
                ActionProto.newBuilder()
                        .setWaitForDom(
                                WaitForDomProto.newBuilder().setTimeoutMs(3_000).setWaitCondition(
                                        ElementConditionProto.newBuilder().setMatch(
                                                moonracerSelector)))
                        .build());
    }

    private void choice(List<ActionProto> actions, String text) {
        actions.add(
                ActionProto.newBuilder()
                        .setPrompt(PromptProto.newBuilder().addChoices(
                                Choice.newBuilder().setChip(ChipProto.newBuilder().setText(text))))
                        .build());
    }

    private void showMessage(List<ActionProto> actions, String message) {
        actions.add(ActionProto.newBuilder()
                            .setTell(TellProto.newBuilder().setMessage(message))
                            .build());
    }
}
