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
 * <p>Returns {@link ViewPresence}s to get/interact with them.
 */
@NullMarked
public class ViewFinder {
    /**
     * Waits for a View of a specific subclass of View that matches |matcher| in one of the
     * |activity|'s subwindows.
     *
     * <p>Pass |options| to override minimum required displayed %, enabled state, etc.
     *
     * @param <ViewT> the type of View to find.
     * @return A {@link ViewPresence} to get/interact with the ViewT.
     */
    public static <ViewT extends View> ViewPresence<ViewT> waitForView(
            Class<ViewT> viewClass,
            Activity activity,
            Matcher<View> matcher,
            ViewElement.Options options) {
        return noopTo().enterState(
                        ViewPresence.create(
                                viewClass,
                                matcher,
                                ViewElement.newOptions()
                                        .initFrom(options)
                                        .rootSpec(RootSpec.activityRoot(activity))
                                        .build()));
    }

    /**
     * Waits for a View of a specific subclass of View that matches |matcher| in one of the
     * |activity|'s subwindows.
     *
     * @param <ViewT> the type of View to find.
     * @return A {@link ViewPresence} to get/interact with the ViewT.
     */
    public static <ViewT extends View> ViewPresence<ViewT> waitForView(
            Class<ViewT> viewClass, Activity activity, Matcher<View> matcher) {
        return waitForView(viewClass, activity, matcher, ViewElement.Options.DEFAULT);
    }

    /**
     * Waits for a View that matches |matcher| in one of the |activity|'s subwindows.
     *
     * @return A {@link ViewPresence} to get/interact with the View.
     */
    public static ViewPresence<View> waitForView(Activity activity, Matcher<View> matcher) {
        return waitForView(View.class, activity, matcher);
    }

    /**
     * Waits for a View of a specific subclass of View that matches |matcher| in any root.
     *
     * <p>Pass |options| to override minimum required displayed %, enabled state, etc.
     *
     * @param <ViewT> the type of View to find.
     * @return A {@link ViewPresence} to get/interact with the ViewT.
     */
    public static <ViewT extends View> ViewPresence<ViewT> waitForView(
            Class<ViewT> viewClass, Matcher<View> matcher, ViewElement.Options options) {
        RootSpec rootSpec = options.mRootSpec;
        // If not specified, default to anyRoot().
        if (rootSpec == null) {
            rootSpec = RootSpec.anyRoot();
        }

        return noopTo().enterState(
                        ViewPresence.create(
                                viewClass,
                                matcher,
                                ViewElement.newOptions()
                                        .initFrom(options)
                                        .rootSpec(rootSpec)
                                        .build()));
    }

    /**
     * Waits for a View of a specific subclass of View that matches |matcher| in any root.
     *
     * @param <ViewT> the type of View to find.
     * @return A {@link ViewPresence} to get/interact with the ViewT.
     */
    public static <ViewT extends View> ViewPresence<ViewT> waitForView(
            Class<ViewT> viewClass, Matcher<View> matcher) {
        return waitForView(viewClass, matcher, ViewElement.Options.DEFAULT);
    }

    /**
     * Waits for a View that matches |matcher| in any root.
     *
     * <p>Pass |options| to override minimum required displayed %, enabled state, etc.
     *
     * @return A {@link ViewPresence} to get/interact with the View.
     */
    public static ViewPresence<View> waitForView(
            Matcher<View> matcher, ViewElement.Options options) {
        return waitForView(View.class, matcher, options);
    }

    /**
     * Waits for a View that matches |matcher| in any root.
     *
     * @return A {@link ViewPresence} to get/interact with the View.
     */
    public static ViewPresence<View> waitForView(Matcher<View> matcher) {
        return waitForView(View.class, matcher);
    }
}
