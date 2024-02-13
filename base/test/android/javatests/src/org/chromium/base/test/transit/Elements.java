// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import androidx.annotation.Nullable;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.ViewConditions.DisplayedCondition;
import org.chromium.base.test.transit.ViewConditions.GatedDisplayedCondition;
import org.chromium.base.test.transit.ViewConditions.MatchedViewProvider;
import org.chromium.base.test.transit.ViewConditions.NotDisplayedAnymoreCondition;
import org.chromium.base.test.transit.ViewElement.Scope;

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

    static final Elements EMPTY = new Elements();

    private ArrayList<ViewElementInState> mViewElements = new ArrayList<>();
    private ArrayList<Condition> mOtherEnterConditions = new ArrayList<>();
    private ArrayList<Condition> mOtherExitConditions = new ArrayList<>();

    /** Private constructor, instantiated by {@link Builder#build()}. */
    private Elements() {}

    public static Builder newBuilder() {
        return new Builder(new Elements());
    }

    List<ViewElementInState> getViewElements() {
        return mViewElements;
    }

    List<Condition> getOtherEnterConditions() {
        return mOtherEnterConditions;
    }

    List<Condition> getOtherExitConditions() {
        return mOtherExitConditions;
    }

    /**
     * Builder for {@link Elements}.
     *
     * <p>Passed to {@link ConditionalState#declareElements(Builder)}, which must declare the
     * ConditionalState's elements by calling the declare___() methods.
     */
    public static class Builder {

        private Elements mElements;

        /** Instantiate by calling {@link Elements#newBuilder()}. */
        private Builder(Elements elements) {
            mElements = elements;
        }

        /** Declare as an element a View that matches |viewMatcher|. */
        public Builder declareView(ViewElement viewElement) {
            mElements.mViewElements.add(new ViewElementInState(viewElement, /* gate= */ null));
            return this;
        }

        /**
         * Conditional version of {@link #declareView(ViewElement)}.
         *
         * <p>The element is only expected if |gate| returns true.
         */
        public Builder declareViewIf(ViewElement viewElement, Condition gate) {
            mElements.mViewElements.add(new ViewElementInState(viewElement, gate));
            return this;
        }

        /**
         * Declare as an element a generic enter Condition. It must remain true as long as the
         * ConditionalState is ACTIVE.
         */
        public Builder declareEnterCondition(Condition condition) {
            mElements.mOtherEnterConditions.add(condition);
            return this;
        }

        /** Declare as an element a generic exit Condition. */
        public Builder declareExitCondition(Condition condition) {
            mElements.mOtherExitConditions.add(condition);
            return this;
        }

        void addAll(Elements otherElements) {
            mElements.mViewElements.addAll(otherElements.mViewElements);
            mElements.mOtherEnterConditions.addAll(otherElements.mOtherEnterConditions);
            mElements.mOtherExitConditions.addAll(otherElements.mOtherExitConditions);
        }

        /**
         * Instantiates the {@link Elements} of a given |conditionalState| after they were declared
         * by calling the Builder's declare___() methods.
         */
        Elements build() {
            Elements elements = mElements;
            mElements = null;
            return elements;
        }
    }

    /**
     * Represents a ViewElement added to a ConditionState.
     *
     * <p>ViewElements should be declared as constants, while ViewElementInStates are created by
     * calling {@link Elements.Builder#declareView(ViewElement)} or {@link
     * Elements.Builder#declareViewIf(ViewElement, Condition)}.
     */
    static class ViewElementInState {
        private final ViewElement mViewElement;
        private final @Nullable Condition mGate;

        private final Condition mEnterCondition;
        private final @Nullable Condition mExitCondition;

        ViewElementInState(ViewElement viewElement, @Nullable Condition gate) {
            mViewElement = viewElement;
            mGate = gate;

            Matcher<View> viewMatcher = mViewElement.getViewMatcher();
            MatchedViewProvider matchedViewProvider;
            if (mGate != null) {
                GatedDisplayedCondition gatedDisplayedCondition =
                        new GatedDisplayedCondition(mViewElement.getViewMatcher(), mGate);
                mEnterCondition = gatedDisplayedCondition;
                matchedViewProvider = gatedDisplayedCondition;
            } else {
                DisplayedCondition displayedCondition = new DisplayedCondition(viewMatcher);
                mEnterCondition = displayedCondition;
                matchedViewProvider = displayedCondition;
            }

            switch (mViewElement.getScope()) {
                case Scope.CONDITIONAL_STATE_SCOPED:
                case Scope.SHARED:
                    mExitCondition =
                            new NotDisplayedAnymoreCondition(viewMatcher, matchedViewProvider);
                    break;
                case Scope.UNSCOPED:
                    mExitCondition = null;
                    break;
                default:
                    mExitCondition = null;
                    assert false;
            }
        }

        ViewElement getViewElement() {
            return mViewElement;
        }

        Condition getEnterCondition() {
            return mEnterCondition;
        }

        @Nullable
        Condition getExitCondition() {
            return mExitCondition;
        }
    }
}
