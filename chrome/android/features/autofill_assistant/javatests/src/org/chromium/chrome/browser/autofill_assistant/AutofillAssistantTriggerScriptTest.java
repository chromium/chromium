// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.support.test.InstrumentationRegistry;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.coordinatorlayout.widget.CoordinatorLayout;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.drawable.AssistantDrawableIcon;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantDrawable;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.autofill_assistant.trigger_scripts.AssistantTriggerScript;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.List;

/** UI tests for {@code AssistantTriggerScript}. */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantTriggerScriptTest {
    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "autofill_assistant_target_website.html";

    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Before
    public void setUp() {
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
    }

    protected BottomSheetController getBottomSheetController() {
        return AutofillAssistantUiTestUtil.getBottomSheetController(mTestRule.getActivity());
    }

    /**
     * Creates a linear layout at the bottom of the screen for use in tests. Showing content
     * directly in the bottom sheet has been flaky in the past (see e.g., crbug.com/1146084).
     */
    private LinearLayout createViewContainerForTest() {
        CoordinatorLayout.LayoutParams lp = new CoordinatorLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.BOTTOM;

        LinearLayout container = new LinearLayout(mTestRule.getActivity());
        container.setOrientation(LinearLayout.VERTICAL);

        ViewGroup chromeCoordinatorView = mTestRule.getActivity().findViewById(R.id.coordinator);
        chromeCoordinatorView.addView(container, lp);
        return container;
    }

    @Test
    @MediumTest
    public void testTriggerScript() throws Exception {
        AssistantTriggerScript triggerScript = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> new AssistantTriggerScript(
                                mTestRule.getActivity(), new AssistantTriggerScript.Delegate() {
                                    @Override
                                    public void onTriggerScriptAction(int action) {}

                                    @Override
                                    public void onBottomSheetClosedWithSwipe() {}

                                    @Override
                                    public void onBackButtonPressed() {}

                                    @Override
                                    public void onFeedbackButtonClicked() {}
                                }, getBottomSheetController()));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantHeaderModel headerModel = triggerScript.getHeaderModelForTest();
            headerModel.set(AssistantHeaderModel.STATUS_MESSAGE, "Hello world!");
            headerModel.set(AssistantHeaderModel.USE_STEP_PROGRESS_BAR, true);
            headerModel.set(AssistantHeaderModel.STEP_PROGRESS_BAR_ICONS,
                    Arrays.asList(AssistantDrawable.createFromIcon(
                                          AssistantDrawableIcon.PROGRESSBAR_DEFAULT_INITIAL_STEP),
                            AssistantDrawable.createFromIcon(
                                    AssistantDrawableIcon.PROGRESSBAR_DEFAULT_DATA_COLLECTION),
                            AssistantDrawable.createFromIcon(
                                    AssistantDrawableIcon.PROGRESSBAR_DEFAULT_PAYMENT),
                            AssistantDrawable.createFromIcon(
                                    AssistantDrawableIcon.PROGRESSBAR_DEFAULT_FINAL_STEP)));
            headerModel.set(AssistantHeaderModel.PROGRESS_ACTIVE_STEP, 1);

            List<AssistantChip> leftAlignedChips = triggerScript.getLeftAlignedChipsForTest();
            leftAlignedChips.add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE,
                    AssistantChip.Icon.OVERFLOW, "", false, false, true, () -> {}));

            List<AssistantChip> rightAlignedChips = triggerScript.getRightAlignedChipsForTest();
            rightAlignedChips.add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE,
                    AssistantChip.Icon.NONE, "Not now", false, false, true, () -> {}));
            rightAlignedChips.add(new AssistantChip(AssistantChip.Type.BUTTON_FILLED_BLUE,
                    AssistantChip.Icon.NONE, "Fast checkout", false, false, true, () -> {}));
        });

        triggerScript.disableBottomSheetAnimationsForTesting(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            triggerScript.update();
            createViewContainerForTest().addView(
                    triggerScript.getBottomSheetContentForTest().getContentView());
        });
        onView(withId(R.id.autofill_assistant)).check(matches(isDisplayed()));
        onView(withId(R.id.header)).check(matches(isDisplayed()));
        onView(withId(R.id.poodle_wrapper)).check(matches(isDisplayed()));
        onView(withText("Hello world!")).check(matches(isDisplayed()));
        onView(withId(R.id.profile_image)).check(matches(isDisplayed()));
        onView(withId(R.id.step_progress_bar)).check(matches(isDisplayed()));
        onView(withContentDescription(R.string.autofill_assistant_overflow_options))
                .check(matches(isDisplayed()));
        onView(withText("Not now")).check(matches(isDisplayed()));
        onView(withText("Fast checkout")).check(matches(isDisplayed()));
    }
}
