// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.any;

import android.view.View;

import androidx.test.espresso.AmbiguousViewMatcherException;
import androidx.test.espresso.NoMatchingRootException;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewInteraction;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.RawFailureHandler;
import org.chromium.base.test.util.ViewPrinter;

import java.util.ArrayList;

/** {@link Condition}s related to Android {@link View}s. */
public class ViewConditions {

    private static final ViewPrinter.Options PRINT_SHALLOW_WITH_BOUNDS =
            new ViewPrinter.Options()
                    .setPrintChildren(false)
                    .setPrintNonVisibleViews(true)
                    .setPrintViewBounds(true);
    private static final ViewPrinter.Options PRINT_SHALLOW =
            new ViewPrinter.Options().setPrintChildren(false).setPrintNonVisibleViews(true);

    /** Fulfilled when a single matching View exists and is displayed. */
    public static class DisplayedCondition extends ConditionWithResult<View> {
        private final Matcher<View> mMatcher;
        private final Options mOptions;
        private View mViewMatched;

        public DisplayedCondition(Matcher<View> matcher, Options options) {
            super(/* isRunOnUiThread= */ false);
            mMatcher = matcher /*, withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)*/;
            mOptions = options;
        }

        @Override
        public String buildDescription() {
            StringBuilder description = new StringBuilder();
            description
                    .append("View: ")
                    .append(StringDescription.toString(mMatcher))
                    .append(" (>= ")
                    .append(mOptions.mDisplayedPercentageRequired)
                    .append("% displayed, ")
                    .append(mOptions.mExpectEnabled ? "enabled" : "disabled")
                    .append(")");
            return description.toString();
        }

        @Override
        protected ConditionStatusWithResult<View> resolveWithSuppliers() {
            if (!ApplicationStatus.hasVisibleActivities()) {
                return awaiting("No visible activities").withoutResult();
            }

            ViewInteraction viewInteraction =
                    onView(mMatcher).withFailureHandler(RawFailureHandler.getInstance());
            ArrayList<String> messages = new ArrayList<>();
            try {
                viewInteraction.perform(
                        new ViewAction() {
                            @Override
                            public Matcher<View> getConstraints() {
                                return any(View.class);
                            }

                            @Override
                            public String getDescription() {
                                return "check existence, visibility and displayed percentage";
                            }

                            @Override
                            public void perform(UiController uiController, View view) {
                                mViewMatched = view;
                            }
                        });
            } catch (NoMatchingViewException
                    | NoMatchingRootException
                    | AmbiguousViewMatcherException e) {
                return notFulfilled(e.getClass().getSimpleName()).withoutResult();
            }

            // Assume found a View, or NoMatchingViewException would be thrown.
            boolean fulfilled = true;
            messages.add(ViewPrinter.describeView(mViewMatched, PRINT_SHALLOW_WITH_BOUNDS));

            View view = mViewMatched;
            int visibility = view.getVisibility();
            if (visibility != View.VISIBLE) {
                fulfilled = false;
                messages.add(String.format("visibility = %s", visibilityIntToString(visibility)));
            } else {
                while (view.getParent() instanceof View) {
                    view = (View) view.getParent();
                    visibility = view.getVisibility();
                    if (visibility != View.VISIBLE) {
                        fulfilled = false;
                        messages.add(
                                String.format(
                                        "visibility of ancestor [%s] = %s",
                                        ViewPrinter.describeView(view, PRINT_SHALLOW),
                                        visibilityIntToString(visibility)));
                        break;
                    }
                }
            }

            // Since perform() above did not throw an Exception, mViewMatched is non-null.
            if (mOptions.mDisplayedPercentageRequired > 0) {
                DisplayedPortion portion = DisplayedPortion.ofView(mViewMatched);
                if (portion.mPercentage < mOptions.mDisplayedPercentageRequired) {
                    fulfilled = false;
                    messages.add(
                            String.format(
                                    "%d%% displayed, expected >= %d%%",
                                    portion.mPercentage, mOptions.mDisplayedPercentageRequired));
                    messages.add("% displayed calculation: " + portion);
                } else {
                    messages.add(String.format("%d%% displayed", portion.mPercentage));
                }
            }
            if (mOptions.mExpectEnabled) {
                if (!mViewMatched.isEnabled()) {
                    fulfilled = false;
                    messages.add("disabled");
                }
            } else { // Expected a displayed but disabled View.
                if (mViewMatched.isEnabled()) {
                    fulfilled = false;
                    messages.add("enabled");
                }
            }

            String message = String.join("; ", messages);
            if (fulfilled) {
                return fulfilled(message).withResult(mViewMatched);
            } else {
                return notFulfilled(message).withoutResult();
            }
        }

        private static String visibilityIntToString(int visibility) {
            return switch (visibility) {
                case View.VISIBLE -> "VISIBLE";
                case View.INVISIBLE -> "INVISIBLE";
                case View.GONE -> "GONE";
                default -> "invalid";
            };
        }

        /**
         * @return an Options builder to customize the ViewCondition.
         */
        public static Options.Builder newOptions() {
            return new Options().new Builder();
        }

        /** Extra options for declaring DisplayedCondition. */
        public static class Options {
            boolean mExpectEnabled = true;
            int mDisplayedPercentageRequired = ViewElement.MIN_DISPLAYED_PERCENT;

            private Options() {}

            public class Builder {
                public Options build() {
                    return Options.this;
                }

                /** Whether the View is expected to be enabled or disabled. */
                public Builder withExpectEnabled(boolean state) {
                    mExpectEnabled = state;
                    return this;
                }

                /** Minimum percentage of the View that needs to be displayed. */
                public Builder withDisplayingAtLeast(int displayedPercentageRequired) {
                    mDisplayedPercentageRequired = displayedPercentageRequired;
                    return this;
                }
            }
        }
    }

    /** Fulfilled when no matching Views exist and are displayed. */
    public static class NotDisplayedAnymoreCondition extends InstrumentationThreadCondition {
        private final Matcher<View> mMatcher;

        private static final String VERBOSE_DESCRIPTION =
                "(view has effective visibility <VISIBLE> and view.getGlobalVisibleRect() to return"
                        + " non-empty rectangle)";
        private static final String SUCCINCT_DESCRIPTION = "isDisplayed()";

        public NotDisplayedAnymoreCondition(Matcher<View> matcher) {
            super();
            mMatcher = allOf(matcher, isDisplayed());
        }

        @Override
        public String buildDescription() {
            return "No more view: "
                    + StringDescription.toString(mMatcher)
                            .replace(VERBOSE_DESCRIPTION, SUCCINCT_DESCRIPTION);
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            if (!ApplicationStatus.hasVisibleActivities()) {
                return fulfilled("No visible activities");
            }

            try {
                onView(mMatcher)
                        .withFailureHandler(RawFailureHandler.getInstance())
                        .check(doesNotExist());
                return fulfilled();
            } catch (AssertionError e) {
                return notFulfilled();
            }
        }
    }
}
