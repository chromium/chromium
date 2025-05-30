// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Set;

/**
 * Represents an Element added to a {@link ConditionalState} that supplies a {@param <ProductT>}.
 *
 * @param <ProductT> the type of object supplied when this Element is present.
 */
@NullMarked
public abstract class Element<ProductT extends @Nullable Object> implements Supplier<ProductT> {
    private final String mId;
    protected ConditionalState mOwner;
    private @Nullable ConditionWithResult<ProductT> mEnterCondition;
    private @Nullable Condition mExitCondition;

    /**
     * @param id A String ID used to match elements in origin and destination states. This avoids
     *     waiting for an element to disappear if it is in the destination state.
     */
    public Element(String id) {
        mId = id;
    }

    @Initializer
    void bind(ConditionalState owner) {
        assert mOwner == null
                : String.format("Element already bound to %s, cannot bind to %s", mOwner, owner);
        mOwner = owner;

        mEnterCondition = createEnterCondition();
        if (mEnterCondition != null) {
            mEnterCondition.bindToState(owner);
        }

        mExitCondition = createExitCondition();
        if (mExitCondition != null) {
            mExitCondition.bindToState(owner);
        }
    }

    /** Must create an ENTER Condition to ensure the element is present in the ConditionalState. */
    public abstract @Nullable ConditionWithResult<ProductT> createEnterCondition();

    /**
     * May create an EXIT Condition to ensure the element is not present after leaving the
     * ConditionalState.
     */
    public abstract @Nullable Condition createExitCondition();

    /** Replace the enter Condition. */
    protected void replaceEnterCondition(ConditionWithResult<ProductT> newEnterCondition) {
        assert mOwner != null : "Must be called after bind()";

        mEnterCondition = newEnterCondition;
        mEnterCondition.bindToState(mOwner);
    }

    // Supplier implementation
    /**
     * @return the product of the element (View, Activity, etc.)
     * @throws AssertionError if the element is in a ConditionalState that's neither ACTIVE nor
     *     TRANSITIONING_FROM, or if the element has no value.
     */
    @Override
    public ProductT get() {
        return getEnterConditionChecked().get();
    }

    // Supplier implementation
    @Override
    public boolean hasValue() {
        return getEnterConditionChecked().hasValue();
    }

    /**
     * Same as get() but callable after the ConditionalState is transitioned from. Use with caution,
     * as most of the time this means the product is not usable anymore.
     *
     * @return the product of the element (View, Activity, etc.) from a non-NEW ConditionalState
     */
    public ProductT getFromPast() {
        return getEnterConditionChecked().getFromPast();
    }

    /**
     * @return an ENTER Condition to ensure the element is present in the ConditionalState.
     */
    @Nullable ConditionWithResult<ProductT> getEnterCondition() {
        return mEnterCondition;
    }

    ConditionWithResult<ProductT> getEnterConditionChecked() {
        ConditionWithResult<ProductT> enterCondition = getEnterCondition();
        assert enterCondition != null
                : String.format("Element %s is missing an enter condition", this);
        return enterCondition;
    }

    /**
     * @param destinationElementIds ids of the elements in the destination for matching
     * @return an EXIT Condition to ensure the element is not present after transitioning to the
     *     destination.
     */
    @Nullable Condition getExitConditionFiltered(Set<String> destinationElementIds) {
        if (mExitCondition == null) {
            return null;
        }

        // Elements don't generate exit Conditions when the same element is in a
        // destination state.
        if (destinationElementIds.contains(getId())) {
            return null;
        }

        return mExitCondition;
    }

    /**
     * @return an id used to identify during a {@link Transition} if the same element is a part of
     *     both the origin and destination {@link ConditionalState}s.
     */
    public String getId() {
        return mId;
    }

    @Override
    public String toString() {
        return getId();
    }
}
