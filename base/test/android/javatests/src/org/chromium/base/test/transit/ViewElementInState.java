// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import androidx.annotation.Nullable;

import org.hamcrest.Matcher;

import org.chromium.base.test.transit.ViewConditions.DisplayedCondition;
import org.chromium.base.test.transit.ViewConditions.GatedDisplayedCondition;
import org.chromium.base.test.transit.ViewConditions.NotDisplayedAnymoreCondition;
import org.chromium.base.test.transit.ViewElement.Scope;

import java.util.Set;

/**
 * Represents a ViewElement added to a ConditionState.
 *
 * <p>ViewElements should be declared as constants, while ViewElementInStates are created by calling
 * {@link Elements.Builder#declareView(ViewElement)} or {@link
 * Elements.Builder#declareViewIf(ViewElement, Condition)}.
 *
 * <p>Generates ENTER and EXIT Conditions for the ConditionalState to ensure the ViewElement is in
 * the right state.
 */
class ViewElementInState implements ElementInState {
    private final ViewElement mViewElement;
    private final @Nullable Condition mGate;

    private final Condition mEnterCondition;
    private final @Nullable Condition mExitCondition;

    ViewElementInState(ViewElement viewElement, @Nullable Condition gate) {
        mViewElement = viewElement;
        mGate = gate;

        Matcher<View> viewMatcher = mViewElement.getViewMatcher();
        if (mGate != null) {
            GatedDisplayedCondition gatedDisplayedCondition =
                    new GatedDisplayedCondition(mViewElement.getViewMatcher(), mGate);
            mEnterCondition = gatedDisplayedCondition;
        } else {
            DisplayedCondition displayedCondition = new DisplayedCondition(viewMatcher);
            mEnterCondition = displayedCondition;
        }

        switch (mViewElement.getScope()) {
            case Scope.CONDITIONAL_STATE_SCOPED:
            case Scope.SHARED:
                mExitCondition = new NotDisplayedAnymoreCondition(viewMatcher);
                break;
            case Scope.UNSCOPED:
                mExitCondition = null;
                break;
            default:
                mExitCondition = null;
                assert false;
        }
    }

    @Override
    public String getId() {
        return mViewElement.getId();
    }

    @Override
    public Condition getEnterCondition() {
        return mEnterCondition;
    }

    @Override
    public @Nullable Condition getExitCondition(Set<String> destinationElementIds) {
        switch (mViewElement.getScope()) {
            case Scope.CONDITIONAL_STATE_SCOPED:
                return mExitCondition;
            case Scope.SHARED:
                return destinationElementIds.contains(getId()) ? null : mExitCondition;
            case Scope.UNSCOPED:
                return null;
            default:
                assert false;
                return null;
        }
    }
}
