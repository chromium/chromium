// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.any;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.View;

import androidx.test.espresso.AmbiguousViewMatcherException;
import androidx.test.espresso.NoMatchingRootException;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.matcher.ViewMatchers;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.RawFailureHandler;
import org.chromium.base.test.util.ViewPrinter;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;

/** {@link Condition}s related to Android {@link View}s. */
@NullMarked
public class ViewConditions {

    private static final ViewPrinter.Options PRINT_SHALLOW_WITH_BOUNDS =
            new ViewPrinter.Options()
                    .setPrintChildren(false)
                    .setPrintNonVisibleViews(true)
                    .setPrintViewBounds(true);
    private static final ViewPrinter.Options PRINT_SHALLOW =
            new ViewPrinter.Options().setPrintChildren(false).setPrintNonVisibleViews(true);

    /**
     * Fulfilled when a single matching View exists and is displayed.
     *
     * @param <ViewT> the type of the View.
     */
    public static class DisplayedCondition<ViewT extends View> extends ConditionWithResult<ViewT> {
        private final Matcher<View> mMatcher;
        private final Class<ViewT> mViewClass;
        private final Options mOptions;
        private @Nullable View mViewMatched;
        private int mPreviousViewX = Integer.MIN_VALUE;
        private int mPreviousViewY = Integer.MIN_VALUE;
        private int mPreviousViewWidth = Integer.MIN_VALUE;
        private int mPreviousViewHeight = Integer.MIN_VALUE;
        private long mLastChangeMs = -1;

        public DisplayedCondition(Matcher<View> matcher, Class<ViewT> viewClass, Options options) {
            super(/* isRunOnUiThread= */ false);
            mMatcher = matcher;
            mViewClass = viewClass;
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
                    .append("% displayed");
            if (mOptions.mSettleTimeMs > 0) {
                description.append(", settled for ").append(mOptions.mSettleTimeMs).append("ms");
            }
            if (mOptions.mExpectEnabled) {
                description.append(", enabled");
            }
            if (mOptions.mExpectDisabled) {
                description.append(", disabled");
            }
            description.append(")");
            return description.toString();
        }

        @Override
        protected ConditionStatusWithResult<ViewT> resolveWithSuppliers() {
            if (!ApplicationStatus.hasVisibleActivities()) {
                return awaiting("No visible activities").withoutResult();
            }

            // Match even views that are not visible so that visibility checking can be done with
            // more details later in this method.
            ArrayList<String> messages = new ArrayList<>();

            Supplier<ViewAction> findViewActionFactory =
                    () ->
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
                            };
            try {
                onView(mMatcher)
                        .withFailureHandler(RawFailureHandler.getInstance())
                        .perform(findViewActionFactory.get());
            } catch (NoMatchingViewException | NoMatchingRootException e) {
                return notFulfilled(e.getClass().getSimpleName()).withoutResult();
            } catch (AmbiguousViewMatcherException e) {
                // Found 2+ Views. Try again, but filtering only by effectively visible Views.
                // This avoids AmbiguousViewMatcherException when there is one VISIBLE but also
                // GONE views that match |mMatcher|.
                try {
                    onView(
                                    allOf(
                                            mMatcher,
                                            withEffectiveVisibility(
                                                    ViewMatchers.Visibility.VISIBLE)))
                            .withFailureHandler(RawFailureHandler.getInstance())
                            .perform(findViewActionFactory.get());
                } catch (NoMatchingViewException f) {
                    // Report the AmbiguousViewMatcherException with the GONE views.
                    return notFulfilled(
                                    e.getClass().getSimpleName()
                                            + " with GONE Views | "
                                            + e.getMessage())
                            .withoutResult();
                } catch (NoMatchingRootException f) {
                    return notFulfilled(f.getClass().getSimpleName()).withoutResult();
                } catch (AmbiguousViewMatcherException f) {
                    return notFulfilled(f.getClass().getSimpleName() + " | " + f.getMessage())
                            .withoutResult();
                }
            }

            // Assume found a View, or an exception would have been thrown above and
            // |notFulfilled()| would have been returned.
            assumeNonNull(mViewMatched);
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
            } else if (mOptions.mExpectDisabled) {
                if (mViewMatched.isEnabled()) {
                    fulfilled = false;
                    messages.add("enabled");
                }
            }

            if (mOptions.mSettleTimeMs > 0) {
                long nowMs = System.currentTimeMillis();
                int[] locationOnScreen = new int[2];
                mViewMatched.getLocationOnScreen(locationOnScreen);
                int newX = locationOnScreen[0];
                int newY = locationOnScreen[1];
                int newWidth = view.getWidth();
                int newHeight = view.getHeight();
                if (mPreviousViewX != newX
                        || mPreviousViewY != newY
                        || mPreviousViewWidth != newWidth
                        || mPreviousViewHeight != newHeight) {
                    mPreviousViewX = newX;
                    mPreviousViewY = newY;
                    mPreviousViewWidth = newWidth;
                    mPreviousViewHeight = newHeight;
                    mLastChangeMs = nowMs;
                }

                long timeSinceMoveMs = nowMs - mLastChangeMs;
                if (timeSinceMoveMs < mOptions.mSettleTimeMs) {
                    fulfilled = false;
                    messages.add("Not settled for " + mOptions.mSettleTimeMs + "ms");
                } else {
                    messages.add("Settled for " + mOptions.mSettleTimeMs + "ms");
                }
            }

            ViewT typedView = null;
            try {
                typedView = mViewClass.cast(mViewMatched);
            } catch (ClassCastException e) {
                fulfilled = false;
                messages.add(
                        String.format(
                                "Matched View was a %s which is not a %s",
                                mViewMatched.getClass().getName(), mViewClass.getName()));
            }

            String message = String.join("; ", messages);
            if (fulfilled) {
                assumeNonNull(typedView);
                return fulfilled(message).withResult(typedView);
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
            boolean mExpectDisabled;
            int mDisplayedPercentageRequired = ViewElement.MIN_DISPLAYED_PERCENT;
            int mSettleTimeMs;

            private Options() {}

            public class Builder {
                public Options build() {
                    return Options.this;
                }

                /** Whether the View is expected to be enabled. */
                public Builder withExpectEnabled(boolean state) {
                    mExpectEnabled = state;
                    return this;
                }

                /** Whether the View is expected to be disabled. */
                public Builder withExpectDisabled(boolean state) {
                    mExpectDisabled = state;
                    return this;
                }

                /** Minimum percentage of the View that needs to be displayed. */
                public Builder withDisplayingAtLeast(int displayedPercentageRequired) {
                    mDisplayedPercentageRequired = displayedPercentageRequired;
                    return this;
                }

                /** How long the View's rect needs to be unchanged. */
                public Builder withSettleTimeMs(int settleTimeMs) {
                    mSettleTimeMs = settleTimeMs;
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
