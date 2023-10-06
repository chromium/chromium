// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.ViewConditions.DisplayedCondition;
import org.chromium.base.test.transit.ViewConditions.DoesNotExistAnymoreCondition;

import java.util.ArrayList;
import java.util.List;

/**
 * The elements that define a {@link ConditionalState}.
 *
 * <pre>
 * - An ACTIVE ConditionalState is considered to have these elements available.
 * - The presence of each element is an enter condition for the ConditionalState.
 * - The absence of each element is an exit condition for the ConditionalState (except for unowned
 *     elements).
 * </pre>
 */
public class Elements {
    private final List<Condition> mEnterConditions;
    private final List<Condition> mExitConditions;

    /** Private constructor, instantiated by {@link Builder#build(ConditionalState)}. */
    private Elements(List<Condition> enterConditions, List<Condition> exitConditions) {
        mEnterConditions = enterConditions;
        mExitConditions = exitConditions;
    }

    List<Condition> getEnterConditions() {
        return mEnterConditions;
    }

    List<Condition> getExitConditions() {
        return mExitConditions;
    }

    /**
     * Builder for {@link Elements}.
     *
     * <p>Passed to {@link ConditionalState#declareElements(Builder)}, which must declare the
     * ConditionalState's elements by calling the declare___() methods.
     */
    public static class Builder {
        private ArrayList<ViewElement> mViewElements = new ArrayList<>();
        private ArrayList<Condition> mOtherEnterConditions = new ArrayList<>();
        private ArrayList<Condition> mOtherExitConditions = new ArrayList<>();

        Builder() {}

        /**
         * Declare as an element a single view that matches |viewMatcher| which will be gone after
         * the ConditionalState is FINISHED.
         */
        public Builder declareView(Matcher<View> viewMatcher) {
            declareView(viewMatcher, /* owned= */ true);
            return this;
        }

        /**
         * Declare as an element a single view that matches |viewMatcher| which will not necessarily
         * be gone after the ConditionalState is FINISHED.
         */
        public Builder declareUnownedView(Matcher<View> viewMatcher) {
            declareView(viewMatcher, /* owned= */ false);
            return this;
        }

        private Builder declareView(Matcher<View> viewMatcher, boolean owned) {
            mViewElements.add(new ViewElement(viewMatcher, owned));
            return this;
        }

        /**
         * Declare as an element a generic enter Condition. It must remain true as long as the
         * ConditionalState is ACTIVE.
         */
        public Builder declareEnterCondition(Condition condition) {
            mOtherEnterConditions.add(condition);
            return this;
        }

        /** Declare as an element a generic exit Condition. */
        public Builder declareExitCondition(Condition condition) {
            mOtherExitConditions.add(condition);
            return this;
        }

        /**
         * Instantiates the {@link Elements} of a given |conditionalState| after they were declared
         * by calling the Builder's declare___() methods.
         */
        Elements build(ConditionalState conditionalState) {
            ArrayList<Condition> enterConditions = new ArrayList<>();
            ArrayList<Condition> exitConditions = new ArrayList<>();

            for (ViewElement viewElement : mViewElements) {
                DisplayedCondition displayedCondition =
                        new DisplayedCondition(viewElement.mViewMatcher);
                enterConditions.add(displayedCondition);
                if (viewElement.mOwned) {
                    exitConditions.add(
                            new DoesNotExistAnymoreCondition(
                                    viewElement.mViewMatcher, displayedCondition));
                }
            }

            enterConditions.addAll(mOtherEnterConditions);
            exitConditions.addAll(mOtherExitConditions);

            return new Elements(enterConditions, exitConditions);
        }
    }

    private static class ViewElement {
        private final Matcher<View> mViewMatcher;
        private final boolean mOwned;

        public ViewElement(Matcher<View> viewMatcher, boolean owned) {
            mViewMatcher = viewMatcher;
            mOwned = owned;
        }
    }
}
