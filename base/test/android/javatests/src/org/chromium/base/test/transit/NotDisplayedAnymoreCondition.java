// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import androidx.test.espresso.Root;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import org.chromium.base.ApplicationStatus;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/** Fulfilled when no matching Views exist and are displayed. */
public class NotDisplayedAnymoreCondition extends UiThreadCondition {
    private final Matcher<View> mMatcher;
    private final @Nullable ViewElement<?> mViewElement;

    public NotDisplayedAnymoreCondition(
            @Nullable ViewElement<?> viewElement, Matcher<View> matcher) {
        super();
        mViewElement = viewElement;
        mMatcher = matcher;
    }

    @Override
    public String buildDescription() {
        return "No more displayed view: " + StringDescription.toString(mMatcher);
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
            rootsToSearch = List.of(rootMatched);
        } else {
            // If not created by a ViewElement (i.e. created by declareNoView()), search
            // the Activity related to the state, or all if there is no specific Activity
            // to search.
            RootSpec rootSpec;
            if (mOwnerState == null) {
                // If it's a TransitionCondition, mOwnerState will be null and search all roots.
                rootSpec = RootSpec.anyRoot();
            } else {
                ActivityElement<?> activityElement = mOwnerState.determineActivityElement();
                if (activityElement == null) {
                    rootSpec = RootSpec.anyRoot();
                } else {
                    rootSpec = RootSpec.activityOrDialogRoot(activityElement);
                }
            }
            rootsToSearch = InternalViewFinder.findRoots(rootSpec);
        }
        List<ViewAndRoot> allMatches = InternalViewFinder.findViews(rootsToSearch, mMatcher);
        List<ViewConditions.DisplayedEvaluation> allEvaluations = new ArrayList<>();
        List<ViewConditions.DisplayedEvaluation> displayedEvaluations = new ArrayList<>();
        for (ViewAndRoot viewAndRoot : allMatches) {
            ViewConditions.DisplayedEvaluation displayedEvaluation =
                    ViewConditions.evaluateMatch(viewAndRoot, /* displayedPercentageRequired= */ 1);
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
