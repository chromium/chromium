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
    private final String mId;
    private final Options mOptions;

    /** Alias for {@link #sharedViewElement(Matcher)} as the default way to declare ViewElements. */
    public static ViewElement viewElement(Matcher<View> viewMatcher) {
        return sharedViewElement(viewMatcher);
    }

    /**
     * Version of {@link #sharedViewElement(Matcher, Options)} using default Options.
     *
     * <p>This is a good default method to the declare ViewElements; when in doubt, use this.
     */
    public static ViewElement sharedViewElement(Matcher<View> viewMatcher) {
        return new ViewElement(viewMatcher, Scope.SHARED, Options.DEFAULT);
    }

    /**
     * Create a shared ViewElement that matches |viewMatcher|.
     *
     * <p>ViewElements are matched to View instances as ENTER conditions.
     *
     * <p>Shared ViewElements add an EXIT condition that no View is matched unless transitioning to
     * a ConditionalState that declares a ViewElement with the same id (which usually means an equal
     * Matcher<View>).
     */
    public static ViewElement sharedViewElement(Matcher<View> viewMatcher, Options options) {
        return new ViewElement(viewMatcher, Scope.SHARED, options);
    }

    /** Version of {@link #scopedViewElement(Matcher, Options)} using default Options. */
    public static ViewElement scopedViewElement(Matcher<View> viewMatcher) {
        return new ViewElement(viewMatcher, Scope.CONDITIONAL_STATE_SCOPED, Options.DEFAULT);
    }

    /**
     * Create a ConditionalState-scoped ViewElement that matches |viewMatcher|.
     *
     * <p>ViewElements are matched to View instances as ENTER conditions.
     *
     * <p>ConditionalState-scoped ViewElements are the most restrictive; they generate an EXIT
     * condition that no View is matched under any circumstances.
     */
    public static ViewElement scopedViewElement(Matcher<View> viewMatcher, Options options) {
        return new ViewElement(viewMatcher, Scope.CONDITIONAL_STATE_SCOPED, options);
    }

    /** Version of {@link #unscopedViewElement(Matcher, Options)} using default Options. */
    public static ViewElement unscopedViewElement(Matcher<View> viewMatcher) {
        return new ViewElement(viewMatcher, Scope.UNSCOPED, Options.DEFAULT);
    }

    /**
     * Create an unscoped ViewElement that matches |viewMatcher|.
     *
     * <p>ViewElements are matched to View instances as ENTER conditions.
     *
     * <p>Unscoped ViewElements are the most permissive; they do not generate EXIT conditions,
     * therefore they may or may not be gone.
     */
    public static ViewElement unscopedViewElement(Matcher<View> viewMatcher, Options options) {
        return new ViewElement(viewMatcher, Scope.UNSCOPED, options);
    }

    private ViewElement(Matcher<View> viewMatcher, @Scope int scope, Options options) {
        mViewMatcher = viewMatcher;
        mScope = scope;

        if (options.mElementId != null) {
            // Use a custom id instead of the Matcher description.
            mId = options.mElementId;
        } else {
            // Capture the description as soon as possible to compare ViewElements added to
            // different
            // states by their description. Espresso Matcher descriptions are not stable; the
            // integer
            // resource ids are translated when a View is provided. See examples in
            // https://crbug.com/41494895#comment7.
            mId = StringDescription.toString(mViewMatcher);
        }

        mOptions = options;
    }

    String getId() {
        return "VE/" + mId;
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

    /**
     * @return the Options passed when declaring the ViewElement.
     */
    public Options getOptions() {
        return mOptions;
    }

    /**
     * @return an Options builder to customize the ViewElement further.
     */
    public static Options.Builder newOptions() {
        return new Options().new Builder();
    }

    /** Extra options for declaring ViewElements. */
    public static class Options {
        static final Options DEFAULT = new Options();
        protected boolean mExpectEnabled = true;
        protected String mElementId;

        protected Options() {}

        public class Builder {
            public Options build() {
                return Options.this;
            }

            /** Use a custom Element id instead of the Matcher<View> description. */
            public Builder elementId(String id) {
                mElementId = id;
                return this;
            }

            /**
             * Expect the View to be disabled instead of enabled.
             *
             * <p>This is different than passing an isEnabled() Matcher.If the matcher was, for
             * example |allOf(withId(ID), isEnabled())|, the exit condition would be considered
             * fulfilled if the View became disabled. Meanwhile, using this option makes the exit
             * condition only be considered fulfilled if no Views |withId(ID)|, enabled or not, were
             * displayed.
             */
            public Builder expectDisabled() {
                mExpectEnabled = false;
                return this;
            }
        }
    }
}
