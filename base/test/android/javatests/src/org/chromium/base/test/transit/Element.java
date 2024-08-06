// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;

import java.util.Set;

/**
 * Represents an Element added to a {@link ConditionalState} that supplies a {@param <ProductT>}.
 *
 * @param <ProductT> the type of object supplied when this Element is present.
 */
public abstract class Element<ProductT> implements Supplier<ProductT> {
    private final String mId;
    private boolean mEnterConditionCreated;
    private boolean mExitConditionCreated;
    private @Nullable ConditionWithResult<ProductT> mEnterCondition;
    private @Nullable Condition mExitCondition;

    /**
     * @param id A String ID used to match elements in origin and destination states. This avoids
     *     waiting for an element to disappear if it is in the destination state.
     */
    public Element(String id) {
        mId = id;
    }

    /** Must create an ENTER Condition to ensure the element is present in the ConditionalState. */
    public abstract ConditionWithResult<ProductT> createEnterCondition();

    /**
     * May create an EXIT Condition to ensure the element is not present after leaving the
     * ConditionalState.
     */
    public abstract @Nullable Condition createExitCondition();

    @Override
    public ProductT get() {
        return getEnterCondition().get();
    }

    @Override
    public boolean hasValue() {
        return getEnterCondition().hasValue();
    }

    /**
     * @return an ENTER Condition to ensure the element is present in the ConditionalState.
     */
    public ConditionWithResult<ProductT> getEnterCondition() {
        if (!mEnterConditionCreated) {
            mEnterCondition = createEnterCondition();
            mEnterConditionCreated = true;
        }
        return mEnterCondition;
    }

    /**
     * @param destinationElementIds ids of the elements in the destination for matching
     * @return an EXIT Condition to ensure the element is not present after transitioning to the
     *     destination.
     */
    public Condition getExitCondition(Set<String> destinationElementIds) {
        if (!mExitConditionCreated) {
            // Elements don't generate exit Conditions when the same element is in a
            // destination state.
            if (destinationElementIds.contains(getId())) {
                return null;
            }
            mExitCondition = createExitCondition();
            mExitConditionCreated = true;
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
