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
public class ViewElementInState implements ElementInState {
    private final ViewElement mViewElement;
    private final @Nullable Condition mGate;

    private final Condition mEnterCondition;
    private final @Nullable Condition mExitCondition;

    ViewElementInState(ViewElement viewElement, @Nullable Condition gate) {
        mViewElement = viewElement;
        mGate = gate;

        Matcher<View> viewMatcher = mViewElement.getViewMatcher();
        ViewElement.Options elementOptions = mViewElement.getOptions();
        DisplayedCondition.Options conditionOptions =
                DisplayedCondition.newOptions()
                        .withExpectEnabled(elementOptions.mExpectEnabled)
                        .withDisplayingAtLeast(elementOptions.mDisplayedPercentageRequired)
                        .build();
        if (mGate != null) {
            GatedDisplayedCondition gatedDisplayedCondition =
                    new GatedDisplayedCondition(
                            mViewElement.getViewMatcher(), mGate, conditionOptions);
            mEnterCondition = gatedDisplayedCondition;
        } else {
            DisplayedCondition displayedCondition =
                    new DisplayedCondition(viewMatcher, conditionOptions);
            mEnterCondition = displayedCondition;
        }

        switch (mViewElement.getScope()) {
            case Scope.SCOPED:
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
            case Scope.SCOPED:
                return destinationElementIds.contains(getId()) ? null : mExitCondition;
            case Scope.UNSCOPED:
                return null;
            default:
                assert false;
                return null;
        }
    }
}
