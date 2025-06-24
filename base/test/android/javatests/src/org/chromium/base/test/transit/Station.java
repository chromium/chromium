// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import org.chromium.base.test.transit.Transition.TransitionOptions;
import org.chromium.base.test.transit.Transition.Trigger;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/**
 * A major {@link ConditionalState}, a "screen" the app can be in. Only one can be active at a time.
 *
 * <p>A transit-layer class should be derived from it and instantiated.
 *
 * <p>As a {@link ConditionalState}, it has a defined lifecycle and must declare {@link Element}s
 * that determine its enter and exit {@link Condition}s.
 *
 * <p>Transitions should be done between Stations. The transit-layer derived class should expose
 * screen-specific methods for the test-layer to use.
 *
 * @param <HostActivity> The activity this station is associate to.
 */
@NullMarked
public abstract class Station<HostActivity extends Activity> extends ConditionalState {
    private static final String TAG = "Transit";
    private static int sLastStationId;

    private final int mId;
    // All facilities that have ever been entered. Exited ones remain so that the history is can
    // be queried.
    private final List<Facility<?>> mFacilities = new ArrayList<>();
    private final String mName;
    private final @Nullable Class<HostActivity> mActivityClass;

    protected final @Nullable ActivityElement<HostActivity> mActivityElement;

    /**
     * Create a base station.
     *
     * @param activityClass the subclass of Activity this Station expects as an element. Expect no
     *     Activity if null.
     */
    protected Station(@Nullable Class<HostActivity> activityClass) {
        mActivityClass = activityClass;
        mId = sLastStationId++;
        mName = String.format("<S%d: %s>", mId, getClass().getSimpleName());
        TrafficControl.notifyCreatedStation(this);

        if (mActivityClass != null) {
            mActivityElement = mElements.declareActivity(mActivityClass);
        } else {
            mActivityElement = null;
        }
    }

    protected List<Facility<?>> getFacilitiesWithPhase(@Phase int phase) {
        List<Facility<?>> facilities = new ArrayList<>();
        for (Facility<?> facility : mFacilities) {
            if (facility.getPhase() == phase) {
                facilities.add(facility);
            }
        }
        return facilities;
    }

    void registerFacility(Facility<?> facility) {
        facility.setHostStation(this);
        mFacilities.add(facility);
    }

    private void assertHasFacilities(List<Facility<?>> facilities) {
        for (var facility : facilities) {
            assert mFacilities.contains(facility) : this + " does not have " + facility;
        }
    }

    @Override
    public String getName() {
        return mName;
    }

    @Override
    public String toString() {
        return mName;
    }

    /**
     * @return the self-incrementing id for logging purposes.
     */
    public int getId() {
        return mId;
    }

    void requireToBeInSameTask(Station<?> originStation) {
        assertInPhase(Phase.NEW);
        if (mActivityElement != null) {
            originStation.assertInPhase(Phase.ACTIVE);
            ActivityElement<?> originActivityElement = originStation.getActivityElement();
            if (originActivityElement != null) {
                mActivityElement.requireToBeInSameTask(originActivityElement.get());
            } else {
                mActivityElement.requireNoParticularTask();
            }
        }
    }

    void requireToBeInNewTask() {
        assertInPhase(Phase.NEW);
        if (mActivityElement != null) {
            mActivityElement.requireToBeInNewTask();
        }
    }

    /**
     * @deprecated Use {@link TripBuilder#enterFacility(Facility)} instead.
     */
    @Deprecated
    public <F extends Facility<?>> F enterFacilitySync(F facility, @Nullable Trigger trigger) {
        return enterFacilitySync(facility, TransitionOptions.DEFAULT, trigger);
    }

    /**
     * @deprecated Use {@link TripBuilder#enterFacility(Facility)} instead.
     */
    @Deprecated
    public <F extends Facility<?>> F enterFacilitySync(
            F facility, TransitionOptions options, @Nullable Trigger trigger) {
        registerFacility(facility);
        FacilityCheckIn checkIn = new FacilityCheckIn(List.of(facility), options, trigger);
        checkIn.transitionSync();
        return facility;
    }

    /**
     * @deprecated Use {@link TripBuilder#enterFacilities(Facility[])} instead.
     */
    @Deprecated
    public void enterFacilitiesSync(List<Facility<?>> facilities, @Nullable Trigger trigger) {
        enterFacilitiesSync(facilities, TransitionOptions.DEFAULT, trigger);
    }

    /**
     * @deprecated Use {@link TripBuilder#enterFacilities(Facility[])} instead.
     */
    @Deprecated
    public void enterFacilitiesSync(
            List<Facility<?>> facilities, TransitionOptions options, @Nullable Trigger trigger) {
        for (Facility<?> f : facilities) {
            registerFacility(f);
        }
        FacilityCheckIn checkIn = new FacilityCheckIn(facilities, options, trigger);
        checkIn.transitionSync();
    }

