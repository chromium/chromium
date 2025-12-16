// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.chromium.base.test.transit.Triggers.noopTo;

import android.app.Activity;
import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.build.annotations.NullMarked;

/**
 * Finds views for tests and return handles for them.
 *
 * <p>waitForView() methods wait for Views to exist, be visible, displayed and enabled before
 * returning.
 *
 * <p>Returns {@link ViewCarryOn}s to get/interact with them.
 */
@NullMarked
public class ViewFinder {
    /**
     * Waits for a View of a specific subclass of View that matches |matcher| in one of the
     * |activity|'s subwindows.
     *
     * @param <ViewT> the type of View to find.
     * @return A {@link ViewCarryOn} to get/interact with the ViewT.
     */
    public static <ViewT extends View> ViewCarryOn<ViewT> waitForView(
            Class<ViewT> viewClass, Activity activity, Matcher<View> matcher) {
        return noopTo().pickUpCarryOn(
                        ViewCarryOn.create(
                                viewClass,
                                matcher,
                                ViewElement.rootSpecOption(RootSpec.activityRoot(activity))));
    }

    /**
     * Waits for a View that matches |matcher| in one of the |activity|'s subwindows.
     *
     * @return A {@link ViewCarryOn} to get/interact with the View.
     */
    public static ViewCarryOn<View> waitForView(Activity activity, Matcher<View> matcher) {
        return waitForView(View.class, activity, matcher);
    }

    /**
     * Waits for a View of a specific subclass of View that matches |matcher| in any root.
     *
     * @param <ViewT> the type of View to find.
     * @return A {@link ViewCarryOn} to get/interact with the ViewT.
     */
    public static <ViewT extends View> ViewCarryOn<ViewT> waitForView(
            Class<ViewT> viewClass, Matcher<View> matcher) {
        return noopTo().pickUpCarryOn(
                        ViewCarryOn.create(
                                viewClass,
                                matcher,
                                ViewElement.rootSpecOption(RootSpec.anyRoot())));
    }

    /**
     * Waits for a View that matches |matcher| in any root.
     *
     * @return A {@link ViewCarryOn} to get/interact with the View.
     */
    public static ViewCarryOn<View> waitForView(Matcher<View> matcher) {
        return waitForView(View.class, matcher);
    }
}
