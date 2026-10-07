// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

import org.hamcrest.Matchers;

import org.chromium.base.test.util.Criteria;
import org.chromium.chrome.R;

/**
 * Helpers for asserting where the Actor overlay sits in the root coordinator layout's draw order.
 *
 * <p>The overlay is layered purely by sibling order: within the coordinator, later siblings draw on
 * top. It must therefore be declared after the control container, so the handoff button can
 * straddle the line of death, and before the snackbar host, bottom app bar, tab switcher, messages,
 * bottom sheet and tab group dialog, so the scrim neither covers them nor swallows their touches.
 *
 * <p>Ids are resolved to the coordinator's direct child that contains them, so the differences in
 * nesting between {@code main.xml} and {@code main_forked_with_secondary_ui_container.xml} do not
 * matter to callers.
 */
final class ActorOverlayZOrderTestUtil {
    private ActorOverlayZOrderTestUtil() {}

    /**
     * Returns the index within {@code coordinator} of the direct child that contains the first of
     * {@code viewIds} that is present.
     *
     * <p>A {@link android.view.ViewStub} is replaced by its inflated view at the same index, so
     * pass both the stub id and the inflated id for views that may not be inflated yet.
     */
    static int childIndexOf(ViewGroup coordinator, int... viewIds) {
        for (int viewId : viewIds) {
            View view = coordinator.findViewById(viewId);
            if (view == null) continue;
            while (view.getParent() != coordinator) {
                ViewParent parent = view.getParent();
                assertTrue(
                        name(coordinator, viewId) + " is not a descendant of the coordinator",
                        parent instanceof View);
                view = (View) parent;
            }
            return coordinator.indexOfChild(view);
        }
        fail("None of these views are present: " + names(coordinator, viewIds));
        return -1;
    }

    /**
     * Asserts that the view at {@code lowerIndex} is drawn underneath the one at {@code
     * upperIndex}.
     */
    static void assertDrawnBelow(
            String lowerDescription, int lowerIndex, String upperDescription, int upperIndex) {
        assertTrue(
                lowerDescription
                        + " (child "
                        + lowerIndex
                        + ") must be drawn below "
                        + upperDescription
                        + " (child "
                        + upperIndex
                        + ")",
                lowerIndex < upperIndex);
    }

    /** Asserts the parts of the overlay's draw order that are the same in every root layout. */
    static void assertOverlayZOrder(ViewGroup coordinator) {
        int overlay = childIndexOf(coordinator, R.id.actor_overlay, R.id.actor_overlay_stub);

        assertDrawnBelow(
                "control container",
                childIndexOf(coordinator, R.id.control_container, R.id.control_container_stub),
                "actor overlay",
                overlay);

        // The overlay glow must not draw over snackbars. See crbug.com/517390259.
        assertDrawnBelow(
                "actor overlay",
                overlay,
                "snackbar container",
                childIndexOf(coordinator, R.id.bottom_container));
        assertDrawnBelow(
                "actor overlay",
                overlay,
                "bottom app bar",
                childIndexOf(
                        coordinator,
                        R.id.bottom_app_bar_container,
                        R.id.bottom_app_bar_container_stub));
        assertDrawnBelow(
                "actor overlay",
                overlay,
                "tab switcher",
                childIndexOf(
                        coordinator,
                        R.id.tab_switcher_view_holder,
                        R.id.tab_switcher_view_holder_stub));
        assertDrawnBelow(
                "actor overlay",
                overlay,
                "message container",
                childIndexOf(coordinator, R.id.message_container));
        assertDrawnBelow(
                "actor overlay",
                overlay,
                "bottom sheet container",
                childIndexOf(coordinator, R.id.sheet_container));
        assertDrawnBelow(
                "actor overlay",
                overlay,
                "tab group dialog container",
                childIndexOf(coordinator, R.id.tab_group_ui_dialog_container));
    }

    /**
     * Asserts the overlay is layered by sibling order alone.
     *
     * <p>Elevation applies across the whole coordinator, so lifting the overlay with it puts the
     * scrim above every later sibling too, not just the control container. See crbug.com/561618991.
     */
    static void assertOverlayIsNotElevated(ViewGroup coordinator) {
        View overlay = coordinator.findViewById(R.id.actor_overlay);
        assertNotNull("Actor overlay is not inflated", overlay);
        assertEquals(
                "Actor overlay must not raise itself with elevation; sibling order layers it",
                0f,
                overlay.getZ(),
                0f);
    }

    /**
     * Checks that the omnibox suggestions are drawn above the overlay. Uses {@link Criteria} so it
     * can be polled while the suggestions inflate.
     *
     * <p>When the suggestions are shown as a popover, the overlay stays visible behind them while
     * the omnibox is focused. The popover is lifted above the overlay by elevation rather than by
     * sibling order, and elevation only orders siblings, so the suggestions container must stay a
     * direct child of the coordinator.
     */
    static void checkOmniboxSuggestionsDrawnAboveOverlay(ViewGroup coordinator) {
        View overlay = coordinator.findViewById(R.id.actor_overlay);
        Criteria.checkThat("Actor overlay is not inflated", overlay, Matchers.notNullValue());
        View suggestions = coordinator.findViewById(R.id.omnibox_suggestions_container);
        Criteria.checkThat(
                "Omnibox suggestions are not inflated", suggestions, Matchers.notNullValue());
        Criteria.checkThat(
                "Omnibox suggestions must be a direct child of the coordinator so their elevation"
                        + " applies against the actor overlay",
                suggestions.getParent(),
                Matchers.is(coordinator));
        Criteria.checkThat(
                "Omnibox suggestions popover must be elevated above the actor overlay",
                suggestions.getZ(),
                Matchers.greaterThan(overlay.getZ()));
    }

    private static String name(ViewGroup coordinator, int viewId) {
        try {
            return coordinator.getResources().getResourceEntryName(viewId);
        } catch (Resources.NotFoundException e) {
            return "0x" + Integer.toHexString(viewId);
        }
    }

    private static String names(ViewGroup coordinator, int... viewIds) {
        StringBuilder builder = new StringBuilder();
        for (int viewId : viewIds) {
            if (builder.length() > 0) builder.append(", ");
            builder.append(name(coordinator, viewId));
        }
        return builder.toString();
    }
}
