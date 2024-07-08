// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;
import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.Callback;
import org.chromium.base.test.transit.ViewConditions.NotDisplayedAnymoreCondition;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

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

    private ArrayList<ElementInState<?>> mElementsInState = new ArrayList<>();
    private Map<Condition, ElementFactory> mElementFactories = new HashMap<>();
    private ArrayList<Condition> mOtherEnterConditions = new ArrayList<>();
    private ArrayList<Condition> mOtherExitConditions = new ArrayList<>();

    Elements() {}

    Builder newBuilder() {
        return new Builder(this);
    }

    Set<String> getElementsInStateIds() {
        Set<String> elementIds = new HashSet<>();
        for (ElementInState<?> element : mElementsInState) {
            elementIds.add(element.getId());
        }
        return elementIds;
    }

    List<ElementInState<?>> getElementsInState() {
        return mElementsInState;
    }

    Map<Condition, ElementFactory> getElementFactories() {
        return mElementFactories;
    }

    List<Condition> getOtherEnterConditions() {
        return mOtherEnterConditions;
    }

    List<Condition> getOtherExitConditions() {
        return mOtherExitConditions;
    }

    void addAll(Elements otherElements) {
        mElementsInState.addAll(otherElements.mElementsInState);
        mElementFactories.putAll(otherElements.mElementFactories);
        mOtherEnterConditions.addAll(otherElements.mOtherEnterConditions);
        mOtherExitConditions.addAll(otherElements.mOtherExitConditions);
    }

    /**
     * Builder for {@link Elements}.
     *
     * <p>Passed to {@link ConditionalState#declareElements(Builder)}, which must declare the
     * ConditionalState's elements by calling the declare___() methods.
     */
    public static class Builder {
        private Elements mOwner;
        private ArrayList<ElementInState<?>> mElementsInState = new ArrayList<>();
        private Map<Condition, ElementFactory> mElementFactories = new HashMap<>();
        private ArrayList<Condition> mOtherEnterConditions = new ArrayList<>();
        private ArrayList<Condition> mOtherExitConditions = new ArrayList<>();

        /** Instantiate by calling {@link Elements#newBuilder()}. */
        private Builder(Elements owner) {
            mOwner = owner;
        }

        /** Declare as an element an Android Activity of type |activityClass|. */
        public <T extends Activity> ActivityElement<T> declareActivity(Class<T> activityClass) {
            assertNotBuilt();
            ActivityElement<T> element = new ActivityElement<>(activityClass);
            mElementsInState.add(element);
            return element;
        }

        /** Declare as an element a View that matches |viewMatcher|. */
        public ViewElementInState declareView(ViewElement viewElement) {
            assertNotBuilt();
            ViewElementInState inState = new ViewElementInState(viewElement, /* gate= */ null);
            mElementsInState.add(inState);
            return inState;
        }

        /** Declare an {@link ElementFactory} that is gated by a Condition. */
        public <T extends Condition> T declareElementFactory(
                T condition, Callback<Elements.Builder> delayedDeclarations) {
            assertNotBuilt();
            mElementFactories.put(condition, new ElementFactory(mOwner, delayedDeclarations));
            return condition;
        }

        /**
         * Conditional version of {@link #declareView(ViewElement)}.
         *
         * <p>The element is only expected if |gate| returns true.
         */
        public ViewElementInState declareViewIf(ViewElement viewElement, Condition gate) {
            assertNotBuilt();
            ViewElementInState inState = new ViewElementInState(viewElement, gate);
            mElementsInState.add(inState);
            return inState;
        }

        /** Declare as a Condition that a View is not displayed. */
        public void declareNoView(Matcher<View> viewMatcher) {
            assertNotBuilt();
            mOtherEnterConditions.add(new NotDisplayedAnymoreCondition(viewMatcher));
        }

        /**
         * Declare as an element a logical check that must return true when and as long as the
         * Station is ACTIVE.
         *
         * <p>Differs from {@link #declareEnterCondition(Condition)} in that shared-scope
         * LogicalElements do not generate exit Conditions when going to another ConditionalState
         * with the same LogicalElement.
         */
        public LogicalElement<?> declareLogicalElement(LogicalElement<?> logicalElement) {
            assertNotBuilt();
            mElementsInState.add(logicalElement);
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
            assertNotBuilt();
            mOtherEnterConditions.add(condition);
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
            assertNotBuilt();
            mOtherExitConditions.add(condition);
            return condition;
        }

        /** Declare a custom element, already rendered to an ElementInState. */
        public <T extends ElementInState<?>> T declareElementInState(T elementInState) {
            assertNotBuilt();
            mElementsInState.add(elementInState);
            return elementInState;
        }

        /**
         * Adds newly declared {@link Elements} (from calling the Builders declare___() methods) to
         * the original {@link Elements} owned by a ConditionalState.
         */
        Elements consolidate() {
            assertNotBuilt();
            Elements newElements = new Elements();
            newElements.mElementsInState.addAll(mElementsInState);
            newElements.mElementFactories.putAll(mElementFactories);
            newElements.mOtherEnterConditions.addAll(mOtherEnterConditions);
            newElements.mOtherExitConditions.addAll(mOtherExitConditions);
            mOwner.addAll(newElements);
            mOwner = null;
            return newElements;
        }

        private void assertNotBuilt() {
            assert mOwner != null
                    : "Elements.Builder already built; if in declareElementFactory(), probably"
                            + " using the outer Elements.Builder instead of the nested one";
        }
    }
}
