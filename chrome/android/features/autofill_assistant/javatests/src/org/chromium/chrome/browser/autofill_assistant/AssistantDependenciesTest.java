// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for AssistantDependencies.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AssistantDependenciesTest {
    @Rule
    public ChromeTabbedActivityTestRule mTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    Callback<Integer> mOnboardingCallback;

    private AutofillAssistantModuleEntry mModuleEntry;
    private AssistantDependencies mAssistantDependencies;

    @Before
    public void setUp() {
        mTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModuleEntry =
                    AutofillAssistantModuleEntryProvider.INSTANCE.getModuleEntryIfInstalled();
            assert mModuleEntry != null;
            mAssistantDependencies = AutofillAssistantFacade.createDependencies(
                    mTestRule.getActivity(), mModuleEntry);
        });
    }

    @Test
    @MediumTest
    public void testBottomSheetOnboarding() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAssistantDependencies.showOnboarding(/* useDialogOnboarding = */ false,
                    TriggerContext.newBuilder().build(), mOnboardingCallback);
        });
        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), isCompletelyDisplayed());

        onView(withId(R.id.button_init_ok)).perform(click());
        verify(mOnboardingCallback).onResult(3 /* AssistantOnboardingResult.ACCEPTED */);
    }

    @Test
    @MediumTest
    public void testDialogOnboarding() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAssistantDependencies.showOnboarding(/* useDialogOnboarding = */ true,
                    TriggerContext.newBuilder().build(), mOnboardingCallback);
        });
        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), isCompletelyDisplayed());

        // Check that the UI is shown in a dialog.
        onView(withId(R.id.button_init_ok))
                .inRoot(withDecorView(not(mTestRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));

        onView(withId(R.id.button_init_ok)).perform(click());
        verify(mOnboardingCallback).onResult(3 /* AssistantOnboardingResult.ACCEPTED */);
    }
}
