// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.chromium.base.test.util.ViewPrinter.Options.PRINT_SHALLOW;
import static org.chromium.base.test.util.ViewPrinter.Options.PRINT_SHALLOW_WITH_BOUNDS;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.View;

import androidx.test.espresso.Root;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.ViewPrinter;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * Fulfilled when a single matching View exists and is displayed.
 *
 * @param <ViewT> the type of the View.
 */
public class DisplayedCondition<ViewT extends View> extends ConditionWithResult<ViewT> {
    private final Matcher<View> mMatcher;
    private final Class<ViewT> mViewClass;
    private final Supplier<RootSpec> mRootSpecSupplier;
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
            Supplier<RootSpec> rootSpecSupplier,
            Options options) {
        super(/* isRunOnUiThread= */ true);
        mMatcher = matcher;
        mViewClass = viewClass;
        mRootSpecSupplier = rootSpecSupplier;
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
        // TODO(crbug.com/456770151): Add to description which RootSpec was used.
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

        RootSpec rootSpec = mRootSpecSupplier.get();
        Supplier<? extends Activity> activitySupplier = rootSpec.getActivitySupplier();

        if (activitySupplier != null) {
            Activity activity = activitySupplier.get();
            if (activity == null) {
                return awaiting("Waiting for Activity from %s", activitySupplier).withoutResult();
            }
            if (activity.isDestroyed()) {
                return notFulfilled("Activity from %s is destroyed", activitySupplier)
                        .withoutResult();
            }
            if (activity.isFinishing()) {
                return notFulfilled("Activity from %s is finishing", activitySupplier)
                        .withoutResult();
            }
        }

        List<Root> roots = InternalViewFinder.findRoots(rootSpec);

        List<ViewAndRoot> viewMatches = InternalViewFinder.findViews(roots, mMatcher);

        if (viewMatches.size() != 1) {
            return notFulfilled(ViewConditions.writeMatchingViewsStatusMessage(viewMatches))
                    .withoutResult();
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
        int mDisplayedPercentageRequired = ViewElement.MIN_DISPLAYED_PERCENT;
        int mSettleTimeMs;

        private Options() {}

        public class Builder {
            public Options build() {
                return Options.this;
            }

            /** Whether the View is expected to be enabled. */
            public Options.Builder withExpectEnabled(boolean state) {
                mExpectEnabled = state;
                return this;
            }

            /** Whether the View is expected to be disabled. */
            public Options.Builder withExpectDisabled(boolean state) {
                mExpectDisabled = state;
                return this;
            }

            /** Minimum percentage of the View that needs to be displayed. */
            public Options.Builder withDisplayingAtLeast(int displayedPercentageRequired) {
                mDisplayedPercentageRequired = displayedPercentageRequired;
                return this;
            }

            /** How long the View's rect needs to be unchanged. */
            public Options.Builder withSettleTimeMs(int settleTimeMs) {
                mSettleTimeMs = settleTimeMs;
                return this;
            }
        }
    }
}
