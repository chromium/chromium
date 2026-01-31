// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.chromium.base.test.util.ViewPrinter.Options.PRINT_SHALLOW_WITH_BOUNDS;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.View;

import androidx.test.espresso.Root;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

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

        description
                .append(", effectively ")
                .append(
                        ViewConditions.visibilityIntToString(
                                mOptions.mExpectedEffectiveVisibility));

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
        return description.append(")").toString();
    }

    @Override
    protected ConditionStatusWithResult<ViewT> resolveWithSuppliers() {
        // Match even views that are not visible so that visibility checking can be done with
        // more details later in this method.
        ArrayList<String> messages = new ArrayList<>();

        RootSpec rootSpec = mRootSpecSupplier.get();

        String reasonToWait = rootSpec.getReasonToWaitToMatch();
        if (reasonToWait != null) {
            return awaiting(reasonToWait).withoutResult();
        }
        String reasonWillNotMatch = rootSpec.getReasonWillNotMatch();
        if (reasonWillNotMatch != null) {
            return notFulfilled(reasonWillNotMatch).withoutResult();
        }

        List<Root> roots = InternalViewFinder.findRoots(rootSpec);

        List<ViewAndRoot> allMatches = InternalViewFinder.findViews(roots, mMatcher);
        List<ViewConditions.DisplayedEvaluation> displayedEvaluations = new ArrayList<>();
        List<ViewConditions.DisplayedEvaluation> displayedMatches = new ArrayList<>();
        for (ViewAndRoot viewAndRoot : allMatches) {
            ViewConditions.DisplayedEvaluation displayedEvaluation =
                    ViewConditions.evaluateMatch(
                            viewAndRoot,
                            mOptions.mDisplayedPercentageRequired,
                            mOptions.mExpectedEffectiveVisibility);
            displayedEvaluations.add(displayedEvaluation);
            if (displayedEvaluation.didMatch) {
                displayedMatches.add(displayedEvaluation);
            }
        }

        if (displayedMatches.isEmpty()) {
            if (allMatches.isEmpty()) {
                return notFulfilled("No matching Views").withoutResult();
            } else {
                return notFulfilled(
                                "Matched only Views displayed < %d%%: %s",
                                mOptions.mDisplayedPercentageRequired,
                                ViewConditions.writeDisplayedViewsStatusMessage(
                                        displayedEvaluations))
                        .withoutResult();
            }
        } else if (displayedMatches.size() > 1) {
            return notFulfilled(ViewConditions.writeDisplayedViewsStatusMessage(displayedMatches))
                    .withoutResult();
        }

        assert displayedMatches.size() == 1;
        ViewConditions.DisplayedEvaluation displayedMatch = displayedMatches.get(0);

        View matchedView = displayedMatch.viewAndRoot.view;

        boolean fulfilled = true;
        messages.add(ViewPrinter.describeView(matchedView, PRINT_SHALLOW_WITH_BOUNDS));
        messages.addAll(displayedMatch.messages);

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
            mMatchedRoot = displayedMatch.viewAndRoot.root;
            return fulfilled(message).withResult(typedView);
        } else {
            return notFulfilled(message).withoutResult();
        }
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
        int mExpectedEffectiveVisibility = View.VISIBLE;
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

            /** Whether the View is expected to be effectively VISIBLE, INVISIBLE or GONE. */
            public Options.Builder withEffectiveVisibility(int expectedVisibility) {
                mExpectedEffectiveVisibility = expectedVisibility;
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
