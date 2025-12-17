// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.view.View;

import androidx.test.espresso.ViewAction;

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
public class OptionalViewElement<ViewT extends View> extends Element<ViewT> {
    private final ViewSpec<ViewT> mViewSpec;
    private final ViewElement.Options mOptions;
    private final List<ViewCarryOn<ViewT>> mCarryOns = new ArrayList<>();

    OptionalViewElement(ViewSpec<ViewT> viewSpec, ViewElement.Options options) {
        super("OVE/" + viewSpec.getMatcherDescription());
        mViewSpec = viewSpec;
        mOptions = ViewElement.newOptions().initFrom(options).unscoped().build();
    }

    @Override
    ConditionWithResult<ViewT> getEnterConditionChecked() {
        // Return the last fulfilled condition.
        for (int i = mCarryOns.size() - 1; i >= 0; i--) {
            ViewCarryOn<ViewT> carryOn = mCarryOns.get(i);
            if (carryOn.getPhase() == ViewCarryOn.Phase.ACTIVE) {
                return carryOn.viewElement.getEnterConditionChecked();
            }
        }
        throw new AssertionError("Need to call checkPresent() first");
    }

    private ViewCarryOn<ViewT> createViewCarryOn() {
        ActivityElement<?> activityElement = mOwner.determineActivityElement();
        RootSpec rootSpec =
                activityElement == null
                        ? RootSpec.anyRoot()
                        : RootSpec.activityOrDialogRoot(activityElement);
        ViewCarryOn<ViewT> carryOn =
                new ViewCarryOn<>(
                        mViewSpec,
                        ViewElement.newOptions().initFrom(mOptions).rootSpec(rootSpec).build());
        mCarryOns.add(carryOn);
        return carryOn;
    }

    /** Ensures the ViewElement has been checked, possibly using a transition. */
    private ViewCarryOn<ViewT> waitForView() {
        return Triggers.noopTo().pickUpCarryOn(createViewCarryOn());
    }

    public ViewT checkPresent() {
        return waitForView().getView();
    }

    public void checkAbsent() {
        mOwner.noopTo().waitFor(absent());
    }

    public TripBuilder performViewActionTo(ViewAction action) {
        return waitForView().viewElement.performViewActionTo(action).withContext(mOwner);
    }

    public TripBuilder clickTo() {
        return waitForView().viewElement.clickTo().withContext(mOwner);
    }

    public TripBuilder longPressTo() {
        return waitForView().viewElement.longPressTo().withContext(mOwner);
    }

    public TripBuilder clickEvenIfPartiallyOccludedTo() {
        return waitForView().viewElement.clickEvenIfPartiallyOccludedTo().withContext(mOwner);
    }

    public TripBuilder typeTextTo(String text) {
        return waitForView().viewElement.typeTextTo(text).withContext(mOwner);
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
                /* viewElement= */ null, mViewSpec.getViewMatcher());
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
