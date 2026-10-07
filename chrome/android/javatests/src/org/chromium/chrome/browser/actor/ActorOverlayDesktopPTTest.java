// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertNotNull;

import static org.chromium.chrome.browser.actor.ActorOverlayZOrderTestUtil.assertDrawnBelow;
import static org.chromium.chrome.browser.actor.ActorOverlayZOrderTestUtil.childIndexOf;

import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.actor.ui.ActorOverlayCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.glic.GlicUtils;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.GlicTransitTestRule;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.omnibox.OmniboxCapabilities;

/** Integration test for ActorOverlay when running in Android Desktop/Side Panel mode. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.GLIC, ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL})
@DisableFeatures({ChromeFeatureList.TAB_BOTTOM_SHEET})
@Batch(Batch.PER_CLASS)
public class ActorOverlayDesktopPTTest {
    @Rule public final GlicTransitTestRule mTestRule = new GlicTransitTestRule();

    @BeforeClass
    public static void setUpClass() {
        // The rule launches ChromeTabbedActivity before the test body runs, so the form factor
        // override must be in place before then for TabbedRootUiCoordinator to create the
        // GlicUiCoordinator.
        GlicUtils.setIsSidePanelFormFactorForTesting(true);
        // Show the omnibox suggestions as a popover, as on desktop devices, regardless of the
        // device the test runs on.
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
    }

    @After
    public void tearDown() {
        // The activity is reused across the batch, so hide the overlay before the next test. Skip
        // this if the test failed before the overlay was set up, so that failure isn't masked.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ActorOverlayCoordinator coordinator = getActorOverlayCoordinator();
                    if (coordinator != null) coordinator.showOverlayForTesting(false);
                });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/565895755")
    public void testActorOverlayIsInflatedAndCanShow() {
        mTestRule.startOnBlankPage();

        // GlicUiCoordinator and ActorOverlayCoordinator should be initialized even though
        // mTabBottomSheetManager is null.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabbedRootUiCoordinator rootUiCoordinator =
                            (TabbedRootUiCoordinator)
                                    mTestRule.getActivity().getRootUiCoordinatorForTesting();
                    assertNotNull(rootUiCoordinator.getGlicUiCoordinatorForTesting());
                    assertNotNull(rootUiCoordinator.getActorOverlayCoordinatorForTesting());
                });

        // The overlay should not be inflated initially.
        onView(withId(R.id.actor_overlay)).check(doesNotExist());

        // Show the overlay and check that it's displayed.
        showOverlay(true);
        onView(withId(R.id.actor_overlay)).check(matches(isDisplayed()));

        // Hide it and check.
        showOverlay(false);
        onView(withId(R.id.actor_overlay)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testOverlayZOrder() {
        mTestRule.startOnBlankPage();
        showOverlay(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup coordinator = mTestRule.getActivity().findViewById(R.id.coordinator);
                    ActorOverlayZOrderTestUtil.assertOverlayZOrder(coordinator);
                    ActorOverlayZOrderTestUtil.assertOverlayIsNotElevated(coordinator);

                    // The side UI containers are specific to this layout and must also be after
                    // the overlay, whichever side the UI is anchored to.
                    int overlay = childIndexOf(coordinator, R.id.actor_overlay);
                    assertDrawnBelow(
                            "actor overlay",
                            overlay,
                            "left side UI container",
                            childIndexOf(
                                    coordinator,
                                    R.id.side_ui_left_anchor_container,
                                    R.id.side_ui_left_anchor_container_stub));
                    assertDrawnBelow(
                            "actor overlay",
                            overlay,
                            "right side UI container",
                            childIndexOf(
                                    coordinator,
                                    R.id.side_ui_right_anchor_container,
                                    R.id.side_ui_right_anchor_container_stub));
                });
    }

    /**
     * On desktop the overlay stays visible while the omnibox is focused, so the suggestions popover
     * must be drawn above it.
     */
    @Test
    @MediumTest
    public void testOmniboxSuggestionsPopoverDrawnAboveOverlay() {
        WebPageStation page = mTestRule.startOnBlankPage();
        OmniboxFacility omnibox = page.openOmnibox();
        showOverlay(true);

        CriteriaHelper.pollUiThread(
                () -> {
                    ViewGroup coordinator = mTestRule.getActivity().findViewById(R.id.coordinator);
                    ActorOverlayZOrderTestUtil.checkOmniboxSuggestionsDrawnAboveOverlay(
                            coordinator);
                });

        omnibox.pressBackTo().exitFacility();
    }

    private void showOverlay(boolean visible) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ActorOverlayCoordinator coordinator = getActorOverlayCoordinator();
                    assertNotNull("Actor overlay coordinator is not initialized", coordinator);
                    coordinator.showOverlayForTesting(visible);
                });
    }

    /** Returns null if the activity or the Actor overlay has not been set up. */
    private @Nullable ActorOverlayCoordinator getActorOverlayCoordinator() {
        ChromeTabbedActivity activity = mTestRule.getActivity();
        if (activity == null || activity.isActivityFinishingOrDestroyed()) return null;
        TabbedRootUiCoordinator rootUiCoordinator =
                (TabbedRootUiCoordinator) activity.getRootUiCoordinatorForTesting();
        return rootUiCoordinator == null
                ? null
                : rootUiCoordinator.getActorOverlayCoordinatorForTesting();
    }
}
