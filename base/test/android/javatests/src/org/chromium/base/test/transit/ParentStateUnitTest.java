// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.Condition.whether;

import android.app.Activity;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.transit.ConditionalState.Phase;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for parent and child {@link ConditionalState}s. */
@RunWith(BaseRobolectricTestRunner.class)
public class ParentStateUnitTest {
    @Rule public final PublicTransitUnitTestRule mTransitRule = new PublicTransitUnitTestRule();

    /** Whether the Views of the test Facilities are shown. */
    private final AtomicBoolean mShown = new AtomicBoolean(true);

    private static class TestStation extends Station<Activity> {
        TestStation() {
            super(Activity.class);
        }
    }

    /** A Facility whose exit Condition is fulfilled when |mShown| is false. */
    private class TestFacility extends Facility<TestStation> {
        TestFacility() {
            declareExitCondition(
                    InstrumentationThreadCondition.from(
                            "Facility hidden", () -> whether(!mShown.get())));
        }
    }

    private TestStation hopOn(Facility<?>... facilities) throws Throwable {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        TestStation station = new TestStation();
        mTransitRule.runTransition(() -> TrafficControl.hopOnAt(activity, station, facilities));
        return station;
    }

    @Test
    public void testEnterFacility_setsStationAsParent() throws Throwable {
        TestStation station = hopOn();
        TestFacility facility = new TestFacility();

        mTransitRule.runTransition(() -> station.noopTo().enterFacility(facility));

        assertSame(station, facility.getParent());
        assertSame(station, facility.getHostStation());
        assertEquals(List.of(facility), station.getActiveChildren());
    }

    @Test
    public void testArriveAtStation_exitsActiveChildren() throws Throwable {
        TestFacility facility = new TestFacility();
        TestStation station = hopOn(facility);

        // The Facility is still shown, so leaving the Station times out on its exit Condition.
        TestStation destination = new TestStation();
        TravelException e =
                mTransitRule.runTransitionExpectingThrow(
                        TravelException.class,
                        () -> station.noopTo().withTimeout(100).arriveAt(destination));
        assertTrue(e.getMessage(), e.getMessage().contains("Facility hidden"));
    }

    @Test
    public void testReachLastStop_exitsActiveChildren() throws Throwable {
        TestFacility facility = new TestFacility();
        TestStation station = hopOn(facility);

        mShown.set(false);
        mTransitRule.runTransition(() -> station.noopTo().reachLastStop());

        assertEquals(Phase.FINISHED, station.getPhase());
        assertEquals(Phase.FINISHED, facility.getPhase());
    }

    @Test
    public void testExitStation_doesNotExitFinishedChildrenAgain() throws Throwable {
        TestFacility facility = new TestFacility();
        TestStation station = hopOn(facility);
        mShown.set(false);
        mTransitRule.runTransition(() -> station.noopTo().exitFacility(facility));

        // The finished Facility is no longer a child, so it is not exited again with the Station.
        assertTrue(station.getActiveChildren().isEmpty());
        mTransitRule.runTransition(() -> station.noopTo().reachLastStop());

        assertEquals(Phase.FINISHED, station.getPhase());
    }

    @Test
    public void testHopOff_finishesChildren() throws Throwable {
        TestFacility facility = new TestFacility();
        TestStation station = hopOn(facility);

        TrafficControl.hopOffPublicTransit();

        assertEquals(Phase.FINISHED, station.getPhase());
        assertEquals(Phase.FINISHED, facility.getPhase());
    }

    @Test
    public void testSetParent_twice_throws() throws Throwable {
        TestStation station = hopOn();
        TestFacility facility = new TestFacility();
        facility.setParent(station);

        assertThrows(AssertionError.class, () -> facility.setParent(station));
    }

    @Test
    public void testSetParent_facilityOfFacility_throws() throws Throwable {
        TestFacility parent = new TestFacility();
        hopOn(parent);

        TestFacility child = new TestFacility();

        assertThrows(AssertionError.class, () -> child.setParent(parent));
    }

    @Test
    public void testArriveAtStation_finishesActiveChildren() throws Throwable {
        TestFacility facility = new TestFacility();
        TestStation station = hopOn(facility);

        mShown.set(false);
        TestStation destination = new TestStation();
        mTransitRule.runTransition(() -> station.noopTo().arriveAt(destination));

        assertEquals(Phase.FINISHED, station.getPhase());
        assertEquals(Phase.FINISHED, facility.getPhase());
        assertEquals(Phase.ACTIVE, destination.getPhase());
    }

    @Test
    public void testArriveAtStation_finishesGrandchildren() throws Throwable {
        TestFacility facility = new TestFacility();
        TestStation station = hopOn(facility);
        State grandchild = enterChildState(facility);

        mShown.set(false);
        TestStation destination = new TestStation();
        mTransitRule.runTransition(() -> station.noopTo().arriveAt(destination));

        assertEquals(Phase.FINISHED, facility.getPhase());
        assertEquals(Phase.FINISHED, grandchild.getPhase());
    }

    @Test
    public void testHopOff_finishesGrandchildren() throws Throwable {
        TestFacility facility = new TestFacility();
        hopOn(facility);
        State grandchild = enterChildState(facility);

        TrafficControl.hopOffPublicTransit();

        assertEquals(Phase.FINISHED, facility.getPhase());
        assertEquals(Phase.FINISHED, grandchild.getPhase());
    }

    @Test
    public void testHopOff_runsHooksInTransitionOrder() throws Throwable {
        List<String> events = new ArrayList<>();
        TestFacility facility =
                new TestFacility() {
                    @Override
                    protected void onTransitionFromStarted() {
                        // The parent is not FINISHED yet, so its Elements can still be read.
                        assertEquals(Phase.TRANSITIONING_FROM, getHostStation().getPhase());
                        events.add("Facility started");
                    }

                    @Override
                    protected void onTransitionFromFinished() {
                        events.add("Facility finished");
                    }
                };
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        TestStation station =
                new TestStation() {
                    @Override
                    protected void onTransitionFromStarted() {
                        events.add("Station started");
                    }

                    @Override
                    protected void onTransitionFromFinished() {
                        events.add("Station finished");
                    }
                };
        mTransitRule.runTransition(() -> TrafficControl.hopOnAt(activity, station, facility));

        TrafficControl.hopOffPublicTransit();

        assertEquals(
                List.of(
                        "Station started",
                        "Facility started",
                        "Station finished",
                        "Facility finished"),
                events);
    }

    @Test
    public void testSetParent_self_throws() {
        TestFacility facility = new TestFacility();

        assertThrows(AssertionError.class, () -> facility.setParent(facility));
    }

    @Test
    public void testSetParent_finishedParent_throws() throws Throwable {
        TestStation station = hopOn();
        TrafficControl.hopOffPublicTransit();

        TestFacility child = new TestFacility();

        assertThrows(AssertionError.class, () -> child.setParent(station));
    }

    /** Enters a new State, child of |parent|, whose exit Condition is fulfilled when hidden. */
    private State enterChildState(ConditionalState parent) throws Throwable {
        State state = new State("Child");
        state.declareExitCondition(
                InstrumentationThreadCondition.from("State hidden", () -> whether(!mShown.get())));
        state.setParent(parent);
        mTransitRule.runTransition(() -> Triggers.noopTo().enterState(state));
        return state;
    }
}
