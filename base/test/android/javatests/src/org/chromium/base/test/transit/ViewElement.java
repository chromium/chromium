// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewAssertion;
import androidx.test.espresso.action.ViewActions;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ViewConditions.DisplayedCondition;
import org.chromium.base.test.transit.ViewConditions.NotDisplayedAnymoreCondition;
import org.chromium.base.test.util.ForgivingClickAction;
import org.chromium.base.test.util.KeyUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Represents a {@link ViewSpec} added to a {@link ConditionalState}.
 *
 * <p>{@link ViewSpec}s should be declared as constants, while {@link ViewElement}s are created by
 * calling {@link ConditionalState#declareView(Matcher)}.
 *
 * <p>Generates ENTER and EXIT Conditions for the ConditionalState to ensure the ViewElement is in
 * the right state.
 *
 * @param <ViewT> the type of the View.
 */
@NullMarked
public class ViewElement<ViewT extends View> extends Element<ViewT> {

    /**
     * Minimum percentage of the View that needs to be displayed for a ViewElement's enter
     * Conditions to be considered fulfilled.
     *
     * <p>Matches Espresso's preconditions for ViewActions like click().
     */
    public static final int MIN_DISPLAYED_PERCENT = 90;

    private final ViewSpec<ViewT> mViewSpec;
    private final Options mOptions;

    ViewElement(ViewSpec<ViewT> viewSpec, Options options) {
        super("VE/" + viewSpec.getMatcherDescription());
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
    public @Nullable ConditionWithResult<ViewT> createEnterCondition() {
        Matcher<View> viewMatcher = mViewSpec.getViewMatcher();
        DisplayedCondition.Options conditionOptions =
                DisplayedCondition.newOptions()
                        .withExpectEnabled(mOptions.mExpectEnabled)
                        .withExpectDisabled(mOptions.mExpectDisabled)
                        .withDisplayingAtLeast(mOptions.mDisplayedPercentageRequired)
                        .withSettleTimeMs(mOptions.mInitialSettleTimeMs)
                        .build();
        return new DisplayedCondition<>(viewMatcher, mViewSpec.getViewClass(), conditionOptions);
    }

    /**
     * Create a {@link DisplayedCondition} like the enter Condition, but also waiting for the View
     * to settle (no changes to its rect coordinates) for 1 second.
     */
    public ConditionWithResult<ViewT> createSettleCondition() {
        Matcher<View> viewMatcher = mViewSpec.getViewMatcher();
        DisplayedCondition.Options conditionOptions =
                DisplayedCondition.newOptions()
                        .withExpectEnabled(mOptions.mExpectEnabled)
                        .withExpectDisabled(mOptions.mExpectDisabled)
                        .withDisplayingAtLeast(mOptions.mDisplayedPercentageRequired)
                        .withSettleTimeMs(1000)
                        .build();
        return new DisplayedCondition<>(viewMatcher, mViewSpec.getViewClass(), conditionOptions);
    }

    @Override
    public @Nullable Condition createExitCondition() {
        if (mOptions.mScoped) {
            return new NotDisplayedAnymoreCondition(mViewSpec.getViewMatcher());
        } else {
            return null;
        }
    }

    /** Returns the {@link ViewSpec} for this ViewElement. */
    public ViewSpec<ViewT> getViewSpec() {
        return mViewSpec;
    }

    /** Returns a {@link ViewSpec} to declare a descandant of this ViewElement. */
    @SafeVarargs
    public final ViewSpec<View> descendant(Matcher<View>... viewMatcher) {
        return mViewSpec.descendant(viewMatcher);
    }

    /** Returns a {@link ViewSpec} to declare a descandant of this ViewElement. */
    @SafeVarargs
    public final <DescendantViewT extends View> ViewSpec<DescendantViewT> descendant(
            Class<DescendantViewT> viewClass, Matcher<View>... viewMatcher) {
        return mViewSpec.descendant(viewClass, viewMatcher);
    }

    /** Returns a {@link ViewSpec} to declare an ancestor of this ViewElement. */
    @SafeVarargs
    public final ViewSpec<View> ancestor(Matcher<View>... viewMatcher) {
        return mViewSpec.ancestor(viewMatcher);
    }

    /** Returns a {@link ViewSpec} to declare an ancestor of this ViewElement. */
    @SafeVarargs
    public final <DescendantViewT extends View> ViewSpec<DescendantViewT> ancestor(
            Class<DescendantViewT> viewClass, Matcher<View>... viewMatcher) {
        return mViewSpec.ancestor(viewClass, viewMatcher);
    }

    /** Trigger an Espresso action on this View. */
    public Transition.Trigger getPerformTrigger(ViewAction action) {
        return () -> Espresso.onView(mViewSpec.getViewMatcher()).perform(action);
    }

    /**
     * Trigger an Espresso click on this View.
     *
     * <p>Requires it to be >90% displayed.
     */
    public Transition.Trigger getClickTrigger() {
        return getPerformTrigger(ViewActions.click());
    }

    /**
     * Trigger an Espresso click on this View.
     *
     * <p>Does not require the View to be > 90% displayed like {@link #getClickTrigger()}.
     *
     * <p>TODO(crbug.com/411140394): Rename clickTrigger() to strictClickTrigger() and rename this
     * to clickTrigger().
     */
    public Transition.Trigger getForgivingClickTrigger() {
        return getPerformTrigger(ForgivingClickAction.forgivingClick());
    }

    /**
     * Trigger an Espresso long press on this View.
     *
     * <p>Requires it to be >90% displayed.
     */
    public Transition.Trigger getLongPressTrigger() {
        return getPerformTrigger(ViewActions.longClick());
    }

    /** Send keycodes to the View to type |text|. */
    public Transition.Trigger getTypeTextTrigger(String text) {
        return () ->
                ThreadUtils.runOnUiThread(
                        () ->
                                KeyUtils.typeTextIntoView(
                                        InstrumentationRegistry.getInstrumentation(), get(), text));
    }

    /** Trigger an Espresso ViewAssertion on this View. */
    public void check(ViewAssertion assertion) {
        Espresso.onView(mViewSpec.getViewMatcher()).check(assertion);
    }

    /** Extra options for declaring ViewElements. */
    public static class Options {
        static final Options DEFAULT = new Options();
        protected boolean mScoped = true;
        protected boolean mExpectEnabled = true;
        protected boolean mExpectDisabled;
        protected int mDisplayedPercentageRequired = ViewElement.MIN_DISPLAYED_PERCENT;
        protected int mInitialSettleTimeMs;

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
                mExpectDisabled = true;
                return this;
            }

            /** Do not expect the View to be necessarily disabled or enabled. */
            public Builder allowDisabled() {
                mExpectEnabled = false;
                mExpectDisabled = false;
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

            /** Waits for the View's rect to stop moving. */
            public Builder initialSettleTime(int settleTimeMs) {
                mInitialSettleTimeMs = settleTimeMs;
                return this;
            }
        }
    }

    /** Convenience {@link Options} setting unscoped(). */
    public static Options unscopedOption() {
        return newOptions().unscoped().build();
    }

    /** Convenience {@link Options} setting expectDisabled(). */
    public static Options expectDisabledOption() {
        return newOptions().expectDisabled().build();
    }

    /** Convenience {@link Options} setting allowDisabled(). */
    public static Options allowDisabledOption() {
        return newOptions().allowDisabled().build();
    }

    /** Convenience {@link Options} setting displayingAtLeast(). */
    public static Options displayingAtLeastOption(int percentage) {
        return newOptions().displayingAtLeast(percentage).build();
    }
}
