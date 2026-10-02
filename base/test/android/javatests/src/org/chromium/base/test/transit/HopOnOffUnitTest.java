// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.transit.ConditionalState.Phase;
import org.chromium.build.annotations.Nullable;

import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for hopping on and off Public Transit. */
@RunWith(BaseRobolectricTestRunner.class)
public class HopOnOffUnitTest {

    /** An Activity subclass, to check hop-on matches by instance rather than by exact class. */
    public static class SubActivity extends Activity {}

    /** A Station in an Activity, with a single Element fulfilled as soon as it is checked. */
    private static class TestStation extends Station<Activity> {
        public final Element<String> valueElement;

        TestStation() {
            this(/* withUnfulfillableExitCondition= */ false);
        }

        TestStation(boolean withUnfulfillableExitCondition) {
            this(Activity.class, withUnfulfillableExitCondition);
        }

        TestStation(
                @Nullable Class<Activity> activityClass, boolean withUnfulfillableExitCondition) {
            super(activityClass);
            valueElement = declareEnterConditionAsElement(new AlwaysFulfilledCondition());
            if (withUnfulfillableExitCondition) {
                declareExitCondition(
                        InstrumentationThreadCondition.from(
                                "Never fulfilled exit Condition", Condition::notFulfilled));
            }
        }
    }

    /** A Facility with a single Element fulfilled as soon as it is checked. */
    private static class TestFacility extends Facility<TestStation> {
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

    @After
    public void tearDown() {
        TrafficControl.hopOffPublicTransit();
    }

    /** Create a RESUMED Activity, tracked by ApplicationStatus. */
    private static <T extends Activity> T resumeActivity(Class<T> activityClass) {
        return Robolectric.buildActivity(activityClass).setup().get();
    }

    /**
     * Transitions must not run on the UI thread, which is the thread Robolectric runs tests on, so
     * run them in a separate thread and propagate any failure to the test thread.
     */
    private static void runTransition(Runnable transition) throws Throwable {
        AtomicReference<Throwable> maybeException = new AtomicReference<>();
        Thread transitionThread = new Thread(transition);
        transitionThread.setUncaughtExceptionHandler((thread, ex) -> maybeException.set(ex));
        transitionThread.start();
        transitionThread.join();
        Throwable exception = maybeException.get();
        if (exception != null) {
            throw exception;
        }
    }

    private static <T extends Station<?>> T hopOnTo(
            Activity activity, T station, Facility<?>... facilities) throws Throwable {
        runTransition(() -> Triggers.noopTo().hopOnTo(activity, station, facilities));
        return station;
    }

    @Test
    public void testHopOn_fromOutsidePublicTransit() throws Throwable {
        Activity activity = resumeActivity(Activity.class);

        TestStation station = hopOnTo(activity, new TestStation());

        assertEquals(Phase.ACTIVE, station.getPhase());
        assertEquals(List.of(station), TrafficControl.getActiveStations());
        assertSame(activity, station.getActivity());
        assertEquals("value", station.valueElement.value());
    }

    @Test
    public void testHopOn_matchesTheGivenInstanceAmongSameClassActivities() throws Throwable {
        // Two windows of the same Activity class; matching by class would be ambiguous.
        Activity firstWindow = resumeActivity(Activity.class);
        Activity secondWindow = resumeActivity(Activity.class);

        TestStation station = hopOnTo(secondWindow, new TestStation());

        assertSame(secondWindow, station.getActivity());
        assertFalse(firstWindow == station.getActivity());
    }

    @Test
    public void testHopOn_acceptsSubclassInstance() throws Throwable {
        // A Station<Activity> can be hopped onto in a SubActivity; the instance is what matters.
        SubActivity activity = resumeActivity(SubActivity.class);

        TestStation station = hopOnTo(activity, new TestStation());

        assertSame(activity, station.getActivity());
    }

    @Test
    public void testHopOn_rejectsInstanceOfWrongClass() {
        Activity activity = resumeActivity(Activity.class);
        // The Station requires a SubActivity; a plain Activity instance cannot satisfy it.
        Station<SubActivity> station = new Station<>(SubActivity.class) {};

        assertThrows(AssertionError.class, () -> station.requireActivityInstance(activity));
    }

    @Test
    public void testHopOn_discardsStationLeftActive() throws Throwable {
        Activity activity = resumeActivity(Activity.class);
        TestStation abandoned = hopOnTo(activity, new TestStation());

        // Simulates a test doing something Public Transit cannot see - e.g. raw Espresso - and
        // then calling another test util that hops on again.
        TestStation station = hopOnTo(activity, new TestStation());

        assertEquals(Phase.FINISHED, abandoned.getPhase());
        assertEquals(Phase.ACTIVE, station.getPhase());
        assertEquals(List.of(station), TrafficControl.getActiveStations());
    }

