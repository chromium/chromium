// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistantWithParams;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantTestTtsController.SpeakRequest;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TellProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;

/**
 * Tests autofill assistant's TextToSpeech functionality.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantTtsIntegrationTest {
    private static final String TEST_PAGE = "autofill_assistant_target_website.html";

    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(
            new AutofillAssistantCustomTabTestRule(mTestRule, TEST_PAGE));

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    public Callback<SpeakRequest> mOnSpeakRequestCallbackMock;

    @Mock
    public Runnable mOnStopRequestCallbackMock;

    private void startAutofillAssistantWithTts(ChromeActivity activity,
            AutofillAssistantTestService testService,
            AutofillAssistantTestTtsController testTtsController) {
        testService.scheduleForInjection();
        testTtsController.scheduleForInjection();
        HashMap<String, Object> parameters = new HashMap();
        parameters.put("ENABLE_TTS", true);
        parameters.put("START_IMMEDIATELY", true);
        startAutofillAssistantWithParams(
                activity, activity.getInitialIntent().getDataString(), parameters);
    }

    @Test
    @MediumTest
    public void ttsTtsButtonBehavior() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setTell(TellProto.newBuilder().setMessage("hello world"))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Chip"))))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestTtsController testTtsController =
                new AutofillAssistantTestTtsController(
                        mOnSpeakRequestCallbackMock, mOnStopRequestCallbackMock);
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistantWithTts(mTestRule.getActivity(), testService, testTtsController);

        // Verify that TTS button is displayed.
        waitUntilViewMatchesCondition(withText("Chip"), isCompletelyDisplayed());
        onView(withId(R.id.tts_button)).check(matches(isDisplayed()));
        onView(withId(R.id.tts_button))
                .check(matches(withTagValue(is(AssistantTagsForTesting.TTS_ENABLED_ICON_TAG))));

        // Verify that TTS is played on button click.
        onView(withId(R.id.tts_button)).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { testTtsController.simulateTtsEvent(0 /* TTS_START */); });
        waitUntilViewMatchesCondition(withId(R.id.tts_button),
                withTagValue(is(AssistantTagsForTesting.TTS_PLAYING_ICON_TAG)));
        verify(mOnSpeakRequestCallbackMock)
                .onResult(argThat(request
                        -> request.mMessage.equals("hello world")
                                && request.mLocale.equals("en-US")));

        // Verify that TTS button is disabled (muted) on button click when TTS is playing.
        onView(withId(R.id.tts_button)).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.tts_button),
                withTagValue(is(AssistantTagsForTesting.TTS_DISABLED_ICON_TAG)));
        verify(mOnStopRequestCallbackMock).run();

        // Verify that TTS button is enabled on button click when it is in disabled state.
        reset(mOnSpeakRequestCallbackMock);
        onView(withId(R.id.tts_button)).perform(click());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { testTtsController.simulateTtsEvent(0 /* TTS_START */); });
        waitUntilViewMatchesCondition(withId(R.id.tts_button),
                withTagValue(is(AssistantTagsForTesting.TTS_PLAYING_ICON_TAG)));
        verify(mOnSpeakRequestCallbackMock)
                .onResult(argThat(request
                        -> request.mMessage.equals("hello world")
                                && request.mLocale.equals("en-US")));
    }
}
