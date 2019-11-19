// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.swipeRight;
import static android.support.test.espresso.assertion.PositionAssertions.isRightOf;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.hasDescendant;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.DefaultItemAnimator;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantActionsCarouselCoordinator;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselModel;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the autofill assistant actions carousel.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantActionsCarouselUiTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    /** Creates a coordinator for use in UI tests, and adds it to the global view hierarchy. */
    private AssistantActionsCarouselCoordinator createCoordinator(AssistantCarouselModel model)
            throws Exception {
        AssistantActionsCarouselCoordinator coordinator = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> new AssistantActionsCarouselCoordinator(
                                InstrumentationRegistry.getTargetContext(), model));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Note: apparently, we need an intermediate container for this coordinator's view,
            // otherwise the view will be invisible.
            // @TODO(crbug.com/806868) figure out why this is the case.
            LinearLayout container = new LinearLayout(InstrumentationRegistry.getTargetContext());
            container.addView(coordinator.getView());
            AutofillAssistantUiTestUtil.attachToCoordinator(mTestRule.getActivity(), container);
        });

        return coordinator;
    }

    @Before
    public void setUp() {
        AutofillAssistantUiTestUtil.startOnBlankPage(mTestRule);
    }

    /** Tests assumptions about the initial state of the carousel. */
    @Test
    @MediumTest
    public void testInitialState() throws Exception {
        AssistantCarouselModel model = new AssistantCarouselModel();
        AssistantActionsCarouselCoordinator coordinator = createCoordinator(model);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertThat(((DefaultItemAnimator) coordinator.getView().getItemAnimator())
                               .getSupportsChangeAnimations(),
                    is(false));
        });
        assertThat(model.getChipsModel().size(), is(0));
        assertThat(coordinator.getView().getAdapter().getItemCount(), is(0));
    }

    /** Adds a single chip and tests assumptions about the view state after the change. */
    @Test
    @MediumTest
    public void testAddSingleChip() throws Exception {
        AssistantCarouselModel model = new AssistantCarouselModel();
        AssistantActionsCarouselCoordinator coordinator = createCoordinator(model);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.getChipsModel().add(
                                new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE,
                                        AssistantChip.Icon.NONE, "Test", false, true, null)));

        // Chip was created and is displayed on the screen.
        onView(is(coordinator.getView()))
                .check(matches(hasDescendant(allOf(withText("Test"), isDisplayed()))));

        // TODO(crbug.com/806868): test that single chip is center aligned.
    }

    /** Adds multiple chips and tests assumptions about the view state after the change. */
    @Test
    @MediumTest
    public void testAddMultipleChips() throws Exception {
        AssistantCarouselModel model = new AssistantCarouselModel();
        AssistantActionsCarouselCoordinator coordinator = createCoordinator(model);

        // Note: this should be a small number that fits on screen without scrolling.
        int numChips = 3;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int i = 0; i < numChips; i++) {
                model.getChipsModel().add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE,
                        AssistantChip.Icon.NONE, "T" + i, false, false, null));
            }
            model.getChipsModel().add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE,
                    AssistantChip.Icon.NONE, "X", false, true, null));
        });

        // Cancel chip is displayed to the user.
        onView(withText("X")).check(matches(isDisplayed()));

        // All chips are to the right of the cancel chip.
        for (int i = 0; i < numChips; i++) {
            onView(withText("T" + i)).check(isRightOf(withText("X")));
        }
    }

    /** Adds many chips and tests that the cancel chip is always visible. */
    @Test
    @MediumTest
    public void testCancelChipAlwaysVisible() throws Exception {
        AssistantCarouselModel model = new AssistantCarouselModel();
        AssistantActionsCarouselCoordinator coordinator = createCoordinator(model);

        // Note: this should be a large number that does not fit on screen without scrolling.
        int numChips = 30;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int i = 0; i < numChips; i++) {
                model.getChipsModel().add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE,
                        AssistantChip.Icon.NONE, "Test" + i, false, false, null));
            }
            model.getChipsModel().add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE,
                    AssistantChip.Icon.NONE, "Cancel", false, true, null));
        });

        // Cancel chip is initially displayed to the user.
        onView(withText("Cancel")).check(matches(isDisplayed()));

        // Scroll right, check that cancel is still visible.
        onView(is(coordinator.getView())).perform(swipeRight());
        onView(withText("Cancel")).check(matches(isDisplayed()));
    }
}
