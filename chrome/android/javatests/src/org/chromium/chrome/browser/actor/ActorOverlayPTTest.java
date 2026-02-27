// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.not;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.ui.SnackbarFacility;

/** Integration test for ActorOverlay. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ActorOverlayPTTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.GLIC_ACTOR_UI)
    public void testActorOverlayIsInflated() {
        mActivityTestRule.startOnBlankPage();
        onView(withId(R.id.actor_overlay)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.GLIC_ACTOR_UI)
    public void testOverlayVisibility() {
        mActivityTestRule.startOnBlankPage();
        showOverlay(true);
        onView(withId(R.id.actor_overlay)).check(matches(isDisplayed()));

        showOverlay(false);
        onView(withId(R.id.actor_overlay)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.GLIC_ACTOR_UI)
    public void testOverlayClickShowsSnackbar() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        showOverlay(true);

        // Click the overlay and wait for the snackbar to appear.
        // We don't verify the exact message because the string resource ID is not easily available.
        // SnackbarFacility will wait for a view with R.id.snackbar_message to appear.
        page.runTo(() -> onView(withId(R.id.actor_overlay)).perform(click()))
                .enterFacility(new SnackbarFacility<>(null, SnackbarFacility.NO_BUTTON));
    }

    private void showOverlay(boolean visible) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabbedRootUiCoordinator rootUiCoordinator =
                            (TabbedRootUiCoordinator)
                                    mActivityTestRule
                                            .getActivity()
                                            .getRootUiCoordinatorForTesting();
                    rootUiCoordinator
                            .getActorOverlayCoordinatorForTesting()
                            .showOverlayForTesting(visible);
                });
    }
}
