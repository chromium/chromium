// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.fail;

import static org.chromium.base.test.transit.ViewSpec.viewSpec;

import android.app.Activity;
import android.view.View;

import androidx.annotation.IntDef;

import org.hamcrest.Matcher;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Base class representing a state with conditions for entering and exiting.
 *
 * <p>Conditions include the existence of {@link Element}s, e.g. Views.
 *
 * <pre>ConditionalStates can be in the following phases:
 * - NEW: Inactive, just created. No transition has started.
 * - TRANSITIONING_TO: A transition into the state has started, but enter conditions might not be
 *     fulfilled yet.
 * - ACTIVE: Active, declared elements should exist.
 * - TRANSITIONING_FROM: A transition out of the state has started, but exit conditions are not
 *     fulfilled yet.
 * - FINISHED: Inactive, transition away is done.
 * </pre>
 *
 * <p>The lifecycle of ConditionalStates is linear:
 *
 * <p>NEW > TRANSITIONING_TO > ACTIVE > TRANSITIONING_FROM > FINISHED
 *
 * <p>Once FINISHED, the ConditionalState does not change state anymore.
 *
 * <p>This is the base class for {@link Station} and {@link Facility}.
 */
@NullMarked
public abstract class ConditionalState {
    @Phase private int mLifecyclePhase = Phase.NEW;
    private final Elements mConsolidatedElements = new Elements(this);
    protected final Elements.Builder mElements = mConsolidatedElements.newBuilder();
    private boolean mAreElementsConsolidated;

