// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.isInternal;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.iterableWithSize;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;

import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
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
import org.chromium.chrome.browser.autofill_assistant.proto.PromptQrCodeScanProto.ImagePickerUiStrings;
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

    @Before
    public void setUp() {
        // Initializes Intents and begins recording intents. Must be called prior to triggering
        // any actions that send out intents which need to be verified or stubbed.
        Intents.init();
    }

    @After
    public void tearDown() {
        // Clears Intents state.
        Intents.release();
    }

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
                        .setCameraPreviewSecurityText(
                                "The details will be safely shared with the website")
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
                is(ProcessedActionStatusProto.QR_CODE_SCAN_CANCELLED));
    }

    @Test
    @MediumTest
    public void testImagePickerCreatesActionPickIntent() throws Exception {
        // Stub all external intents. By default Espresso does not stub any Intent. Note that in
        // this case, all external calls will be blocked.
        intending(not(isInternal())).respondWith(new ActivityResult(Activity.RESULT_OK, null));

        ArrayList<ActionProto> list = new ArrayList<>();
        ImagePickerUiStrings imagePickerUiStrings =
                ImagePickerUiStrings.newBuilder()
                        .setTitleText("Scan QR Code")
                        .setPermissionText("Please provide permissions to access images")
                        .setPermissionButtonText("Continue")
                        .setOpenSettingsText("Please enable media permissions in device settings")
                        .setOpenSettingsButtonText("Open Settings")
                        .build();
        list.add(ActionProto.newBuilder()
                         .setPromptQrCodeScan(
                                 PromptQrCodeScanProto.newBuilder()
                                         .setUseGallery(true)
                                         .setOutputClientMemoryKey("output_client_memory_key")
                                         .setImagePickerUiStrings(imagePickerUiStrings))
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

        // Verify that an ACTION_PICK intent is started.
        intended(hasAction(Intent.ACTION_PICK));
    }
}
