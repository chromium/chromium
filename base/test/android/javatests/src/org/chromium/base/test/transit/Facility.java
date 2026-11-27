// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Facility is a {@link ConditionalState} scoped to a single host {@link Station} instance.
 *
 * <p>Its parent is the host {@link Station} by default. It can instead be another, already entered,
 * Facility of the same host {@link Station}, set with {@link #setParent}, in which case exiting the
 * parent Facility also exits this Facility.
 *
 * <p>This should be used for example for popup dialogs, menus, temporary messages. A transit-layer
 * class should be derived from it and instantiated. It should expose facility-specific methods for
 * the test-layer to use.
 *
 * <p>As a {@link ConditionalState}, it has a defined lifecycle and must declare {@link Element}s
 * that determine its enter and exit {@link Condition}s.
 *
 * <p>Leaving the host {@link Station} causes this state to be left as well, and exit {@link
 * Condition}s will be waited upon for the {@link Trip} to be complete.
 *
 * <p>Transitions into and out of a Facility while the host {@link Station} is ACTIVE should be done
 * with:
 *
 * <ul>
 *   <li>{@link TripBuilder#enterFacility(Facility)}
 *   <li>{@link TripBuilder#exitFacility(Facility)}}
 * </ul>
 *
 * @param <HostStationT> the type of host {@link Station} this is scoped to.
 */
@NullMarked
public class Facility<HostStationT extends Station<?>> extends ConditionalState {
    private static int sLastFacilityId = 1000;
    private final int mId = ++sLastFacilityId;

    // Until setParent() this is null, but this field is accessed very often and asserting doesn't
    // add much value.
    @SuppressWarnings("NullAway")
    protected HostStationT mHostStation;

    protected final @Nullable String mCustomName;

    /**
     * Constructor for named subclasses.
     *
     * <p>Named subclasses should let name default to the simple class name, e.g. "<S4|F1002:
     * SubclassNameFacility>".
     */
    protected Facility() {
        super();
        mCustomName = null;
    }

    /**
     * Create an empty Facility. Elements can be declared after creation.
     *
     * @param name Direct instantiations should provide a name which will be displayed as
     *     "<S4|F1002: ProvidedName>",
     */
    public Facility(String name) {
        super();
        mCustomName = name;
    }

    /**
     * Set the parent of this Facility, either its host {@link Station} or another Facility of the
     * host {@link Station}.
     */
    @SuppressWarnings("unchecked")
    @Override
    public void setParent(ConditionalState parent) {
        Station<?> hostStation;
        if (parent instanceof Station<?> station) {
            hostStation = station;
        } else if (parent instanceof Facility<?> parentFacility) {
            parentFacility.assertInPhase(Phase.ACTIVE);
            hostStation = parentFacility.getHostStation();
            assert hostStation != null
                    : String.format("Parent Facility %s of %s has no host Station", parent, this);
        } else {
            throw new AssertionError(
                    String.format(
                            "The parent of Facility %s must be a Station or a Facility, not %s",
                            this, parent));
        }
        super.setParent(parent);
        mHostStation = (HostStationT) hostStation;
    }

    /** Get the host {@link Station} this facility is scoped to. */
    public HostStationT getHostStation() {
        return mHostStation;
    }

    /**
     * Outcome: this Facility is replaced by |next| in a single Transition.
     *
     * <p>When the Trigger comes from this Facility, e.g. one of its Elements, {@link
     * TripBuilder#replaceFacility(Facility)} is shorter. Use this Outcome when another Facility
     * offers the Trigger, more state changes are needed, or a method offers the replacement for
     * callers to reach, e.g. {@code trip.reach(dialog.replacedBy(sheet).and(softKeyboard))}.
     */
    public <F extends Facility<? super HostStationT>> Outcome<F> replacedBy(F next) {
        return Outcome.enter(next).andExit(this);
    }

    @Override
    public String getName() {
        return String.format(
                "<S%s|F%s: %s>",
                mHostStation == null ? "-unset" : mHostStation.getId(),
                mId,
                mCustomName != null ? mCustomName : getClass().getSimpleName());
    }

    @Override
    <T extends Activity> void onDeclaredActivityElement(ActivityElement<T> element) {
        throw new UnsupportedOperationException(
                "Facilities cannot declare ActivityElements, Views are searched in the host"
                        + " Station's Activity");
    }
}
