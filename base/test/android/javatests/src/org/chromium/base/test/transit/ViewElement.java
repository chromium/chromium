// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.matcher.RootMatchers.withDecorView;

import static org.hamcrest.CoreMatchers.is;

import static org.chromium.base.test.transit.Condition.whether;
import static org.chromium.base.test.transit.SimpleConditions.instrumentationThreadCondition;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.view.View;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.Root;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewAssertion;
import androidx.test.espresso.action.ViewActions;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
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
    private static final String TAG = "Transit";

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
                newDisplayedConditionOptions(mOptions).build();
        return new DisplayedCondition<>(
                viewMatcher,
                mViewSpec.getViewClass(),
                mOwner::determineActivityElement,
                conditionOptions);
    }

    static DisplayedCondition.Options.Builder newDisplayedConditionOptions(Options options) {
        return DisplayedCondition.newOptions()
                .withInDialogRoot(options.mInDialog)
                .withExpectEnabled(options.mExpectEnabled)
                .withExpectDisabled(options.mExpectDisabled)
                .withDisplayingAtLeast(options.mDisplayedPercentageRequired)
                .withSettleTimeMs(options.mInitialSettleTimeMs);
    }

    @Override
    public @Nullable Condition createExitCondition() {
        if (mOptions.mScoped) {
            return new NotDisplayedAnymoreCondition(this, mViewSpec.getViewMatcher());
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

    /**
     * Start a Transition by clicking this View.
     *
     * <p>Requires the View to be >90% displayed.
     */
    public TripBuilder clickTo() {
        if (mOptions.mDisplayedPercentageRequired > 90) {
            return performViewActionTo(ViewActions.click());
        } else {
            return performViewActionTo(ForgivingClickAction.forgivingClick());
        }
    }

    /** Start a Transition by long pressing this View. */
    public TripBuilder longPressTo() {
        return performViewActionTo(ViewActions.longClick());
    }

    /**
     * Start a Transition by clicking this View even if partially occluded.
     *
     * <p>Does not require the View to be >90% displayed like {@link #clickTo()}.
     */
    public TripBuilder clickEvenIfPartiallyOccludedTo() {
        return performViewActionTo(ForgivingClickAction.forgivingClick());
    }

    /** Start a Transition by typing |text| into this View char by char. */
    public TripBuilder typeTextTo(String text) {
        return new TripBuilder()
                .withContext(this)
                .withRunOnUiThread()
                .withTrigger(
                        () ->
                                KeyUtils.typeTextIntoView(
                                        InstrumentationRegistry.getInstrumentation(),
                                        value(),
                                        text));
    }

    /** Start a Transition by performing an Espresso ViewAction on this View. */
    public TripBuilder performViewActionTo(ViewAction action) {
        return new TripBuilder()
                .withContext(this)
                .withTrigger(
                        () -> {
                            Root rootMatched = getDisplayedCondition().getRootMatched();
                            assert rootMatched != null;

                            // If the window isn't focused, Espresso will wait for it to be focused
                            // as part of onView().perform().
                            //
                            // Call moveTaskToFront to focus on that window, which will
                            // asynchronously move it to the front.
                            //
                            // This is crucial in multiwindow. Even when two tasks are displayed
                            // side-by-side, only the window of the task last interacted with is
                            // focused.
                            if (!rootMatched.getDecorView().hasWindowFocus()) {
                                Log.i(TAG, "Root does not have window focus, moving to front.");
                                focusWindow(rootMatched);
                            }

                            Espresso.onView(mViewSpec.getViewMatcher())
                                    .inRoot(withDecorView(is(rootMatched.getDecorView())))
                                    .perform(action);
                        });
    }

    private void focusWindow(Root rootMatched) {
        Activity activity;

        ActivityElement<?> activityElement = mOwner.determineActivityElement();
        if (activityElement == null) {
            Context context = rootMatched.getDecorView().getContext();
            activity = ContextUtils.activityFromContext(context);

            if (activity == null) {
                Log.w(TAG, "Root is not tied to an Activity, cannot move it to front.");
                return;
            }
        } else {
            activity = activityElement.get();
            assert activity != null;
        }

        Triggers.runTo(
                        () -> {
                            ActivityManager activityManager =
                                    (ActivityManager)
                                            activity.getSystemService(Context.ACTIVITY_SERVICE);
                            activityManager.moveTaskToFront(activity.getTaskId(), 0);
                        })
                .withContext(this)
                .waitForAnd(
                        instrumentationThreadCondition(
                                "Root has window focus",
                                () -> whether(rootMatched.getDecorView().hasWindowFocus())))
                .pickUpCarryOn(new ViewSettledCarryOn(activityElement, this));
    }

    /** Trigger an Espresso ViewAssertion on this View. */
    public void check(ViewAssertion assertion) {
        Root rootMatched = getDisplayedCondition().getRootMatched();
        assert rootMatched != null;
        assert rootMatched.getDecorView().hasWindowFocus() : "Window is not focused";

        Espresso.onView(mViewSpec.getViewMatcher())
                .inRoot(withDecorView(is(rootMatched.getDecorView())))
                .check(assertion);
    }

    /** Creates a Condition fulfilled if the View matches the |matcher|. */
    public Condition matches(Matcher<View> matcher) {
        return new ViewElementMatchesCondition(this, matcher);
    }

    /** Returns the {@link Options} for this ViewElement. */
    public Options getOptions() {
        return mOptions;
    }

    /** Returns an {@link Options.Builder} copying the {@link Options} for this ViewElement. */
    public Options.Builder copyOptions() {
        return ViewElement.newOptions().initFrom(mOptions);
    }

    DisplayedCondition<ViewT> getDisplayedCondition() {
        assert mEnterCondition != null;
        return (DisplayedCondition<ViewT>) mEnterCondition;
    }

    /** Extra options for declaring ViewElements. */
    public static class Options {
        static final Options DEFAULT = new Options();
        protected boolean mScoped = true;
        protected boolean mInDialog;
        protected boolean mExpectEnabled = true;
        protected boolean mExpectDisabled;
        protected int mDisplayedPercentageRequired = ViewElement.MIN_DISPLAYED_PERCENT;
        protected int mInitialSettleTimeMs;

        protected Options() {}

        public class Builder {
            public Options build() {
                return Options.this;
            }

            /** Don't expect the View to necessarily disappear when exiting the ConditionalState. */
            public Builder unscoped() {
                mScoped = false;
                return this;
            }

            /** Expect the View to be in a dialog root. */
            public Builder inDialog() {
                mInDialog = true;
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

            /** Copy |optionsToClose|'s options into this instance. */
            public Builder initFrom(Options optionsToClone) {
                mScoped = optionsToClone.mScoped;
                mInDialog = optionsToClone.mInDialog;
                mExpectDisabled = optionsToClone.mExpectDisabled;
                mExpectEnabled = optionsToClone.mExpectEnabled;
                mDisplayedPercentageRequired = optionsToClone.mDisplayedPercentageRequired;
                mInitialSettleTimeMs = optionsToClone.mInitialSettleTimeMs;
                return this;
            }
        }
    }

    /** Convenience {@link Options} setting unscoped(). */
    public static Options unscopedOption() {
        return newOptions().unscoped().build();
    }

    /** Convenience {@link Options} setting inDialog(). */
    public static Options inDialogOption() {
        return newOptions().inDialog().build();
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
