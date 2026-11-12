// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.assertEquals;

import android.app.Activity;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.transit.ConditionalState.Phase;
import org.chromium.build.annotations.Nullable;

/** Unit tests for {@link Station#declareDefaultFacility(Facility)}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DefaultFacilitiesUnitTest {
    @Rule public final PublicTransitUnitTestRule mTransitRule = new PublicTransitUnitTestRule();

    /** A Station without default Facilities. */
    private static class PlainStation extends Station<Activity> {
        PlainStation() {
            super(Activity.class);
        }
    }

    /** A Station with one default Facility. */
    private static class StationWithDefaultFacility extends Station<Activity> {
        public final TestFacility defaultFacility;

        StationWithDefaultFacility() {
            this(Activity.class);
        }

        StationWithDefaultFacility(@Nullable Class<Activity> activityClass) {
            super(activityClass);
            defaultFacility = declareDefaultFacility(new TestFacility());
        }
    }

    /** A Facility with a single Element fulfilled as soon as it is checked. */
    private static class TestFacility extends Facility<StationWithDefaultFacility> {
        public final Element<String> valueElement;

        TestFacility() {
            valueElement = declareEnterConditionAsElement(new AlwaysFulfilledCondition());
        }
    }

    private static class AlwaysFulfilledCondition extends ConditionWithResult<String> {
        private AlwaysFulfilledCondition() {
            super(/* isRunOnUiThread= */ false);
        }

        @Override
        protected ConditionStatusWithResult<String> resolveWithSuppliers() {
            return fulfilled("always fulfilled").withResult("value");
        }

        @Override
        public String buildDescription() {
            return "Always fulfilled Condition";
        }
    }

    private <T extends Station<?>> T hopOnTo(T station) throws Throwable {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        mTransitRule.runTransition(() -> TrafficControl.hopOnAt(activity, station));
        return station;
    }

    @Test
    public void testHopOn_entersDefaultFacility() throws Throwable {
        StationWithDefaultFacility station = hopOnTo(new StationWithDefaultFacility());

        assertEquals(Phase.ACTIVE, station.defaultFacility.getPhase());
        assertEquals(station, station.defaultFacility.getHostStation());
        assertEquals("value", station.defaultFacility.valueElement.value());
    }

    @Test
    public void testArriveAt_entersDefaultFacility() throws Throwable {
        PlainStation origin = hopOnTo(new PlainStation());

        StationWithDefaultFacility destination = new StationWithDefaultFacility();
        mTransitRule.runTransition(() -> origin.noopTo().arriveAt(destination));

        assertEquals(Phase.ACTIVE, destination.getPhase());
        assertEquals(Phase.ACTIVE, destination.defaultFacility.getPhase());
    }

    @Test
    public void testArriveAt_defaultFacilityAlsoPassedExplicitly_isEnteredOnce() throws Throwable {
        PlainStation origin = hopOnTo(new PlainStation());

        StationWithDefaultFacility destination = new StationWithDefaultFacility();
        mTransitRule.runTransition(
                () -> origin.noopTo().arriveAt(destination, destination.defaultFacility));

        assertEquals(Phase.ACTIVE, destination.defaultFacility.getPhase());
    }

    @Test
    public void testLeavingStation_exitsDefaultFacility() throws Throwable {
        StationWithDefaultFacility origin = hopOnTo(new StationWithDefaultFacility());

        PlainStation destination = new PlainStation();
        mTransitRule.runTransition(() -> origin.noopTo().arriveAt(destination));

        assertEquals(Phase.FINISHED, origin.defaultFacility.getPhase());
    }

    @Test
    public void testExitDefaultFacility_stationStaysActive() throws Throwable {
        StationWithDefaultFacility origin = hopOnTo(new StationWithDefaultFacility());

        mTransitRule.runTransition(() -> origin.noopTo().exitFacility(origin.defaultFacility));
        assertEquals(Phase.ACTIVE, origin.getPhase());
        assertEquals(Phase.FINISHED, origin.defaultFacility.getPhase());

        // Leaving the Station doesn't exit the default Facility a second time.
        PlainStation destination = new PlainStation();
        mTransitRule.runTransition(() -> origin.noopTo().arriveAt(destination));
        assertEquals(Phase.ACTIVE, destination.getPhase());
    }

    @Test
    public void testInNewTask_entersDefaultFacility() throws Throwable {
        PlainStation origin = hopOnTo(new PlainStation());

        // Without an Activity, so the destination doesn't need to be found in a new task.
        StationWithDefaultFacility destination =
                new StationWithDefaultFacility(/* activityClass= */ null);
        mTransitRule.runTransition(() -> origin.noopTo().inNewTask().arriveAt(destination));

        assertEquals(Phase.ACTIVE, destination.getPhase());
        assertEquals(Phase.ACTIVE, destination.defaultFacility.getPhase());
    }
}
