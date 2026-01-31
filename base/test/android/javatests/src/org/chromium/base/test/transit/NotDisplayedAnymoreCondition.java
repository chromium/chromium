// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import androidx.test.espresso.Root;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Fulfilled when no matching Views exist and are displayed. */
public class NotDisplayedAnymoreCondition extends UiThreadCondition {
    private final Supplier<RootSpec> mRootSpecSupplier;
    private final Matcher<View> mMatcher;

    public NotDisplayedAnymoreCondition(
            Supplier<RootSpec> rootSpecSupplier, Matcher<View> matcher) {
        super();
        mRootSpecSupplier = rootSpecSupplier;
        mMatcher = matcher;
    }

    @Override
    public String buildDescription() {
        return "No more displayed view: " + StringDescription.toString(mMatcher);
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        RootSpec rootSpec = mRootSpecSupplier.get();
        String reasonToWaitToMatch = rootSpec.getReasonToWaitToMatch();
        if (reasonToWaitToMatch != null) {
            return awaiting(reasonToWaitToMatch);
        }
        String reasonWillNotMatch = rootSpec.getReasonWillNotMatch();
        if (reasonWillNotMatch != null) {
            return fulfilled(reasonWillNotMatch);
        }

        List<Root> rootsToSearch = InternalViewFinder.findRoots(rootSpec);

        List<ViewAndRoot> allMatches = InternalViewFinder.findViews(rootsToSearch, mMatcher);
        List<ViewConditions.DisplayedEvaluation> allEvaluations = new ArrayList<>();
        List<ViewConditions.DisplayedEvaluation> displayedEvaluations = new ArrayList<>();
        for (ViewAndRoot viewAndRoot : allMatches) {
            ViewConditions.DisplayedEvaluation displayedEvaluation =
                    ViewConditions.evaluateMatch(
                            viewAndRoot, /* displayedPercentageRequired= */ 1, View.VISIBLE);
            allEvaluations.add(displayedEvaluation);
            if (displayedEvaluation.didMatch) {
                displayedEvaluations.add(displayedEvaluation);
            }
        }

        if (displayedEvaluations.isEmpty()) {
            return fulfilled(ViewConditions.writeDisplayedViewsStatusMessage(allEvaluations));
        } else {
            return notFulfilled(
                    ViewConditions.writeDisplayedViewsStatusMessage(displayedEvaluations));
        }
    }
}
