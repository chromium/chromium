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

    private ArrayList<Element<?>> mElements = new ArrayList<>();
    private Map<Condition, ElementFactory> mElementFactories = new HashMap<>();
    private ArrayList<Condition> mOtherEnterConditions = new ArrayList<>();
    private ArrayList<Condition> mOtherExitConditions = new ArrayList<>();

    Elements() {}

    Builder newBuilder() {
        return new Builder(this);
    }

    Set<String> getElementIds() {
        Set<String> elementIds = new HashSet<>();
        for (Element<?> element : mElements) {
            elementIds.add(element.getId());
        }
        return elementIds;
    }

    List<Element<?>> getElements() {
        return mElements;
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
        mElements.addAll(otherElements.mElements);
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
        private ArrayList<Element<?>> mElements = new ArrayList<>();
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
            mElements.add(element);
            return element;
        }

        /** Declare as an element a View that matches |viewMatcher|. */
        public ViewElement declareView(ViewSpec viewSpec) {
            return declareView(viewSpec, ViewElement.Options.DEFAULT);
        }

        /** Declare as an element a View that matches |viewMatcher| with extra Options. */
        public ViewElement declareView(ViewSpec viewSpec, ViewElement.Options options) {
            assertNotBuilt();
            ViewElement element = new ViewElement(viewSpec, options);
            mElements.add(element);
            return element;
        }

        /**
         * Declare an {@link ElementFactory} gated by a {@link Condition}.
         *
         * <p>When the Condition becomes fulfilled, |delayedDeclarations| will be run to declare new
         * Elements.
         */
        public void declareElementFactory(
                Condition condition, Callback<Elements.Builder> delayedDeclarations) {
            assertNotBuilt();
            mElementFactories.put(condition, new ElementFactory(mOwner, delayedDeclarations));
        }

        /**
         * Declare an {@link ElementFactory} gated by an {@link Element}'s enter Condition.
         *
         * <p>When the {@link Element}'s enter Condition becomes fulfilled, |delayedDeclarations|
         * will be run to declare new Elements.
         */
        public void declareElementFactory(
                Element<?> element, Callback<Elements.Builder> delayedDeclarations) {
            declareElementFactory(element.getEnterCondition(), delayedDeclarations);
        }

        /** Declare as a Condition that a View is not displayed. */
        public void declareNoView(ViewSpec viewSpec) {
            declareNoView(viewSpec.getViewMatcher());
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
            mElements.add(logicalElement);
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

        /** Declare a custom Element. */
        public <T extends Element<?>> T declareElement(T element) {
            assertNotBuilt();
            mElements.add(element);
            return element;
        }

        /**
         * Adds newly declared {@link Elements} (from calling the Builders declare___() methods) to
         * the original {@link Elements} owned by a ConditionalState.
         */
        Elements consolidate() {
            assertNotBuilt();
            Elements newElements = new Elements();
            newElements.mElements.addAll(mElements);
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
