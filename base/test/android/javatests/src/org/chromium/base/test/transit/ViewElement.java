// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.hamcrest.Matchers.allOf;

import android.view.View;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewInteraction;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

/** An Element representing a view characteristic of a ConditionalState. */
public class ViewElement {

    private final Matcher<View> mViewMatcher;
    private final boolean mOwned;
    private final String mViewMatcherDescription;

    /**
     * Create a shared-ownership ViewElement that matches |viewMatcher|.
     *
     * <p>Shared-ownership ViewElements should be gone after the ConditionalState is FINISHED when
     * transitioning to a ConditionalState that does not own declare a ViewElement with the same
     * matcher.
     */
    public static ViewElement sharedViewElement(Matcher<View> viewMatcher) {
        return new ViewElement(viewMatcher, /* owned= */ true);
    }

    /**
     * Create an unowned ViewElement that matches |viewMatcher|.
     *
     * <p>Unowned ViewElements are the most permissive; they may or may not be gone after the
     * ConditionalState is FINISHED.
     */
    public static ViewElement unownedViewElement(Matcher<View> viewMatcher) {
        return new ViewElement(viewMatcher, /* owned= */ false);
    }

    private ViewElement(Matcher<View> viewMatcher, boolean owned) {
        mViewMatcher = viewMatcher;
        mOwned = owned;

        // Capture the description as soon as possible to compare ViewElements added to different
        // states by their description. Espresso Matcher descriptions are not stable; the integer
        // resource ids are translated when a View is provided. See examples in
        // https://crbug.com/41494895#comment7.
        mViewMatcherDescription = StringDescription.toString(mViewMatcher);
    }

    String getViewMatcherDescription() {
        return mViewMatcherDescription;
    }

    /**
     * @return the Matcher<View> used to create this element
     */
    public Matcher<View> getViewMatcher() {
        return mViewMatcher;
    }

    boolean isOwned() {
        return mOwned;
    }

    /**
     * Start an Espresso interaction with a displayed View that matches this ViewElement's Matcher.
     */
    public ViewInteraction onView() {
        return Espresso.onView(allOf(mViewMatcher, isDisplayed()));
    }

    /**
     * Perform an Espresso ViewAction on a displayed View that matches this ViewElement's Matcher.
     */
    public ViewInteraction perform(ViewAction action) {
        return onView().perform(action);
    }
}
