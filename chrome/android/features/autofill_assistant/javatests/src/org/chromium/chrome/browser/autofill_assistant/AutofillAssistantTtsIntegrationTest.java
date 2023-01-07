// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.Matchers.is;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistantWithParams;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.accessibilityservice.AccessibilityServiceInfo;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
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
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantTestTtsController.SpeakRequest;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TellProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TellProto.TextToSpeech;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.AssistantTagsForTesting;
import org.chromium.components.autofill_assistant.R;
import org.chromium.content.browser.accessibility.BrowserAccessibilityState;
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

    private int mOriginalFeedbackTypeMask;

    @Before
    public void setUp() throws Exception {
        mOriginalFeedbackTypeMask =
                BrowserAccessibilityState.getAccessibilityServiceFeedbackTypeMask();
    }

    @After
    public void tearDown() {
        // Reset accessibility state.
        BrowserAccessibilityState.setFeedbackTypeMaskForTesting(mOriginalFeedbackTypeMask);
    }

    private void startAutofillAssistantWithTts(AutofillAssistantTestScript script,
            AutofillAssistantTestTtsController testTtsController) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.scheduleForInjection();
        testTtsController.scheduleForInjection();
        HashMap<String, Object> parameters = new HashMap();
        parameters.put("ENABLE_TTS", true);
        parameters.put("START_IMMEDIATELY", true);
        startAutofillAssistantWithParams(mTestRule.getActivity(),
                mTestRule.getActivity().getInitialIntent().getDataString(), parameters);
    }

    @Test
    @MediumTest
    public void testTtsButtonBehavior() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setTell(TellProto.newBuilder()
                                          .setMessage("hello world")
                                          .setTextToSpeech(
                                                  TextToSpeech.newBuilder().setPlayNow(true)))
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
        startAutofillAssistantWithTts(script, testTtsController);

        // Verify that TTS button is displayed and TTS starts playing immediately.
        waitUntilViewMatchesCondition(withText("Chip"), isCompletelyDisplayed());
        onView(withId(R.id.tts_button)).check(matches(isDisplayed()));
        onView(withId(R.id.tts_button))
                .check(matches(withTagValue(is(AssistantTagsForTesting.TTS_ENABLED_ICON_TAG))));
        verify(mOnSpeakRequestCallbackMock)
                .onResult(argThat(request
                        -> request.mMessage.equals("hello world")
                                && request.mLocale.equals("en-US")));

        // Verify that TTS is played on button click.
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

        // Verify that TTS is stopped when bottom sheet is closed.
        reset(mOnStopRequestCallbackMock);
        onView(allOf(withContentDescription("Close"),
                       isDescendantOfA(
                               withTagValue(is(AssistantTagsForTesting.RECYCLER_VIEW_TAG)))))
                .perform(click());
        waitUntilViewMatchesCondition(withText("Undo"), isDisplayed());
        verify(mOnStopRequestCallbackMock).run();
    }

    @Test
    @MediumTest
    public void ttsButtonIsNotShownWhenA11yEnabled() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setTell(TellProto.newBuilder()
                                          .setMessage("hello world")
                                          .setTextToSpeech(
                                                  TextToSpeech.newBuilder().setPlayNow(true)))
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

        // Mock enabling an accessibility service with spoken feedback.
        BrowserAccessibilityState.setFeedbackTypeMaskForTesting(
                AccessibilityServiceInfo.FEEDBACK_SPOKEN);

        AutofillAssistantTestTtsController testTtsController =
                new AutofillAssistantTestTtsController(
                        mOnSpeakRequestCallbackMock, mOnStopRequestCallbackMock);
        startAutofillAssistantWithTts(script, testTtsController);

        // Verify that the TTS button is not displayed and `play_now` does not play TTS.
        waitUntilViewMatchesCondition(withText("Chip"), isCompletelyDisplayed());
        onView(withId(R.id.tts_button)).check(matches(not(isDisplayed())));
        verify(mOnSpeakRequestCallbackMock, never()).onResult(any(SpeakRequest.class));
    }
}
