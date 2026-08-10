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

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.GlicTransitTestRule;

/** Integration test for ActorOverlay when running in Android Desktop/Side Panel mode. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.GLIC, ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL})
@DisableFeatures({ChromeFeatureList.TAB_BOTTOM_SHEET})
@Batch(Batch.PER_CLASS)
public class ActorOverlayDesktopPTTest {
    @Rule public final GlicTransitTestRule mTestRule = new GlicTransitTestRule();

    @Test
    @MediumTest
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

    private void showOverlay(boolean visible) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabbedRootUiCoordinator rootUiCoordinator =
                            (TabbedRootUiCoordinator)
                                    mTestRule.getActivity().getRootUiCoordinatorForTesting();
                    rootUiCoordinator
                            .getActorOverlayCoordinatorForTesting()
                            .showOverlayForTesting(visible);
                });
    }
}
