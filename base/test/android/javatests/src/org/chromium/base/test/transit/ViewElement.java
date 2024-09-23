// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import androidx.annotation.Nullable;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.ViewConditions.DisplayedCondition;
import org.chromium.base.test.transit.ViewConditions.NotDisplayedAnymoreCondition;

/**
 * Represents a {@link ViewSpec} added to a {@link ConditionalState}.
 *
 * <p>{@link ViewSpec}s should be declared as constants, while {@link ViewElement}s are
 * created by calling {@link Elements.Builder#declareView(ViewSpec)}.
 *
 * <p>Generates ENTER and EXIT Conditions for the ConditionalState to ensure the ViewElement is in
 * the right state.
 */
public class ViewElement extends Element<View> {

    /**
     * Minimum percentage of the View that needs to be displayed for a ViewElement's enter
     * Conditions to be considered fulfilled.
     *
     * <p>Matches Espresso's preconditions for ViewActions like click().
     */
    public static final int MIN_DISPLAYED_PERCENT = 90;

    private final ViewSpec mViewSpec;
    private final Options mOptions;

    ViewElement(ViewSpec viewSpec, Options options) {
        super(
                "VE/"
                        + (options.mElementId != null
                                ? options.mElementId
                                : viewSpec.getMatcherDescription()));
        mViewSpec = viewSpec;
        mOptions = options;
    }

    /**
     * @return an Options builder to customize the ViewElement further.
     */
    public static ViewElement.Options.Builder newOptions() {
        return new Options().new Builder();
    }

    @Override
    public ConditionWithResult<View> createEnterCondition() {
        Matcher<View> viewMatcher = mViewSpec.getViewMatcher();
        DisplayedCondition.Options conditionOptions =
                DisplayedCondition.newOptions()
                        .withExpectEnabled(mOptions.mExpectEnabled)
                        .withDisplayingAtLeast(mOptions.mDisplayedPercentageRequired)
                        .build();
        return new DisplayedCondition(viewMatcher, conditionOptions);
    }

    @Override
    public @Nullable Condition createExitCondition() {
        if (mOptions.mScoped) {
            return new NotDisplayedAnymoreCondition(mViewSpec.getViewMatcher());
        } else {
            return null;
        }
    }

    /** Extra options for declaring ViewElements. */
    public static class Options {
        static final Options DEFAULT = new Options();
        protected boolean mScoped = true;
        protected boolean mExpectEnabled = true;
        protected String mElementId;
        protected Integer mDisplayedPercentageRequired = ViewElement.MIN_DISPLAYED_PERCENT;

        protected Options() {}

        public class Builder {
            public Options build() {
                return Options.this;
            }

            /** Don't except the View to necessarily disappear when exiting the ConditionalState. */
            public Builder unscoped() {
                mScoped = false;
                return this;
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

            /**
             * Changes the minimum percentage of the View that needs be displayed to fulfill the
             * enter Condition. Default is >=90% visible, which matches the minimum requirement for
             * ViewInteractions like click().
             */
            public Builder displayingAtLeast(int percentage) {
                mDisplayedPercentageRequired = percentage;
                return this;
            }
        }
    }

    /** Convenience {@link Options} setting unscoped(). */
    public static Options unscopedOption() {
        return newOptions().unscoped().build();
    }

    /** Convenience {@link Options} setting elementId(). */
    public static Options elementIdOption(String id) {
        return newOptions().elementId(id).build();
    }

    /** Convenience {@link Options} setting expectDisabled(). */
    public static Options expectDisabledOption() {
        return newOptions().expectDisabled().build();
    }

    /** Convenience {@link Options} setting displayingAtLeast(). */
    public static Options displayingAtLeastOption(int percentage) {
        return newOptions().displayingAtLeast(percentage).build();
    }
}
