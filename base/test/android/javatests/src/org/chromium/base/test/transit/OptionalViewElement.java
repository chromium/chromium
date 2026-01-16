// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewAssertion;
import androidx.test.espresso.ViewInteraction;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/**
 * Represents a lazily-checked {@link ViewElement}.
 *
 * <p>ENTER and EXIT Conditions are modified as to not block entering the owner {@link
 * ConditionalState} on this optional element. Instead, the element is lazily checked when
 * necessary:
 *
 * <ul>
 *   <li>When it is interacted with (e.g. clickTo(), performViewActionTo())
 *   <li>When its presence is checked (checkPresent()).
 *   <li>When its absence is checked (checkAbsent()).
 * </ul>
 *
 * @param <ViewT> the type of the View.
 */
@NullMarked
public class OptionalViewElement<ViewT extends View> extends Element<ViewT>
        implements ViewInterface {
    private final ViewSpec<ViewT> mViewSpec;
    private final ViewElement.Options mOptions;
    private final List<ViewPresence<ViewT>> mStates = new ArrayList<>();

    OptionalViewElement(ViewSpec<ViewT> viewSpec, ViewElement.Options options) {
        super("OVE/" + viewSpec.getMatcherDescription());
        mViewSpec = viewSpec;
        mOptions = ViewElement.newOptions().initFrom(options).unscoped().build();
    }

    @Override
    ConditionWithResult<ViewT> getEnterConditionChecked() {
        // Return the last fulfilled condition.
        for (int i = mStates.size() - 1; i >= 0; i--) {
            ViewPresence<ViewT> state = mStates.get(i);
            if (state.getPhase() == ViewPresence.Phase.ACTIVE) {
                return state.viewElement.getEnterConditionChecked();
            }
        }
        throw new AssertionError("Need to call checkPresent() first");
    }

    private ViewPresence<ViewT> createViewPresence() {
        ActivityElement<?> activityElement = mOwner.determineActivityElement();
        RootSpec rootSpec =
                activityElement == null
                        ? RootSpec.anyRoot()
                        : RootSpec.activityOrDialogRoot(activityElement);
        ViewPresence<ViewT> viewPresence =
                new ViewPresence<>(
                        mViewSpec,
                        ViewElement.newOptions().initFrom(mOptions).rootSpec(rootSpec).build());
        mStates.add(viewPresence);
        return viewPresence;
    }

    /** Ensures the ViewElement has been checked, possibly using a transition. */
    private ViewPresence<ViewT> waitForView() {
        return Triggers.noopTo().enterState(createViewPresence());
    }

    public ViewT checkPresent() {
        return waitForView().getView();
    }

    public void checkAbsent() {
        mOwner.noopTo().waitFor(absent());
    }

    @Override
    public void check(ViewAssertion assertion) {
        waitForView().check(assertion);
    }

    @Override
    public TripBuilder performViewActionTo(ViewAction action) {
        return waitForView().performViewActionTo(action).withContext(mOwner);
    }

    @Override
    public TripBuilder clickTo() {
        return waitForView().clickTo().withContext(mOwner);
    }

    @Override
    public TripBuilder longPressTo() {
        return waitForView().longPressTo().withContext(mOwner);
    }

    @Override
    public TripBuilder typeTextTo(String text) {
        return waitForView().typeTextTo(text).withContext(mOwner);
    }

    @Override
    public ViewInteraction onView() {
        return waitForView().onView();
    }

    /** Create a Condition fulfilled when this OptionalViewElement is present. */
    public ConditionWithResult<ViewT> present() {
        // Delay calculating the root spec because the owner state might not be set yet.
        Supplier<RootSpec> rootSpecSupplier = () -> ViewElement.calculateRootSpec(mOptions, mOwner);
        DisplayedCondition.Options conditionOptions =
                ViewElement.calculateDisplayedConditionOptions(mOptions).build();
        return new DisplayedCondition<>(
                mViewSpec.getViewMatcher(),
                mViewSpec.getViewClass(),
                rootSpecSupplier,
                conditionOptions);
    }

    /** Create a Condition fulfilled when this OptionalViewElement is absent. */
    public Condition absent() {
        return new NotDisplayedAnymoreCondition(
                () -> ViewElement.calculateRootSpec(ViewElement.Options.DEFAULT, mOwner),
                mViewSpec.getViewMatcher());
    }

    @Override
    public @Nullable ConditionWithResult<ViewT> createEnterCondition() {
        // TODO(crbug.com/464314426): Add a info-only DisplayedCondition.
        return null;
    }

    @Override
    public @Nullable Condition createExitCondition() {
        return null;
    }
}
