// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.hamcrest.Matchers.allOf;

import android.view.View;

import androidx.annotation.IntDef;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewInteraction;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** An Element representing a view characteristic of a ConditionalState. */
public class ViewElement {
    @IntDef({Scope.CONDITIONAL_STATE_SCOPED, Scope.SHARED, Scope.UNSCOPED})
    @Retention(RetentionPolicy.SOURCE)
    @interface Scope {
        int CONDITIONAL_STATE_SCOPED = 0;
        int SHARED = 1;
        int UNSCOPED = 2;
    }

    private final Matcher<View> mViewMatcher;
    private final @Scope int mScope;
    private final String mViewMatcherDescription;

    /** Alias for {@link #sharedViewElement(Matcher)} as the default way to declare ViewElements. */
    public static ViewElement viewElement(Matcher<View> viewMatcher) {
        return sharedViewElement(viewMatcher);
    }

    /**
     * Create a shared ViewElement that matches |viewMatcher|.
     *
     * <p>ViewElements are matched to View instances as ENTER conditions.
     *
     * <p>Shared ViewElements add an EXIT condition that the View instance matched is gone unless
     * transitioning to a ConditionalState that declares a ViewElement with an equal Matcher<View>.
     *
     * <p>This is a good default method to the declare ViewElements; when in doubt, use this.
     */
    public static ViewElement sharedViewElement(Matcher<View> viewMatcher) {
        return new ViewElement(viewMatcher, Scope.SHARED);
    }

    /**
     * Create a ConditionalState-scoped ViewElement that matches |viewMatcher|.
     *
     * <p>ViewElements are matched to View instances as ENTER conditions.
     *
     * <p>ConditionalState-scoped ViewElements are the most restrictive; they generate an EXIT
     * condition that the View instance matched is gone.
     */
    public static ViewElement scopedViewElement(Matcher<View> viewMatcher) {
        return new ViewElement(viewMatcher, Scope.CONDITIONAL_STATE_SCOPED);
    }

    /**
     * Create an unscoped ViewElement that matches |viewMatcher|.
     *
     * <p>ViewElements are matched to View instances as ENTER conditions.
     *
     * <p>Unscoped ViewElements are the most permissive; they do not generate EXIT conditions,
     * therefore they may or may not be gone.
     */
    public static ViewElement unscopedViewElement(Matcher<View> viewMatcher) {
        return new ViewElement(viewMatcher, Scope.UNSCOPED);
    }

    private ViewElement(Matcher<View> viewMatcher, @Scope int scope) {
        mViewMatcher = viewMatcher;
        mScope = scope;

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

    @Scope
    int getScope() {
        return mScope;
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
