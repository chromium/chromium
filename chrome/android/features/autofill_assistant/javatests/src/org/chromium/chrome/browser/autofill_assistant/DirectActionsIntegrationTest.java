// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementExists;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.MiniActionTestUtil.addTapSteps;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;

import android.os.Bundle;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
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
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DirectActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.InfoBoxProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowInfoBoxProto;
import org.chromium.chrome.browser.autofill_assistant.proto.StopProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TellProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.directactions.DirectActionHandler;
import org.chromium.chrome.browser.directactions.FakeDirectActionReporter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.autofill_assistant.AssistantDependencies;
import org.chromium.components.autofill_assistant.AssistantFeatures;
import org.chromium.components.autofill_assistant.AutofillAssistantModuleEntry;
import org.chromium.components.autofill_assistant.AutofillAssistantModuleEntryProvider;
import org.chromium.components.autofill_assistant.AutofillAssistantPreferencesUtil;
import org.chromium.components.autofill_assistant.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Tests autofill-assistant direct actions.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class DirectActionsIntegrationTest {
    public DirectActionsIntegrationTest() {}

    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain =
            RuleChain.outerRule(mTestRule).around(new AutofillAssistantCustomTabTestRule(
                    mTestRule, "autofill_assistant_target_website.html"));

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    Callback<Bundle> mDirectActionResultCallback;

    private AutofillAssistantModuleEntry mModuleEntry;
    private DirectActionHandler mDirectActionHandler;
    private FakeDirectActionReporter mDirectActionReporter;

    @Before
    public void setUp() {
        mDirectActionReporter = new FakeDirectActionReporter();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModuleEntry =
                    AutofillAssistantModuleEntryProvider.INSTANCE.getModuleEntryIfInstalled();
            assert mModuleEntry != null;
            AssistantDependencies dependencies =
                    new AssistantStaticDependenciesChrome().createDependencies(
                            mTestRule.getActivity());
            mDirectActionHandler = AutofillAssistantFacade.createDirectActionHandler(
                    mTestRule.getActivity(), dependencies.getBottomSheetController(),
                    mTestRule.getActivity().getBrowserControlsManager(), dependencies.getRootView(),
                    mTestRule.getActivity().getActivityTabProvider());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.DIRECT_ACTIONS, AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_DIRECT_ACTIONS_NAME})
    public void
    testOnboardingAndStart() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(false);
        mDirectActionHandler.reportAvailableDirectActions(mDirectActionReporter);
        Assert.assertThat(mDirectActionReporter.getDirectActions(),
                containsInAnyOrder("onboarding", "onboarding_and_start"));

        ArrayList<ActionProto> list = new ArrayList<>();
        // Tapping touch_area_one will make it disappear.
        addTapSteps(toCssSelector("#touch_area_one"), list);

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setDirectAction(
                                DirectActionProto.newBuilder()
                                        .addNames("some_direct_action")
                                        .build()))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.scheduleForInjection();

        Bundle arguments = new Bundle();
        arguments.putString("name", "some_direct_action");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDirectActionHandler.performDirectAction(
                    "onboarding_and_start", arguments, mDirectActionResultCallback);
        });

        waitUntilViewMatchesCondition(withText("I agree"), isDisplayed());
        onView(withText("I agree")).perform(click());

        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));
        verify(mDirectActionResultCallback)
                .onResult(argThat(bundle -> bundle.getBoolean("success")));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.DIRECT_ACTIONS, AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_DIRECT_ACTIONS_NAME})
    public void
    testOnboardingAndStartShowsErrorMessageIfRequested() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(false);
        mDirectActionHandler.reportAvailableDirectActions(mDirectActionReporter);
        Assert.assertThat(mDirectActionReporter.getDirectActions(),
                containsInAnyOrder("onboarding", "onboarding_and_start"));

        // No scripts available.
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.emptyList());
        testService.scheduleForInjection();

        Bundle arguments = new Bundle();
        arguments.putString("name", "some_direct_action");
        arguments.putBoolean("show_error_on_failure", true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDirectActionHandler.performDirectAction(
                    "onboarding_and_start", arguments, mDirectActionResultCallback);
        });

        waitUntilViewMatchesCondition(withText("I agree"), isDisplayed());
        onView(withText("I agree")).perform(click());

        waitUntilViewMatchesCondition(withText("Sorry, something went wrong."), isDisplayed());
        verify(mDirectActionResultCallback)
                .onResult(argThat(bundle -> !bundle.getBoolean("success")));
    }

    /**
     * Regression test for b/200916720.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.DIRECT_ACTIONS, AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_DIRECT_ACTIONS_NAME})
    public void
    testOnboardingTwice() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(false);
        mDirectActionHandler.reportAvailableDirectActions(mDirectActionReporter);
        Assert.assertThat(mDirectActionReporter.getDirectActions(),
                containsInAnyOrder("onboarding", "onboarding_and_start"));

        ArrayList<ActionProto> list = new ArrayList<>();
        // Tapping touch_area_one will make it disappear.
        addTapSteps(toCssSelector("#touch_area_one"), list);

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setDirectAction(
                                DirectActionProto.newBuilder()
                                        .addNames("some_direct_action")
                                        .build()))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.scheduleForInjection();

        Bundle arguments = new Bundle();
        arguments.putString("name", "some_direct_action");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDirectActionHandler.performDirectAction(
                    "onboarding_and_start", arguments, mDirectActionResultCallback);
        });
        waitUntilViewMatchesCondition(withText("I agree"), isDisplayed());

        // Don't agree to the onboarding. Instead, restart the direct action. This tests a case
        // where the client already exists, but the controller does not.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDirectActionHandler.performDirectAction(
                    "onboarding_and_start", arguments, mDirectActionResultCallback);
        });
        waitUntilViewMatchesCondition(withText("I agree"), isDisplayed());

        onView(withText("I agree")).perform(click());
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));
        verify(mDirectActionResultCallback)
                .onResult(argThat(bundle -> bundle.getBoolean("success")));
    }

    /**
     * Regression test for b/195417453.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.DIRECT_ACTIONS, AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_DIRECT_ACTIONS_NAME})
    @DisabledTest(message = "https://crbug.com/1272997")
    public void
    testStatusMessageResetsBetweenRuns() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Prompt"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setShowInfoBox(ShowInfoBoxProto.newBuilder().setInfoBox(
                                 InfoBoxProto.newBuilder().setExplanation(
                                         "InfoBox message from previous run")))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setTell(TellProto.newBuilder().setMessage(
                                 "Status message from previous run"))
                         .build());
        list.add(ActionProto.newBuilder().setStop(StopProto.newBuilder()).build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setDirectAction(
                                DirectActionProto.newBuilder()
                                        .addNames("some_direct_action")
                                        .build()))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.scheduleForInjection();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDirectActionHandler.reportAvailableDirectActions(mDirectActionReporter);
            Assert.assertThat(mDirectActionReporter.getDirectActions(),
                    containsInAnyOrder("fetch_website_actions"));
            mDirectActionHandler.performDirectAction(
                    "fetch_website_actions", new Bundle(), mDirectActionResultCallback);
            verify(mDirectActionResultCallback)
                    .onResult(argThat(bundle -> bundle.getBoolean("success")));

            mDirectActionHandler.reportAvailableDirectActions(mDirectActionReporter);
            Assert.assertThat(mDirectActionReporter.getDirectActions(),
                    containsInAnyOrder("fetch_website_actions", "some_direct_action"));
            mDirectActionHandler.performDirectAction(
                    "some_direct_action", new Bundle(), mDirectActionResultCallback);
        });
        waitUntilViewMatchesCondition(withText("Prompt"), isDisplayed());

        // Changes the status message, shows the info box, then gracefully stops the script.
        onView(withText("Prompt")).perform(click());

        // Run the same direct action again, but don't accept the prompt. The script won't run the
        // showInfoBox and tell actions, thus the UI should be left at startup-default.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDirectActionHandler.performDirectAction(
                    "some_direct_action", new Bundle(), mDirectActionResultCallback);
        });
        waitUntilViewMatchesCondition(withText("Prompt"), isDisplayed());
        onView(withText("InfoBox message from previous run")).check(doesNotExist());
        onView(withId(R.id.info_box_explanation)).check(matches(not(isDisplayed())));
        onView(withText("Status message from previous run")).check(doesNotExist());
    }

    /**
     * Regression test for b/195417125.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.DIRECT_ACTIONS, AssistantFeatures.AUTOFILL_ASSISTANT_NAME,
            AssistantFeatures.AUTOFILL_ASSISTANT_DIRECT_ACTIONS_NAME})
    public void
    testLastTellMessageDisplayedAfterStop() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().addChoices(
                                 PromptProto.Choice.newBuilder().setChip(
                                         ChipProto.newBuilder().setText("Prompt"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setTell(TellProto.newBuilder().setMessage("Last tell message"))
                         .build());
        list.add(ActionProto.newBuilder().setStop(StopProto.newBuilder()).build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setDirectAction(
                                DirectActionProto.newBuilder()
                                        .addNames("some_direct_action")
                                        .build()))
                        .build(),
                list);
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.scheduleForInjection();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDirectActionHandler.reportAvailableDirectActions(mDirectActionReporter);
            Assert.assertThat(mDirectActionReporter.getDirectActions(),
                    containsInAnyOrder("fetch_website_actions"));
            mDirectActionHandler.performDirectAction(
                    "fetch_website_actions", new Bundle(), mDirectActionResultCallback);
            verify(mDirectActionResultCallback)
                    .onResult(argThat(bundle -> bundle.getBoolean("success")));

            mDirectActionHandler.reportAvailableDirectActions(mDirectActionReporter);
            Assert.assertThat(mDirectActionReporter.getDirectActions(),
                    containsInAnyOrder("fetch_website_actions", "some_direct_action"));
            mDirectActionHandler.performDirectAction(
                    "some_direct_action", new Bundle(), mDirectActionResultCallback);
        });
        waitUntilViewMatchesCondition(withText("Prompt"), isDisplayed());
        onView(withText("Prompt")).perform(click());
        waitUntilViewMatchesCondition(withText("Last tell message"), isDisplayed());
        // The last tell message should still be visible (and not disappear
        // immediately) after the script stops.
        try {
            waitUntilViewMatchesCondition(withText("Last tell message"), not(isDisplayed()), 200);
        } catch (AssertionError e) {
            if (e.getCause() instanceof CriteriaNotSatisfiedException) {
                // This is ok, the view is still there, the test succeeds.
                return;
            }
            throw e;
        }
        throw new CriteriaNotSatisfiedException(
                "Expected last tell message to be visible after stop");
    }
}
