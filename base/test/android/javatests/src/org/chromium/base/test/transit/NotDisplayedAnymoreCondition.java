// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;

import static org.hamcrest.CoreMatchers.allOf;

import android.view.View;

import androidx.test.espresso.Root;

import org.hamcrest.Matcher;
import org.hamcrest.StringDescription;

import org.chromium.base.ApplicationStatus;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/** Fulfilled when no matching Views exist and are displayed. */
public class NotDisplayedAnymoreCondition extends UiThreadCondition {
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
            RootSpec rootSpec;
            if (mOwnerState == null) {
                // If it's a TransitionCondition, mOwnerState will be null and search all roots.
                rootSpec = RootSpec.anyRoot();
            } else {
                ActivityElement<?> activityElement = mOwnerState.determineActivityElement();
                if (activityElement == null) {
                    // TODO(crbug.com/456768907): Allow this only for dialogs.
                    rootSpec = RootSpec.anyRoot();
                } else {
                    rootSpec = RootSpec.activityOrDialogRoot(activityElement);
                }
            }
            rootsToSearch = InternalViewFinder.findRoots(rootSpec);
        }
        List<ViewAndRoot> viewMatches = InternalViewFinder.findViews(rootsToSearch, mMatcher);

        if (viewMatches.isEmpty()) {
            return fulfilled();
        } else {
            return notFulfilled(ViewConditions.writeMatchingViewsStatusMessage(viewMatches));
        }
    }
}
