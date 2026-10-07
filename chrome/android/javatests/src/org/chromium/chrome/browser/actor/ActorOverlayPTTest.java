// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.actor.ui.ActorOverlayCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.GlicTransitTestRule;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.ui.SnackbarFacility;

/** Integration test for ActorOverlay. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.GLIC, ChromeFeatureList.TAB_BOTTOM_SHEET})
@DisableFeatures({ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL})
@Batch(Batch.PER_CLASS)
public class ActorOverlayPTTest {
    @Rule public final GlicTransitTestRule mTestRule = new GlicTransitTestRule();

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
    public void testActorOverlayIsNotInflatedInitially() {
        mTestRule.startOnBlankPage();
        onView(withId(R.id.actor_overlay)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testOverlayVisibility() {
        mTestRule.startOnBlankPage();
        showOverlay(true);
        onView(withId(R.id.actor_overlay)).check(matches(isDisplayed()));

        showOverlay(false);
        onView(withId(R.id.actor_overlay)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testOverlayClickShowsSnackbar() {
        WebPageStation page = mTestRule.startOnBlankPage();
        showOverlay(true);

        // Click the overlay and wait for the snackbar to appear.
        // We don't verify the exact message because the string resource ID is not easily available.
        // SnackbarFacility will wait for a view with R.id.snackbar_message to appear.
        page.runTo(() -> onView(withId(R.id.actor_overlay)).perform(click()))
                .enterFacility(new SnackbarFacility<>(null, SnackbarFacility.NO_BUTTON));
    }

    @Test
    @MediumTest
    public void testBackPressShowsSnackbar() {
        WebPageStation page = mTestRule.startOnBlankPage();
        showOverlay(true);

        // Press back and wait for the snackbar to appear.
        page.pressBackTo().enterFacility(new SnackbarFacility<>(null, SnackbarFacility.NO_BUTTON));
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
                });
    }

    /**
     * The Actor overlay observes {@link
     * org.chromium.chrome.browser.ui.RootUiCoordinator#getOmniboxFocusStateSupplier()} to hide the
     * take-over button while the omnibox is focused, and to also suppress the overlay when the
     * suggestions are not drawn as an elevated popover above it. The suppression logic itself is
     * covered by ActorOverlayCoordinatorTest, which supplies the focus state directly; this test
     * covers the part that unit tests cannot, namely that the supplier the overlay is handed is
     * really driven by omnibox focus.
     */
    @Test
    @MediumTest
    public void testOmniboxFocusStateSupplierTracksOmniboxFocus() {
        WebPageStation page = mTestRule.startOnBlankPage();
        assertFalse("Omnibox should not be focused on startup", isOmniboxFocused());

        OmniboxFacility omnibox = page.openOmnibox();
        assertTrue(
                "Focusing the omnibox must update the supplier the Actor overlay observes",
                isOmniboxFocused());

        omnibox.pressBackTo().exitFacility();
        assertFalse(
                "Dismissing the omnibox must update the supplier the Actor overlay observes",
                isOmniboxFocused());
    }

    private boolean isOmniboxFocused() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mTestRule
                                .getActivity()
                                .getRootUiCoordinatorForTesting()
                                .getOmniboxFocusStateSupplier()
                                .get());
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
