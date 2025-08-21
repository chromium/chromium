// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

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

    @Override
    public String getName() {
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
                mActivityElement.requireToBeInSameTask(originActivityElement.value());
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
        return mActivityElement.value();
    }
}
