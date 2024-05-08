// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;
import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.ViewConditions.NotDisplayedAnymoreCondition;

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

    private ArrayList<ElementInState> mElementsInState = new ArrayList<>();
    private ArrayList<Condition> mOtherEnterConditions = new ArrayList<>();
    private ArrayList<Condition> mOtherExitConditions = new ArrayList<>();

    /** Private constructor, instantiated by {@link Builder#build()}. */
    private Elements() {}

    public static Builder newBuilder() {
        return new Builder(new Elements());
    }

    List<ElementInState> getElementsInState() {
        return mElementsInState;
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

        /** Declare as an element an Android Activity of type |activityClass|. */
        public <T extends Activity> ActivityElement<T> declareActivity(Class<T> activityClass) {
            ActivityElement<T> element = new ActivityElement<>(activityClass);
            mElements.mElementsInState.add(element);
            return element;
        }

        /** Declare as an element a View that matches |viewMatcher|. */
        public ViewElementInState declareView(ViewElement viewElement) {
            ViewElementInState inState = new ViewElementInState(viewElement, /* gate= */ null);
            mElements.mElementsInState.add(inState);
            return inState;
        }

        /**
         * Conditional version of {@link #declareView(ViewElement)}.
         *
         * <p>The element is only expected if |gate| returns true.
         */
        public ViewElementInState declareViewIf(ViewElement viewElement, Condition gate) {
            ViewElementInState inState = new ViewElementInState(viewElement, gate);
            mElements.mElementsInState.add(inState);
            return inState;
        }

        /** Declare as a Condition that a View is not displayed. */
        public void declareNoView(Matcher<View> viewMatcher) {
            mElements.mOtherEnterConditions.add(new NotDisplayedAnymoreCondition(viewMatcher));
        }

        /**
         * Declare as an element a logical check that must return true when and as long as the
         * Station is ACTIVE.
         *
         * <p>Differs from {@link #declareEnterCondition(Condition)} in that shared-scope
         * LogicalElements do not generate exit Conditions when going to another ConditionalState
         * with the same LogicalElement.
         */
        public LogicalElement declareLogicalElement(LogicalElement logicalElement) {
            mElements.mElementsInState.add(logicalElement);
            return logicalElement;
        }

        /**
         * Declare as an element a generic enter Condition. It must be true for a transition into
         * this ConditionalState to be complete.
         *
         * <p>No promises are made that the Condition is true as long as the ConditionalState is
         * ACTIVE. For these cases, use {@link LogicalElement}.
         *
         * <p>Further, no promises are made that the Condition is false after exiting the State. Use
         * a scoped {@link LogicalElement} in this case.
         */
        public <T extends Condition> T declareEnterCondition(T condition) {
            mElements.mOtherEnterConditions.add(condition);
            return condition;
        }

        /**
         * Declare as an element a generic exit Condition. It must be true for a transition out of
         * this ConditionalState to be complete.
         *
         * <p>No promises are made that the Condition is false as long as the ConditionalState is
         * ACTIVE. For these cases, use a scoped {@link LogicalElement}.
         */
        public <T extends Condition> T declareExitCondition(T condition) {
            mElements.mOtherExitConditions.add(condition);
            return condition;
        }

        /** Declare a custom element, already rendered to an ElementInState. */
        public <T extends ElementInState> T declareElementInState(T elementInState) {
            mElements.mElementsInState.add(elementInState);
            return elementInState;
        }

        void addAll(Elements otherElements) {
            mElements.mElementsInState.addAll(otherElements.mElementsInState);
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
}