    @Test
    public void testHopOff_finishesStationAndFacilitiesSoStaleElementsFail() throws Throwable {
        Activity activity = resumeActivity(Activity.class);
        TestFacility facility = new TestFacility();
        TestStation station = hopOnTo(activity, new TestStation(), facility);
        assertEquals(Phase.ACTIVE, facility.getPhase());
        assertEquals("value", facility.valueElement.value());

        TrafficControl.hopOffPublicTransit();

        assertEquals(Phase.FINISHED, station.getPhase());
        assertEquals(Phase.FINISHED, facility.getPhase());
        assertTrue(TrafficControl.getActiveStations().isEmpty());
        // Reading an Element of a hopped off Station or Facility must fail loudly instead of
        // returning a stale object.
        assertThrows(AssertionError.class, station.valueElement::value);
        assertThrows(AssertionError.class, facility.valueElement::value);
    }

    @Test
    public void testHopOff_finishesStationLeftTransitioningFrom() throws Throwable {
        Activity activity = resumeActivity(Activity.class);
        TestStation station = hopOnTo(activity, new TestStation());
        // A failed Transition leaves its origin in TRANSITIONING_FROM. Hopping off must not
        // assert on that, or one failed test would make the next test's hop-off fail too.
        station.setStateTransitioningFrom();

        TrafficControl.hopOffPublicTransit();

        assertEquals(Phase.FINISHED, station.getPhase());
        assertTrue(TrafficControl.getActiveStations().isEmpty());
    }

    @Test
    public void testHopOff_isIdempotent() throws Throwable {
        Activity activity = resumeActivity(Activity.class);
        TestStation station = hopOnTo(activity, new TestStation());

        TrafficControl.hopOffPublicTransit();
        TrafficControl.hopOffPublicTransit();

        assertEquals(Phase.FINISHED, station.getPhase());
    }

    @Test
    public void testHopOn_doesNotCheckExitConditions() throws Throwable {
        Activity activity = resumeActivity(Activity.class);
        // Hopping on makes no claim about what happened while outside Public Transit, so the exit
        // Conditions of the Station left behind must not be checked; if they were, this Station
        // could never be left.
        hopOnTo(activity, new TestStation(/* withUnfulfillableExitCondition= */ true));

        TestStation station = hopOnTo(activity, new TestStation());

        assertEquals(Phase.ACTIVE, station.getPhase());
    }

    @Test
    public void testHopOn_withMultipleStationsActive() throws Throwable {
        // Two active Stations, as in multi-window, make the origin Station impossible to infer.
        // Hopping on does not need an origin Station, so it still works, and hops off from both.
        Activity firstActivity = resumeActivity(Activity.class);
        TestStation firstWindow = hopOnTo(firstActivity, new TestStation());
        // Activated without a Transition, so it has no resolved ActivityElement; make it
        // Activity-less, like EntryPointSentinelStation, so it is tracked as active regardless.
        TestStation secondWindow =
                new TestStation(
                        /* activityClass= */ null, /* withUnfulfillableExitCondition= */ false);
        secondWindow.setStateActiveWithoutTransition();
        TrafficControl.notifyActiveStationsChanged(List.of(), List.of(secondWindow));
        assertEquals(2, TrafficControl.getActiveStations().size());

        TestStation station = hopOnTo(firstActivity, new TestStation());

        assertEquals(Phase.FINISHED, firstWindow.getPhase());
        assertEquals(Phase.FINISHED, secondWindow.getPhase());
        assertEquals(Phase.ACTIVE, station.getPhase());
        assertEquals(List.of(station), TrafficControl.getActiveStations());
    }

    @Test
    public void testHoppedOnStation_isUsableAsOriginOfNormalTransition() throws Throwable {
        // Hop on in a SubActivity, then travel to a Station<Activity>: requireToBeInSameTask()
        // must accept the origin's subclass instance rather than demand the exact declared class.
        SubActivity activity = resumeActivity(SubActivity.class);
        TestStation hoppedOn = hopOnTo(activity, new TestStation());

        TestStation destination = new TestStation();
        runTransition(() -> Triggers.noopTo().withContext(hoppedOn).arriveAt(destination));

        assertEquals(Phase.FINISHED, hoppedOn.getPhase());
        assertEquals(Phase.ACTIVE, destination.getPhase());
        assertSame(activity, destination.getActivity());
        assertFalse(TrafficControl.getActiveStations().contains(hoppedOn));
    }
}
