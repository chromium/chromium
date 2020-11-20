// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.swipeRight;
import static androidx.test.espresso.assertion.PositionAssertions.isRightOf;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;

import android.support.test.InstrumentationRegistry;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantActionsCarouselCoordinator;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselModel;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

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

        assertThat(model.get(AssistantCarouselModel.CHIPS).size(), is(0));
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
                        -> model.set(AssistantCarouselModel.CHIPS,
                                Collections.singletonList(new AssistantChip(
                                        AssistantChip.Type.BUTTON_HAIRLINE, AssistantChip.Icon.NONE,
                                        "Test", false, true, true, ""))));

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
        List<AssistantChip> chips = new ArrayList<>();
        for (int i = 0; i < numChips; i++) {
            chips.add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE, AssistantChip.Icon.NONE,
                    "T" + i, false, false, true, ""));
        }
        chips.add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE, AssistantChip.Icon.NONE,
                "X", false, true, true, ""));

        TestThreadUtils.runOnUiThreadBlocking(() -> model.set(AssistantCarouselModel.CHIPS, chips));

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
        List<AssistantChip> chips = new ArrayList<>();
        for (int i = 0; i < numChips; i++) {
            chips.add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE, AssistantChip.Icon.NONE,
                    "Test" + i, false, false, true, ""));
        }
        chips.add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE, AssistantChip.Icon.NONE,
                "Cancel", false, true, true, ""));
        TestThreadUtils.runOnUiThreadBlocking(() -> model.set(AssistantCarouselModel.CHIPS, chips));

        // Cancel chip is initially displayed to the user.
        onView(withText("Cancel")).check(matches(isDisplayed()));

        // Scroll right, check that cancel is still visible.
        onView(is(coordinator.getView())).perform(swipeRight());
        onView(withText("Cancel")).check(matches(isDisplayed()));
    }

    /**
     * Tests the change between two chip configurations:
     * X           Test_2
     * X   Test_1  Test_2
     *
     * This inserts Test_1 in between X and Test_2, forcing Test_2 to move.
     */
    @Test
    @MediumTest
    public void testMoveChip() throws Exception {
        AssistantCarouselModel model = new AssistantCarouselModel();
        AssistantActionsCarouselCoordinator coordinator = createCoordinator(model);

        List<AssistantChip> chips = new ArrayList<>();
        chips.add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE, AssistantChip.Icon.NONE,
                "Test 2", false, false, true, ""));
        chips.add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE, AssistantChip.Icon.NONE,
                "Cancel", false, true, true, ""));
        TestThreadUtils.runOnUiThreadBlocking(() -> model.set(AssistantCarouselModel.CHIPS, chips));
        onView(withText("Cancel")).check(matches(isDisplayed()));
        onView(withText("Test 2")).check(matches(isDisplayed()));
        onView(withText("Test 2")).check(isRightOf(withText("Cancel")));

        List<AssistantChip> newChips = new ArrayList<>();
        newChips.add(chips.get(0));
        newChips.add(new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE, AssistantChip.Icon.NONE,
                "Test 1", false, false, true, ""));
        newChips.add(chips.get(1));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCarouselModel.CHIPS, newChips));
        onView(withText("Cancel")).check(matches(isDisplayed()));
        onView(withText("Test 1")).check(matches(isDisplayed()));
        onView(withText("Test 2")).check(matches(isDisplayed()));
        onView(withText("Test 1")).check(isRightOf(withText("Cancel")));
        onView(withText("Test 2")).check(isRightOf(withText("Test 1")));
    }

    /**
     * Adds a single chip with non empty content description, and tests that same is used as content
     * description.
     */
    @Test
    @MediumTest
    public void testSuppliedNonEmptyContentDescriptionIsUsed() throws Exception {
        String contentDescription = "Test content description";
        AssistantCarouselModel model = new AssistantCarouselModel();
        AssistantActionsCarouselCoordinator coordinator = createCoordinator(model);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantCarouselModel.CHIPS,
                                Collections.singletonList(new AssistantChip(
                                        AssistantChip.Type.BUTTON_HAIRLINE, AssistantChip.Icon.NONE,
                                        "Chip", false, true, true, contentDescription))));

        onView(is(coordinator.getView()))
                .check(matches(hasDescendant(
                        allOf(withContentDescription(contentDescription), isDisplayed()))));
    }

    /**
     * Adds a single chip with empty content description, and tests that same is used as content
     * description.
     */
    @Test
    @MediumTest
    public void testSuppliedEmptyContentDescriptionIsUsed() throws Exception {
        String contentDescription = "";
        AssistantCarouselModel model = new AssistantCarouselModel();
        AssistantActionsCarouselCoordinator coordinator = createCoordinator(model);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantCarouselModel.CHIPS,
                                Collections.singletonList(new AssistantChip(
                                        AssistantChip.Type.BUTTON_HAIRLINE, AssistantChip.Icon.NONE,
                                        "Chip", false, true, true, contentDescription))));

        onView(is(coordinator.getView()))
                .check(matches(hasDescendant(
                        allOf(withContentDescription(contentDescription), isDisplayed()))));
    }

    /**
     * Adds a single chip with null content description, and tests that chip text is used as content
     * description if it's non-empty.
     */
    @Test
    @MediumTest
    public void testWhenNullContentDescriptionIsSuppliedChipTextIsUsed() throws Exception {
        String chipText = "Chip Text";
        AssistantCarouselModel model = new AssistantCarouselModel();
        AssistantActionsCarouselCoordinator coordinatorNonEmptyChipText = createCoordinator(model);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantCarouselModel.CHIPS,
                                Collections.singletonList(
                                        new AssistantChip(AssistantChip.Type.BUTTON_HAIRLINE,
                                                AssistantChip.Icon.DONE, chipText, false, true,
                                                true, /* contentDescription */ null))));

        onView(is(coordinatorNonEmptyChipText.getView()))
                .check(matches(
                        hasDescendant(allOf(withContentDescription(chipText), isDisplayed()))));
    }

    /**
     * Adds a single chip with null content description and empty chip text, and tests that icon
     * description is used as content description if available.
     */
    @Test
    @MediumTest
    public void testWhenNullContentDescriptionIsSuppliedChipTextOrIconDescriptionIsUsed()
            throws Exception {
        AssistantCarouselModel model = new AssistantCarouselModel();
        AssistantActionsCarouselCoordinator coordinatorEmptyChipText = createCoordinator(model);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantCarouselModel.CHIPS,
                                Collections.singletonList(new AssistantChip(
                                        AssistantChip.Type.BUTTON_HAIRLINE, AssistantChip.Icon.DONE,
                                        /* chipText */ "", false, true, true,
                                        /* contentDescription */ null))));

        onView(is(coordinatorEmptyChipText.getView()))
                .check(matches(
                        hasDescendant(allOf(withContentDescription("Done"), isDisplayed()))));
    }
}
