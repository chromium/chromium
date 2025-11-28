// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.View;

import androidx.test.espresso.Root;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.ViewPrinter;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** {@link Condition}s related to Android {@link View}s. */
@NullMarked
public class ViewConditions {

    private static final String TAG = "Transit";
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
        private final Supplier<@Nullable ActivityElement<?>> mActivityElementSupplier;
        private final Options mOptions;
        private int mPreviousViewX = Integer.MIN_VALUE;
        private int mPreviousViewY = Integer.MIN_VALUE;
        private int mPreviousViewWidth = Integer.MIN_VALUE;
        private int mPreviousViewHeight = Integer.MIN_VALUE;
        private long mLastChangeMs = -1;
        private @Nullable Root mMatchedRoot;

        public DisplayedCondition(
                Matcher<View> matcher,
                Class<ViewT> viewClass,
                Supplier<@Nullable ActivityElement<?>> activityElementSupplier,
                Options options) {
            super(/* isRunOnUiThread= */ true);
            mMatcher = matcher;
            mViewClass = viewClass;
            // Do not dependOnSupplier(activityElementSupplier) because a null supplied Activity is
            // valid.
            mActivityElementSupplier = activityElementSupplier;
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
            if (mOptions.mInDialogRoot) {
                description.append(", in dialog");
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

            ActivityElement<?> activityElement = mActivityElementSupplier.get();
            Activity activity;
            if (activityElement == null) {
                // TODO(crbug.com/456768907): Allow this only for dialogs.
                activity = null;
            } else {
                activity = activityElement.get();
                if (activity == null) {
                    return awaiting("Waiting for Activity from %s", activityElement)
                            .withoutResult();
                }
                if (activity.isDestroyed()) {
                    return notFulfilled("Activity from %s is destroyed", activityElement)
                            .withoutResult();
                }
                if (activity.isFinishing()) {
                    return notFulfilled("Activity from %s is finishing", activityElement)
                            .withoutResult();
                }
            }

            List<Root> roots = ViewFinder.findRoots(activity);

            List<ViewAndRoot> viewMatches = ViewFinder.findViews(roots, mMatcher);

            if (viewMatches.size() != 1) {
                return notFulfilled(writeMatchingViewsStatusMessage(viewMatches)).withoutResult();
            }
            ViewAndRoot matchedViewAndRoot = viewMatches.get(0);

            View matchedView = matchedViewAndRoot.view;

            boolean fulfilled = true;
            messages.add(ViewPrinter.describeView(matchedView, PRINT_SHALLOW_WITH_BOUNDS));

            int visibility = matchedView.getVisibility();
            if (visibility != View.VISIBLE) {
                fulfilled = false;
                messages.add(String.format("visibility = %s", visibilityIntToString(visibility)));
            } else {
                View view = matchedView;
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

            if (mOptions.mDisplayedPercentageRequired > 0) {
                DisplayedPortion portion = DisplayedPortion.ofView(matchedView);
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
                if (!matchedView.isEnabled()) {
                    fulfilled = false;
                    messages.add("disabled");
                }
            } else if (mOptions.mExpectDisabled) {
                if (matchedView.isEnabled()) {
                    fulfilled = false;
                    messages.add("enabled");
                }
            }

            if (mOptions.mSettleTimeMs > 0) {
                long nowMs = System.currentTimeMillis();
                int[] locationOnScreen = new int[2];
                matchedView.getLocationOnScreen(locationOnScreen);
                int newX = locationOnScreen[0];
                int newY = locationOnScreen[1];
                int newWidth = matchedView.getWidth();
                int newHeight = matchedView.getHeight();
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
                typedView = mViewClass.cast(matchedView);
            } catch (ClassCastException e) {
                fulfilled = false;
                messages.add(
                        String.format(
                                "Matched View was a %s which is not a %s",
                                matchedView.getClass().getName(), mViewClass.getName()));
            }

            String message = String.join("; ", messages);
            if (fulfilled) {
                assumeNonNull(typedView);
                mMatchedRoot = matchedViewAndRoot.root;
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

        @Nullable Root getRootMatched() {
            return mMatchedRoot;
        }

        /** Extra options for declaring DisplayedCondition. */
        public static class Options {
            boolean mExpectEnabled = true;
            boolean mExpectDisabled;
            boolean mInDialogRoot;
            int mDisplayedPercentageRequired = ViewElement.MIN_DISPLAYED_PERCENT;
            int mSettleTimeMs;

            private Options() {}

            public class Builder {
                public Options build() {
                    return Options.this;
                }

                /** Whether the View is expected to be a descendant of a dialog root. */
                public Builder withInDialogRoot(boolean inDialogRoot) {
                    mInDialogRoot = inDialogRoot;
                    return this;
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
    public static class NotDisplayedAnymoreCondition extends UiThreadCondition {
        private final Matcher<View> mMatcher;
        private final @Nullable ViewElement<?> mViewElement;

        private static final String VERBOSE_DESCRIPTION =
                "(view has effective visibility <VISIBLE> and view.getGlobalVisibleRect() to return"
                        + " non-empty rectangle)";
        private static final String SUCCINCT_DESCRIPTION = "isDisplayed()";

        public NotDisplayedAnymoreCondition(
                @Nullable ViewElement<?> viewElement, Matcher<View> matcher) {
            super();
            mViewElement = viewElement;
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

            List<Root> rootsToSearch;
            if (mViewElement != null) {
                // If created by a ViewElement, search the root which it was matched.
                Root rootMatched = mViewElement.getDisplayedCondition().getRootMatched();
                assert rootMatched != null;
                if (!rootMatched.getDecorView().hasWindowFocus()) {
                    return fulfilled();
                }
                rootsToSearch = List.of(rootMatched);
            } else {
                // If not created by a ViewElement (i.e. created by declareNoView()), search
                // the Activity related to the state, or all if there is no specific Activity
                // to search.
                Activity activity;
                if (mOwnerState == null) {
                    // If it's a TransitionCondition, mOwnerState will be null and search all roots.
                    activity = null;
                } else {
                    ActivityElement<?> activityElement = mOwnerState.determineActivityElement();
                    if (activityElement == null) {
                        // TODO(crbug.com/456768907): Allow this only for dialogs.
                        activity = null;
                    } else {
                        activity = activityElement.get();
                    }
                }
                rootsToSearch = ViewFinder.findRoots(activity);
            }
            List<ViewAndRoot> viewMatches = ViewFinder.findViews(rootsToSearch, mMatcher);

            if (viewMatches.isEmpty()) {
                return fulfilled();
            } else {
                return notFulfilled(writeMatchingViewsStatusMessage(viewMatches));
            }
        }
    }

    public static String writeMatchingViewsStatusMessage(List<ViewAndRoot> viewMatches) {
        // TODO(crbug.com/456770151): Print which root matches are in.
        if (viewMatches.isEmpty()) {
            return "No matching Views";
        } else if (viewMatches.size() == 1) {
            String viewDescription =
                    ViewPrinter.describeView(viewMatches.get(0).view, PRINT_SHALLOW);
            return String.format("1 matching View: %s", viewDescription);
        } else {
            String viewDescription1 =
                    ViewPrinter.describeView(viewMatches.get(0).view, PRINT_SHALLOW);
            String viewDescription2 =
                    ViewPrinter.describeView(viewMatches.get(1).view, PRINT_SHALLOW);
            String moreString = viewMatches.size() > 2 ? " and more" : "";

            return String.format(
                    "%d matching Views: %s, %s%s",
                    viewMatches.size(), viewDescription1, viewDescription2, moreString);
        }
    }
}