    /**
     * @deprecated Use {@link TripBuilder#exitFacility(Facility)} instead.
     */
    @Deprecated
    public void exitFacilitySync(Facility<?> facility, @Nullable Trigger trigger) {
        exitFacilitySync(facility, TransitionOptions.DEFAULT, trigger);
    }

    /**
     * @deprecated Use {@link TripBuilder#exitFacility(Facility)} instead.
     */
    @Deprecated
    public void exitFacilitySync(
            Facility<?> facility, TransitionOptions options, @Nullable Trigger trigger) {
        exitFacilitiesSync(List.of(facility), options, trigger);
    }

    /**
     * @deprecated Use {@link TripBuilder#exitFacilities(Facility[])} instead.
     */
    @Deprecated
    public void exitFacilitiesSync(List<Facility<?>> facilities, @Nullable Trigger trigger) {
        exitFacilitiesSync(facilities, TransitionOptions.DEFAULT, trigger);
    }

    /**
     * @deprecated Use {@link TripBuilder#exitFacilities(Facility[])} instead.
     */
    @Deprecated
    public void exitFacilitiesSync(
            List<Facility<?>> facilities, TransitionOptions options, @Nullable Trigger trigger) {
        assertHasFacilities(facilities);
        FacilityCheckOut checkOut = new FacilityCheckOut(facilities, options, trigger);
        checkOut.transitionSync();
    }

    /**
     * @deprecated Use {@code tripBuilder.exitFacilityAnd(f).enterFacility(g)| instead.
     */
    @Deprecated
    public <F extends Facility<?>> F swapFacilitySync(
            Facility<?> facilityToExit, F facilityToEnter, @Nullable Trigger trigger) {
        return swapFacilitySync(
                List.of(facilityToExit), facilityToEnter, TransitionOptions.DEFAULT, trigger);
    }

    /**
     * @deprecated Use {@code tripBuilder.exitFacilityAnd(f).enterFacility(g)| instead.
     */
    @Deprecated
    public <F extends Facility<?>> F swapFacilitySync(
            Facility<?> facilityToExit,
            F facilityToEnter,
            TransitionOptions options,
            @Nullable Trigger trigger) {
        return swapFacilitySync(List.of(facilityToExit), facilityToEnter, options, trigger);
    }

    /**
     * @deprecated Use {@code tripBuilder.exitFacilitiesAnd(f, g).enterFacility(h)| instead.
     */
    @Deprecated
    public <F extends Facility<?>> F swapFacilitySync(
            List<Facility<?>> facilitiesToExit, F facilityToEnter, @Nullable Trigger trigger) {
        return swapFacilitySync(
                facilitiesToExit, facilityToEnter, TransitionOptions.DEFAULT, trigger);
    }

    /**
     * @deprecated Use {@code tripBuilder.exitFacilitiesAnd(f, g).enterFacility(h)| instead.
     */
    @Deprecated
    public <F extends Facility<?>> F swapFacilitySync(
            List<Facility<?>> facilitiesToExit,
            F facilityToEnter,
            TransitionOptions options,
            @Nullable Trigger trigger) {
        assertHasFacilities(facilitiesToExit);
        registerFacility(facilityToEnter);
        FacilitySwap swap =
                new FacilitySwap(facilitiesToExit, List.of(facilityToEnter), options, trigger);
        swap.transitionSync();
        return facilityToEnter;
    }

    /**
     * @deprecated Use {@code tripBuilder.exitFacilitiesAnd(f, g).enterFacilities(h, i)| instead.
     */
    @Deprecated
    public void swapFacilitiesSync(
            List<Facility<?>> facilitiesToExit,
            List<Facility<?>> facilitiesToEnter,
            @Nullable Trigger trigger) {
        swapFacilitiesSync(facilitiesToExit, facilitiesToEnter, TransitionOptions.DEFAULT, trigger);
    }

    /**
     * Version of {@link #swapFacilitiesSync(List, List, Trigger)} with extra TransitionOptions.
     *
     * @deprecated use {@link TripBuilder} instead.
     */
    @Deprecated
    public void swapFacilitiesSync(
            List<Facility<?>> facilitiesToExit,
            List<Facility<?>> facilitiesToEnter,
            TransitionOptions options,
            @Nullable Trigger trigger) {
        assertHasFacilities(facilitiesToExit);
        for (Facility<?> facility : facilitiesToEnter) {
            registerFacility(facility);
        }
        FacilitySwap swap = new FacilitySwap(facilitiesToExit, facilitiesToEnter, options, trigger);
        swap.transitionSync();
    }

    /** Get the activity element associate with this station, if there's any. */
    public @Nullable ActivityElement<HostActivity> getActivityElement() {
        return mActivityElement;
    }

    /**
     * Returns the Activity matched to the ActivityCondition.
     *
     * <p>The element is only guaranteed to exist as long as the station is ACTIVE or in transition
     * triggers when it is already TRANSITIONING_FROM.
     */
    public HostActivity getActivity() {
        assert mActivityElement != null
                : "Requesting an ActivityElement for a station with no host activity.";
        return mActivityElement.get();
    }
}
