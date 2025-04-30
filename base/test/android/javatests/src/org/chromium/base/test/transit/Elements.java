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

        /** See {@link ConditionalState#declareActivity(Class)}. */
        public <T extends Activity> ActivityElement<T> declareActivity(Class<T> activityClass) {
            ActivityElement<T> element = new ActivityElement<>(activityClass);
            return declareElement(element);
        }

        /** See {@link ConditionalState#declareView(ViewSpec)}. */
        public <ViewT extends View> ViewElement<ViewT> declareView(ViewSpec<ViewT> viewSpec) {
            return declareView(viewSpec, ViewElement.Options.DEFAULT);
        }

        /** See {@link ConditionalState#declareView(ViewSpec, ViewElement.Options)}. */
        public <ViewT extends View> ViewElement<ViewT> declareView(
                ViewSpec<ViewT> viewSpec, ViewElement.Options options) {
            ViewElement<ViewT> element = new ViewElement<>(viewSpec, options);
            return declareElement(element);
        }

        /**
         * See {@link ConditionalState#declareElementFactory(Condition, Callback)}.
         *
         * @deprecated Use {@link #declareElementFactory(Element, Callback)} instead.}
         */
        @Deprecated
        public void declareElementFactory(
                Condition condition, Callback<Builder> delayedDeclarations) {
            assertNotBuilt();
            mElementFactories.put(condition, new ElementFactory(mOwner, delayedDeclarations));
        }

        /** See {@link ConditionalState#declareElementFactory(Element, Callback)}. */
        public void declareElementFactory(
                Element<?> element, Callback<Elements.Builder> delayedDeclarations) {
            declareElementFactory(element.getEnterCondition(), delayedDeclarations);
        }

        /** See {@link ConditionalState#declareNoView(ViewSpec)}. */
        public void declareNoView(ViewSpec viewSpec) {
            declareNoView(viewSpec.getViewMatcher());
        }

        /** See {@link ConditionalState#declareNoView(Matcher)}. */
        public void declareNoView(Matcher<View> viewMatcher) {
            declareEnterCondition(new ViewConditions.NotDisplayedAnymoreCondition(viewMatcher));
        }

        /** See {@link ConditionalState#declareEnterCondition(Condition)}. */
        public <T extends Condition> void declareEnterCondition(T condition) {
            assertNotBuilt();
            condition.bindToState(mOwner.mOwnerState);
            mOtherEnterConditions.add(condition);
        }

        /** See {@link ConditionalState#declareEnterConditionAsElement(Condition)}. */
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

        /** See {@link ConditionalState#declareExitCondition(Condition)}. */
        public <T extends Condition> T declareExitCondition(T condition) {
            assertNotBuilt();
            condition.bindToState(mOwner.mOwnerState);
            mOtherExitConditions.add(condition);
            return condition;
        }

        /** See {@link ConditionalState#declareElement(Element)}. */
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
        BaseElements consolidate() {
            assertNotBuilt();
            BaseElements newElements = new BaseElements();
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
