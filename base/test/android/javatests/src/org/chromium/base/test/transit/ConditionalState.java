// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.fail;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Base class representing a state with conditions for entering and exiting.
 *
 * <p>Conditions include the existence of {@link Elements}, e.g. Views.
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
public abstract class ConditionalState {
    @Phase private int mLifecyclePhase = Phase.NEW;
    private Elements mElements;

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
     * Declare the {@link Elements} that define this ConditionalState, such as Views.
     *
     * <p>Transit-layer {@link Station}s and {@link Facility}s should override this and use the
     * |elements| param to declare what elements need to be waited for for the state to be
     * considered active.
     *
     * @param elements use the #declare___() methods to describe the Elements that define the state.
     */
    public abstract void declareElements(Elements.Builder elements);

    Elements getElements() {
        initElements();
        return mElements;
    }

    private void initElements() {
        if (mElements == null) {
            mElements = new Elements();
            Elements.Builder builder = mElements.newBuilder();
            declareElements(builder);
            builder.consolidate();
        }
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

    protected void assertSuppliersCanBeUsed() {
        int phase = getPhase();
        if (phase != Phase.ACTIVE && phase != Phase.TRANSITIONING_FROM) {
            fail(
                    String.format(
                            "%s should have been ACTIVE or TRANSITIONING_FROM, but was %s",
                            this, phaseToString(phase)));
        }
    }
}