    /** Lifecycle phases of ConditionalState. */
    @IntDef({
        Phase.NEW,
        Phase.TRANSITIONING_TO,
        Phase.ACTIVE,
        Phase.TRANSITIONING_FROM,
        Phase.FINISHED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Phase {
        int NEW = 0;
        int TRANSITIONING_TO = 1;
        int ACTIVE = 2;
        int TRANSITIONING_FROM = 3;
        int FINISHED = 4;
    }

    /**
     * Declare extra {@link Element}s that define this ConditionalState, such as Views.
     *
     * <p>Transit-layer {@link Station}s and {@link Facility}s can declare Elements in their
     * constructor and/or override this method. This method is called after binding a Facility to a
     * Station, so some elements are easier to declare here.
     */
    public void declareExtraElements() {}

    Elements getElements() {
        if (!mAreElementsConsolidated) {
            declareExtraElements();
            mElements.consolidate();
            mAreElementsConsolidated = true;
        }
        return mConsolidatedElements;
    }

    void setStateTransitioningTo() {
        assertInPhase(Phase.NEW);
        mLifecyclePhase = Phase.TRANSITIONING_TO;
        onTransitionToStarted();
    }

    /** Hook to run code before a transition to the ConditionalState. */
    protected void onTransitionToStarted() {}

    void setStateActive() {
        assertInPhase(Phase.TRANSITIONING_TO);
        mLifecyclePhase = Phase.ACTIVE;
        onTransitionToFinished();
    }

    /** Hook to run code after a transition to the ConditionalState. */
    protected void onTransitionToFinished() {}

    void setStateTransitioningFrom() {
        assertInPhase(Phase.ACTIVE);
        mLifecyclePhase = Phase.TRANSITIONING_FROM;
        onTransitionFromStarted();
    }

    /** Hook to run code before a transition from the ConditionalState. */
    protected void onTransitionFromStarted() {}

    void setStateFinished() {
        assertInPhase(Phase.TRANSITIONING_FROM);
        mLifecyclePhase = Phase.FINISHED;
        onTransitionFromFinished();
    }

    /** Hook to run code after a transition from the ConditionalState. */
    protected void onTransitionFromFinished() {}

    /**
     * @return the name of the State for use in debugging/error messages.
     */
    public abstract String getName();

    /**
     * @return the lifecycle {@link Phase} this ConditionalState is in.
     */
    public @Phase int getPhase() {
        return mLifecyclePhase;
    }

    /** Assert this ConditionalState is in an expected lifecycle {@link Phase}. */
    public void assertInPhase(@Phase int expectedPhase) {
        if (mLifecyclePhase != expectedPhase) {
            fail(
                    String.format(
                            "%s should have been in %s, but was %s",
                            this, phaseToString(expectedPhase), phaseToString(mLifecyclePhase)));
        }
    }

    /** Check the declared Elements still exist. */
    public final void recheckActiveConditions() {
        assertInPhase(Phase.ACTIVE);

        List<Condition> enterConditions = new ArrayList<>();
        Elements elements = getElements();
        for (Element<?> element : elements.getElements()) {
            Condition enterCondition = element.getEnterCondition();
            if (enterCondition != null) {
                enterConditions.add(enterCondition);
            }
        }

        ConditionChecker.check(getName(), enterConditions);
    }

    /**
     * @return a String representation of a lifecycle {@link Phase}.
     */
    public static String phaseToString(@Phase int phase) {
        switch (phase) {
            case Phase.NEW:
                return "Phase.NEW";
            case Phase.TRANSITIONING_TO:
                return "Phase.TRANSITIONING_TO";
            case Phase.ACTIVE:
                return "Phase.ACTIVE";
            case Phase.TRANSITIONING_FROM:
                return "Phase.TRANSITIONING_AWAY";
            case Phase.FINISHED:
                return "Phase.FINISHED";
            default:
                throw new IllegalArgumentException("No string representation for phase " + phase);
        }
    }

    public static String phaseToShortString(@Phase int phase) {
        switch (phase) {
            case Phase.NEW:
                return "NEW";
            case Phase.TRANSITIONING_TO:
                return "TRANSITIONING_TO";
            case Phase.ACTIVE:
                return "ACTIVE";
            case Phase.TRANSITIONING_FROM:
                return "TRANSITIONING_AWAY";
            case Phase.FINISHED:
                return "FINISHED";
            default:
                throw new IllegalArgumentException(
                        "No short string representation for phase " + phase);
        }
    }

    /** Should be used only by {@link EntryPointSentinelStation}. */
    void setStateActiveWithoutTransition() {
        mLifecyclePhase = Phase.ACTIVE;
    }

    /**
     * Assert this ConditionalState is in the ACTIVE, TRANSITIONING_FROM or TRANSITIONING_TO phase,
     * and thus it makes sense to use its {@link Element}s as Suppliers.
     *
     * <p>This avoids getting an out-of-date object from a state that was already transitioned away
     * from, or getting an object from a state before transitioning to it.
     */
    void assertSuppliersMightBeValid() {
        int phase = getPhase();
        if (phase != Phase.ACTIVE
                && phase != Phase.TRANSITIONING_FROM
                && phase != Phase.TRANSITIONING_TO) {
            fail(
                    String.format(
                            "%s should have been ACTIVE or TRANSITIONING_FROM or TRANSITIONING_TO,"
                                    + " but was %s",
                            this, phaseToString(phase)));
        }
    }

    /** Declare as an element an Android Activity of type |activityClass|. */
    protected <T extends Activity> ActivityElement<T> declareActivity(Class<T> activityClass) {
        return mElements.declareActivity(activityClass);
    }

    /** Declare as an element a View that matches |viewMatcher|. */
    public <ViewT extends View> ViewElement<ViewT> declareView(ViewSpec<ViewT> viewSpec) {
        return mElements.declareView(viewSpec);
    }

    /** Declare as an element a View that matches |viewMatcher| with extra Options. */
    public ViewElement<View> declareView(Matcher<View> viewMatcher, ViewElement.Options options) {
        return mElements.declareView(viewMatcher, options);
    }

    /** Declare as an element a |viewClass| that matches |viewMatcher|. */
    public <ViewT extends View> ViewElement<ViewT> declareView(
            Class<ViewT> viewClass, Matcher<View> viewMatcher) {
        return mElements.declareView(viewClass, viewMatcher);
    }

    /** Declare as an element a |viewClass| that matches |viewMatcher| with extra Options. */
    public <ViewT extends View> ViewElement<ViewT> declareView(
            Class<ViewT> viewClass, Matcher<View> viewMatcher, ViewElement.Options options) {
        return mElements.declareView(viewClass, viewMatcher, options);
    }

    /** Declare as an element a View that matches |viewSpec| with extra Options. */
    public <ViewT extends View> ViewElement<ViewT> declareView(
            ViewSpec<ViewT> viewSpec, ViewElement.Options options) {
        return mElements.declareView(viewSpec, options);
    }

    /** Declare as an element a View that matches |viewSpec|. */
    public ViewElement<View> declareView(Matcher<View> viewMatcher) {
        return mElements.declareView(viewMatcher);
    }

    /** Declare as a Condition that a View is not displayed. */
    public void declareNoView(ViewSpec<?> viewSpec) {
        mElements.declareNoView(viewSpec);
    }

    /** Declare as a Condition that a View is not displayed. */
    public void declareNoView(Matcher<View> viewMatcher) {
        mElements.declareNoView(viewMatcher);
    }

    /**
     * Declare as an element a generic enter Condition. It must be true for a transition into this
     * ConditionalState to be complete.
     *
     * <p>No promises are made that the Condition is true as long as the ConditionalState is ACTIVE.
     * For these cases, use {@link LogicalElement}.
     *
     * <p>Further, no promises are made that the Condition is false after exiting the State. Use a
     * scoped {@link LogicalElement} in this case.
     */
    public final void declareEnterCondition(Condition condition) {
        mElements.declareEnterCondition(condition);
    }

    /**
     * Declare as an element a generic enter Condition. It must be true for a transition into this
     * ConditionalState to be complete.
     *
     * <p>No promises are made that the Condition is true as long as the ConditionalState is ACTIVE.
     * For these cases, use {@link LogicalElement}.
     *
     * <p>Further, no promises are made that the Condition is false after exiting the State. Use a
     * scoped {@link LogicalElement} in this case.
     */
    public <ProductT, T extends ConditionWithResult<ProductT>>
            Element<ProductT> declareEnterConditionAsElement(T condition) {
        return mElements.declareEnterConditionAsElement(condition);
    }

    /**
     * Declare as an element a generic exit Condition. It must be true for a transition out of this
     * ConditionalState to be complete.
     *
     * <p>No promises are made that the Condition is false as long as the ConditionalState is
     * ACTIVE. For these cases, use a scoped {@link LogicalElement}.
     */
    public final void declareExitCondition(Condition condition) {
        mElements.declareExitCondition(condition);
    }

    /**
     * Declare an {@link ElementFactory} gated by an {@link Element}'s enter Condition.
     *
     * <p>When the {@link Element}'s enter Condition becomes fulfilled, |delayedDeclarations| will
     * be run to declare new Elements.
     */
    public void declareElementFactory(
            Element<?> element, Callback<Elements.Builder> delayedDeclarations) {
        mElements.declareElementFactory(element, delayedDeclarations);
    }

    /** Declare a custom Element. */
    public <T extends Element<?>> T declareElement(T element) {
        return mElements.declareElement(element);
    }
}
