// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;
import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.base.Callback;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

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
@NullMarked
public class Elements extends BaseElements {
    private final ConditionalState mOwnerState;

    Elements(ConditionalState ownerState) {
        mOwnerState = ownerState;
    }

    Builder newBuilder() {
        return new Builder(this);
    }

    /**
     * Builder for {@link Elements}.
     *
     * <p>Passed to {@link ConditionalState#declareElements(Elements.Builder)}}, which must declare
     * the ConditionalState's elements by calling the declare___() methods.
     */
    public static class Builder {
        private @Nullable Elements mOwner;
        private final ArrayList<Element<?>> mElements = new ArrayList<>();
        private final Map<Condition, ElementFactory> mElementFactories = new HashMap<>();
        private final ArrayList<Condition> mOtherEnterConditions = new ArrayList<>();
        private final ArrayList<Condition> mOtherExitConditions = new ArrayList<>();

        /** Instantiate by calling {@link Elements#newBuilder()}. */
        private Builder(Elements owner) {
            mOwner = owner;
        }

        /** Declare as an element an Android Activity of type |activityClass|. */
        public <T extends Activity> ActivityElement<T> declareActivity(Class<T> activityClass) {
            ActivityElement<T> element = new ActivityElement<>(activityClass);
            return declareElement(element);
        }

        /** Declare as an element a View that matches |viewMatcher|. */
        public <ViewT extends View> ViewElement<ViewT> declareView(ViewSpec<ViewT> viewSpec) {
            return declareView(viewSpec, ViewElement.Options.DEFAULT);
        }

        /** Declare as an element a View that matches |viewMatcher| with extra Options. */
        public <ViewT extends View> ViewElement<ViewT> declareView(
                ViewSpec<ViewT> viewSpec, ViewElement.Options options) {
            ViewElement<ViewT> element = new ViewElement<>(viewSpec, options);
            return declareElement(element);
        }

        /**
         * Declare an {@link ElementFactory} gated by a {@link Condition}.
         *
         * <p>When the Condition becomes fulfilled, |delayedDeclarations| will be run to declare new
         * Elements.
         */
        public void declareElementFactory(
                Condition condition, Callback<Builder> delayedDeclarations) {
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
            declareEnterCondition(new ViewConditions.NotDisplayedAnymoreCondition(viewMatcher));
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
        public <T extends Condition> void declareEnterCondition(T condition) {
            assertNotBuilt();
            condition.bindToState(mOwner.mOwnerState);
            mOtherEnterConditions.add(condition);
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
        public <ProductT, T extends ConditionWithResult<ProductT>>
                Element<ProductT> declareEnterConditionAsElement(T condition) {
            assertNotBuilt();
            Element<ProductT> element =
                    new Element<>("CE/" + condition.getDescription()) {
                        @Override
                        public ConditionWithResult<ProductT> createEnterCondition() {
                            return condition;
                        }

                        @Override
                        public @Nullable Condition createExitCondition() {
                            return null;
                        }
                    };
            declareElement(element);
            return element;
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
            condition.bindToState(mOwner.mOwnerState);
            mOtherExitConditions.add(condition);
            return condition;
        }

        /** Declare a custom Element. */
        public <T extends Element<?>> T declareElement(T element) {
            assertNotBuilt();
            element.bind(mOwner.mOwnerState);
            mElements.add(element);
            return element;
        }

        /**
         * Adds newly declared {@link Elements} (from calling the Builders declare___() methods) to
         * the original {@link Elements} owned by a ConditionalState.
         */
        Elements consolidate() {
            assertNotBuilt();
            Elements newElements = new Elements(mOwner.mOwnerState);
            newElements.mElements.addAll(mElements);
            newElements.mElementFactories.putAll(mElementFactories);
            newElements.mOtherEnterConditions.addAll(mOtherEnterConditions);
            newElements.mOtherExitConditions.addAll(mOtherExitConditions);
            mOwner.addAll(newElements);
            mOwner = null;
            return newElements;
        }

        @EnsuresNonNull("mOwner")
        private void assertNotBuilt() {
            assert mOwner != null
                    : "Elements.Builder already built; if in declareElementFactory(), probably"
                            + " using the outer Elements.Builder instead of the nested one";
        }
    }
}
