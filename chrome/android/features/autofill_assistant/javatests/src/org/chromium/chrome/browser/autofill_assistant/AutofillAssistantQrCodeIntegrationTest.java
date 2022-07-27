// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.iterableWithSize;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionStatusProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptQrCodeScanProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptQrCodeScanProto.CameraScanUiStrings;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests autofill assistant's QR Code Scan functionality.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantQrCodeIntegrationTest {
    private static final String TEST_PAGE = "autofill_assistant_target_website.html";

    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(
            new AutofillAssistantCustomTabTestRule(mTestRule, TEST_PAGE));

    @Test
    @MediumTest
    public void testCameraScanToolbar() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        CameraScanUiStrings cameraScanUiStrings =
                CameraScanUiStrings.newBuilder()
                        .setTitleText("Scan QR Code")
                        .setPermissionText("Please provide camera permissions")
                        .setPermissionButtonText("Continue")
                        .setOpenSettingsText("Please enable camera permissions in device settings")
                        .setOpenSettingsButtonText("Open Settings")
                        .setCameraPreviewInstructionText("Focus the QR Code inside the box")
                        .build();
        list.add(ActionProto.newBuilder()
                         .setPromptQrCodeScan(
                                 PromptQrCodeScanProto.newBuilder()
                                         .setOutputClientMemoryKey("output_client_memory_key")
                                         .setCameraScanUiStrings(cameraScanUiStrings))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        // Verify that toolbar title is displayed.
        waitUntilViewMatchesCondition(withText("Scan QR Code"), isCompletelyDisplayed());

        // Prepare next set of actions, cancel QR Code Scan action and verify action status.
        ArrayList<ActionProto> nextActions = new ArrayList<>();
        nextActions.add(
                ActionProto.newBuilder()
                        .setPrompt(PromptProto.newBuilder()
                                           .setMessage("Finished")
                                           .addChoices(PromptProto.Choice.newBuilder().setChip(
                                                   ChipProto.newBuilder()
                                                           .setType(ChipType.DONE_ACTION)
                                                           .setText("End"))))
                        .build());
        testService.setNextActions(nextActions);
        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withContentDescription("Close")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);
        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(1));
        assertThat(processedActions.get(0).getStatus(),
                is(ProcessedActionStatusProto.OTHER_ACTION_STATUS));
    }
}
