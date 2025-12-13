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

    // Until setHostStation() this is null, but this field is accessed very often and asserting
    // doesn't add much value.
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

    void setHostStation(Station station) {
        assert mHostStation == null
                : "Facility " + this + " already added to a station. Tried to add it to " + station;
        mHostStation = (HostStationT) station;
    }

    /** Get the host {@link Station} this facility is scoped to. */
    public HostStationT getHostStation() {
        return mHostStation;
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
    @Nullable ActivityElement<?> determineActivityElement() {
        return mHostStation.determineActivityElement();
    }

    @Override
    <T extends Activity> void onDeclaredActivityElement(ActivityElement<T> element) {
        throw new UnsupportedOperationException(
                "Facilities cannot declare ActivityElements, Views are searched in the host"
                        + " Station's Activity");
    }
}
