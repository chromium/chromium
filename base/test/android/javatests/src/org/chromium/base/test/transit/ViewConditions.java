// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayingAtLeast;

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

/** {@link Condition}s related to Android {@link View}s. */
public class ViewConditions {
    /**
     * Fulfilled when a single matching View exists and is displayed, but ignored if |gate| returns
     * true.
     */
    public static class GatedDisplayedCondition extends InstrumentationThreadCondition {

        private final DisplayedCondition mDisplayedCondition;
        private final Condition mGate;

        public GatedDisplayedCondition(
                Matcher<View> matcher, Condition gate, DisplayedCondition.Options options) {
            super();
            mDisplayedCondition = new DisplayedCondition(matcher, options);
            mGate = gate;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() throws Exception {
            ConditionStatus gateStatus = mGate.check();
            String gateMessage = gateStatus.getMessageAsGate();
            if (gateStatus.isAwaiting()) {
                return notFulfilled(gateMessage);
            }

            if (!gateStatus.isFulfilled()) {
                return fulfilled(gateMessage);
            }

            ConditionStatus status = mDisplayedCondition.check();
            status.amendMessage(gateMessage);
            return status;
        }

        @Override
        public String buildDescription() {
            return String.format(
                    "%s (if %s)", mDisplayedCondition.buildDescription(), mGate.buildDescription());
        }
    }

    /** Fulfilled when a single matching View exists and is displayed. */
    public static class DisplayedCondition extends InstrumentationThreadCondition {
        private final Matcher<View> mMatcher;
        private final Options mOptions;
        private View mViewMatched;

        private static final String VERBOSE_DESCRIPTION =
                "(view has effective visibility <VISIBLE> and view.getGlobalVisibleRect() covers at"
                        + " least <90> percent of the view's area)";
        private static final String SUCCINCT_DESCRIPTION = "(getGlobalVisibleRect() > 90%)";

        public DisplayedCondition(Matcher<View> matcher, Options options) {
            super();
            mMatcher = allOf(matcher, isDisplayingAtLeast(options.mDisplayedPercentageRequired));
            mOptions = options;
        }

        @Override
        public String buildDescription() {
            return "View: "
                    + createMatcherDescription(mMatcher, VERBOSE_DESCRIPTION, SUCCINCT_DESCRIPTION);
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            if (!ApplicationStatus.hasVisibleActivities()) {
                return awaiting("No visible activities");
            }

            ViewInteraction viewInteraction =
                    onView(mMatcher).withFailureHandler(RawFailureHandler.getInstance());
            String[] message = new String[1];
            try {
                viewInteraction.perform(
                        new ViewAction() {
                            @Override
                            public Matcher<View> getConstraints() {
                                return any(View.class);
                            }

                            @Override
                            public String getDescription() {
                                return "check exists and consistent";
                            }

                            @Override
                            public void perform(UiController uiController, View view) {
                                if (mViewMatched != null && mViewMatched != view) {
                                    message[0] =
                                            String.format(
                                                    "Matched a different view, was %s, now %s",
                                                    mViewMatched, view);
                                }
                                mViewMatched = view;
                            }
                        });
                if (mOptions.mExpectEnabled) {
                    if (!mViewMatched.isEnabled()) {
                        return notFulfilled("View displayed but disabled");
                    }
                } else { // Expected a displayed but disabled View.
                    if (mViewMatched.isEnabled()) {
                        return notFulfilled("View displayed but enabled");
                    }
                }
                return fulfilled(message[0]);
            } catch (NoMatchingViewException
                    | NoMatchingRootException
                    | AmbiguousViewMatcherException e) {
                if (mViewMatched != null) {
                    throw new IllegalStateException(
                            String.format(
                                    "Had matched a view (%s), but now got %s",
                                    mViewMatched, e.getClass().getSimpleName()),
                            e);
                }
                return notFulfilled(e.getClass().getSimpleName());
            }
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
        private static final String SUCCINCT_DESCRIPTION = "(getGlobalVisibleRect() > 0%)";

        public NotDisplayedAnymoreCondition(Matcher<View> matcher) {
            super();
            mMatcher = allOf(matcher, isDisplayed());
        }

        @Override
        public String buildDescription() {
            return "No more view: "
                    + createMatcherDescription(mMatcher, VERBOSE_DESCRIPTION, SUCCINCT_DESCRIPTION);
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

    /** Returns a less verbose view matcher description. */
    private static String createMatcherDescription(
            Matcher<View> matcher, String verboseString, String succinctString) {
        StringDescription d = new StringDescription();
        matcher.describeTo(d);
        String description = d.toString();
        return description.replace(verboseString, succinctString);
    }
}
