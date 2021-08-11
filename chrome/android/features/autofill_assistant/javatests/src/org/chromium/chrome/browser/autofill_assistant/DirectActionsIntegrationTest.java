// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsInAnyOrder;
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
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DirectActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.directactions.DirectActionHandler;
import org.chromium.chrome.browser.directactions.FakeDirectActionReporter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
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
    private AssistantDependenciesImpl mAssistantDependencies;
    private DirectActionHandler mDirectActionHandler;
    private FakeDirectActionReporter mDirectActionReporter;

    @Before
    public void setUp() {
        mDirectActionReporter = new FakeDirectActionReporter();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModuleEntry =
                    AutofillAssistantModuleEntryProvider.INSTANCE.getModuleEntryIfInstalled();
            assert mModuleEntry != null;
            mAssistantDependencies =
                    (AssistantDependenciesImpl) AutofillAssistantFacade.createDependencies(
                            mTestRule.getActivity(), mModuleEntry);
            mDirectActionHandler = AutofillAssistantFacade.createDirectActionHandler(
                    mTestRule.getActivity(), mAssistantDependencies.getBottomSheetController(),
                    mAssistantDependencies.getBrowserControls(),
                    mAssistantDependencies.getCompositorViewHolder(),
                    mAssistantDependencies.getActivityTabProvider());
        });
    }

    @Test
    @MediumTest
    @Features.
    EnableFeatures({ChromeFeatureList.DIRECT_ACTIONS, ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_DIRECT_ACTIONS})
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
    @Features.
    EnableFeatures({ChromeFeatureList.DIRECT_ACTIONS, ChromeFeatureList.AUTOFILL_ASSISTANT,
            ChromeFeatureList.AUTOFILL_ASSISTANT_DIRECT_ACTIONS})
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
}