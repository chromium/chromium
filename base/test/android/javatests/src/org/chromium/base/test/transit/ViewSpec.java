// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.Matchers.allOf;

import android.view.View;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import org.chromium.base.test.util.ViewPrinter;
import org.chromium.build.annotations.NullMarked;

import java.util.Arrays;

/**
 * A spec to generate ViewElements representing a view characteristic of a ConditionalState.
 *
 * @param <ViewT> the type of the View.
 */
@NullMarked
public class ViewSpec<ViewT extends View> {
    private final Matcher<View> mViewMatcher;
    private final Class<ViewT> mViewClass;
    private final String mMatcherDescription;

    /** Create a ViewSpec of a View that matches multiple Matchers<View>. */
    public static <ViewT extends View> ViewSpec<ViewT> viewSpec(
            Class<ViewT> viewClass, Matcher<View> viewMatcher) {
        return new ViewSpec<>(viewMatcher, viewClass);
    }

    /** Create a ViewSpec of a View that matches multiple Matchers<View>. */
    @SafeVarargs
    public static <ViewT extends View> ViewSpec<ViewT> viewSpec(
            Class<ViewT> viewClass, Matcher<View>... viewMatchers) {
        return new ViewSpec<>(allOf(viewMatchers), viewClass);
    }

    /** Create a ViewSpec from a Matcher<View>. */
    public static ViewSpec<View> viewSpec(Matcher<View> viewMatcher) {
        return new ViewSpec<>(viewMatcher, View.class);
    }

    /** Create a ViewSpec of a View that matches multiple Matchers<View>. */
    @SafeVarargs
    public static ViewSpec<View> viewSpec(Matcher<View>... viewMatchers) {
        return new ViewSpec<>(allOf(viewMatchers), View.class);
    }

    /** Create a ViewSpec for a descendant of this ViewSpec. */
    public final <ChildViewT extends View> ViewSpec<ChildViewT> descendant(
            Class<ChildViewT> viewClass, Matcher<View> viewMatcher) {
        return viewSpec(viewClass, viewMatcher, isDescendantOfA(mViewMatcher));
    }

    /** Create a ViewSpec for a descendant of this ViewSpec that matches multiple Matchers<View>. */
    @SafeVarargs
    public final ViewSpec<View> descendant(Matcher<View>... viewMatchers) {
        Matcher<View>[] allViewMatchers = Arrays.copyOf(viewMatchers, viewMatchers.length + 1);
        allViewMatchers[viewMatchers.length] = isDescendantOfA(mViewMatcher);
        return viewSpec(allViewMatchers);
    }

    /** Create a ViewSpec for a descendant of this ViewSpec that matches multiple Matchers<View>. */
    @SafeVarargs
    public final <ChildViewT extends View> ViewSpec<ChildViewT> descendant(
            Class<ChildViewT> viewClass, Matcher<View>... viewMatchers) {
        Matcher<View>[] allViewMatchers = Arrays.copyOf(viewMatchers, viewMatchers.length + 1);
        allViewMatchers[viewMatchers.length] = isDescendantOfA(mViewMatcher);
        return viewSpec(viewClass, allViewMatchers);
    }

    /** Create a ViewSpec for a descendant of this ViewSpec that matches multiple Matchers<View>. */
    @SafeVarargs
    public final ViewSpec<View> ancestor(Matcher<View>... viewMatchers) {
        Matcher<View>[] allViewMatchers = Arrays.copyOf(viewMatchers, viewMatchers.length + 1);
        allViewMatchers[viewMatchers.length] = hasDescendant(mViewMatcher);
        return viewSpec(allViewMatchers);
    }

    /** Create a ViewSpec for a descendant of this ViewSpec that matches multiple Matchers<View>. */
    @SafeVarargs
    public final <ChildViewT extends View> ViewSpec<ChildViewT> ancestor(
            Class<ChildViewT> viewClass, Matcher<View>... viewMatchers) {
        Matcher<View>[] allViewMatchers = Arrays.copyOf(viewMatchers, viewMatchers.length + 1);
        allViewMatchers[viewMatchers.length] = hasDescendant(mViewMatcher);
        return viewSpec(viewClass, allViewMatchers);
    }

    /** Creates a ViewSpec that matches this ViewSpec _and_ another Matcher<View>. */
    public final ViewSpec<View> and(Matcher<View> viewMatcher) {
        return viewSpec(viewMatcher, mViewMatcher);
    }

    /** Creates a ViewSpec that matches this ViewSpec _and_ multiple other Matchers<View>. */
    @SafeVarargs
    public final ViewSpec<View> and(Matcher<View>... viewMatchers) {
        Matcher<View>[] allViewMatchers = Arrays.copyOf(viewMatchers, viewMatchers.length + 1);
        allViewMatchers[viewMatchers.length] = mViewMatcher;
        return viewSpec(allViewMatchers);
    }

    private ViewSpec(Matcher<View> viewMatcher, Class<ViewT> viewClass) {
        mViewMatcher = viewMatcher;
        mViewClass = viewClass;

        // Capture the description as soon as possible to compare ViewSpecs added to different
        // states by their description. Espresso Matcher descriptions are not stable: the integer
        // resource ids are translated when a View is provided. See examples in
        // https://crbug.com/41494895#comment7.
        mMatcherDescription = removeResolvedIds(StringDescription.toString(mViewMatcher));
    }

    private static String removeResolvedIds(String matcherDescription) {
        // Replace:
        // "VE/view.getId() is <2130773232/org.chromium.chrome.tests:id/hub_toolbar>"
        // with:
        // "VE/view.getId() is <2130773232>"

        // Generated ids have at least 8 digits, since they are >= 0xffffff (16777215)
        return matcherDescription.replaceAll("<([0-9]{8,})/.*>", "<$1>");
    }

    /**
     * @return the Matcher<View> used to create this element
     */
    public Matcher<View> getViewMatcher() {
        return mViewMatcher;
    }

    /**
     * @return the Class<View> used to create this element
     */
    public Class<ViewT> getViewClass() {
        return mViewClass;
    }

    /**
     * @return a stable String describing the Matcher<View>.
     */
    public String getMatcherDescription() {
        return mMatcherDescription;
    }

    /**
     * Print the whole View hierarchy that contains the View matched to this ViewElement.
     *
     * <p>For debugging.
     */
    public void printFromRoot() {
        Espresso.onView(mViewMatcher)
                .perform(
                        new ViewAction() {
                            @Override
                            public Matcher<View> getConstraints() {
                                return instanceOf(View.class);
                            }

                            @Override
                            public String getDescription() {
                                return "print the View hierarchy for debugging";
                            }

                            @Override
                            public void perform(UiController uiController, View view) {
                                ViewPrinter.printView(view.getRootView());
                            }
                        });
    }
}
