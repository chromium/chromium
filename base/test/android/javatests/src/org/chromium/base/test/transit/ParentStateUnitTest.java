// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.transit.Condition.whether;

import android.app.Activity;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.transit.ConditionalState.Phase;
import org.chromium.base.test.transit.PublicTransitUnitTestRule.AsyncTransition;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

/** Unit tests for parent and child {@link ConditionalState}s. */
@RunWith(BaseRobolectricTestRunner.class)
public class ParentStateUnitTest {
    @Rule public final PublicTransitUnitTestRule mTransitRule = new PublicTransitUnitTestRule();

    /** Whether the Views of the test Facilities and States are shown. */
    private final AtomicBoolean mShown = new AtomicBoolean(true);

    private @Nullable ActivityController<Activity> mActivityController;

    private static class TestStation extends Station<Activity> {
        TestStation() {
            super(Activity.class);
        }

        TestStation(@Nullable Class<Activity> activityClass) {
            super(activityClass);
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

    /** An Element present while |mShown| is true, with an EXIT Condition. */
    private class ShownElement extends Element<String> {
        ShownElement() {
            super("ShownElement");
        }

        @Override
        public ConditionWithResult<String> createEnterCondition() {
            return new ConditionWithResult<>(/* isRunOnUiThread= */ false) {
                @Override
                protected ConditionStatusWithResult<String> resolveWithSuppliers() {
                    return mShown.get()
                            ? fulfilled().withResult("value")
                            : notFulfilled().withoutResult();
                }

                @Override
                public String buildDescription() {
                    return "ShownElement shown";
                }
            };
        }

        @Override
        public Condition createExitCondition() {
            return new Condition(/* isRunOnUiThread= */ false) {
                @Override
                protected ConditionStatus checkWithSuppliers() {
                    return whether(!mShown.get());
                }

                @Override
                public String buildDescription() {
                    return "ShownElement hidden";
                }
            };
        }
    }

    private TestStation hopOn(Facility<?>... facilities) throws Throwable {
        mActivityController = Robolectric.buildActivity(Activity.class).setup();
        Activity activity = mActivityController.get();
        TestStation station = new TestStation();
        mTransitRule.runTransition(() -> TrafficControl.hopOnAt(activity, station, facilities));
        return station;
    }

    private void reachLastStopDestroyingActivity(Station<?> station) throws Throwable {
        AsyncTransition transition =
                mTransitRule.startTransition(() -> station.noopTo().reachLastStop());
        Objects.requireNonNull(mActivityController).pause().stop().destroy();
        transition.join();
    }

    /** Enters a new State child of |parent| that declares a {@link ShownElement}. */
    private State enterChildState(ConditionalState parent) throws Throwable {
        return enterChildState(new State("Child"), parent);
    }

    private State enterChildState(State state, ConditionalState parent) throws Throwable {
        state.setParent(parent);
        state.declareElement(new ShownElement());
        mTransitRule.runTransition(() -> Triggers.noopTo().enterState(state));
        return state;
    }

    // Facilities as children of Stations.
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
    public void testEnterState_withoutParent_hasNoParent() throws Throwable {
        TestStation station = hopOn();
        State state = new State("NoParent");

        mTransitRule.runTransition(() -> station.noopTo().enterState(state));

        assertNull(state.getParent());
        assertTrue(station.getActiveChildren().isEmpty());
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
        State child = enterChildState(station);

        mShown.set(false);
        reachLastStopDestroyingActivity(station);

        assertEquals(Phase.FINISHED, station.getPhase());
        assertEquals(Phase.FINISHED, facility.getPhase());
        assertEquals(Phase.FINISHED, child.getPhase());
    }

    @Test
    public void testReachLastStop_activityNotDestroyed_throws() throws Throwable {
        TestStation station = hopOn();

        TravelException e =
                mTransitRule.runTransitionExpectingThrow(
                        TravelException.class,
                        () -> station.noopTo().withTimeout(100).reachLastStop());
        assertTrue(e.getMessage(), e.getMessage().contains("Activity is DESTROYED: Activity"));
    }

    @Test
    public void testReachLastStop_alreadyExpectingActivityDestroyed_isIdempotent()
            throws Throwable {
        TestStation station = hopOn();
        Objects.requireNonNull(station.getActivityElement()).expectActivityDestroyed();

        reachLastStopDestroyingActivity(station);

        assertEquals(Phase.FINISHED, station.getPhase());
    }

    @Test
    public void testExitStation_doesNotExitFinishedChildrenAgain() throws Throwable {
        TestFacility facility = new TestFacility();
        TestStation station = hopOn(facility);
        mShown.set(false);
        mTransitRule.runTransition(() -> station.noopTo().exitFacility(facility));

        // The finished Facility is no longer a child, so it is not exited again with the Station.
        assertTrue(station.getActiveChildren().isEmpty());
        reachLastStopDestroyingActivity(station);

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

    // Facilities as children of Facilities.

    @Test
    public void testEnterFacilityWithFacilityParent() throws Throwable {
        TestFacility parent = new TestFacility();
        TestStation station = hopOn(parent);
        TestFacility child = new TestFacility();

        child.setParent(parent);
        mTransitRule.runTransition(() -> station.noopTo().enterFacility(child));

        assertSame(parent, child.getParent());
        assertSame(station, child.getHostStation());
        assertEquals(List.of(child), parent.getActiveChildren());
        assertEquals(List.of(parent), station.getActiveChildren());
    }

    @Test
    public void testSetParent_facilityNotEnteredYet_throws() throws Throwable {
        TestStation station = hopOn();
        TestFacility parent = new TestFacility();
        // Give the parent a host Station so only its Phase.NEW makes setParent() throw.
        parent.setParent(station);

        TestFacility child = new TestFacility();

        AssertionError e = assertThrows(AssertionError.class, () -> child.setParent(parent));
        assertTrue(e.getMessage(), e.getMessage().contains("should have been in Phase.ACTIVE"));
    }

    @Test
    public void testExitParentFacility_exitsChildFacility() throws Throwable {
        TestFacility parent = new TestFacility();
        TestStation station = hopOn(parent);
        TestFacility child = new TestFacility();
        child.setParent(parent);
        mTransitRule.runTransition(() -> station.noopTo().enterFacility(child));

        mShown.set(false);
        mTransitRule.runTransition(() -> station.noopTo().exitFacility(parent));

        assertEquals(Phase.FINISHED, parent.getPhase());
        assertEquals(Phase.FINISHED, child.getPhase());
        assertEquals(Phase.ACTIVE, station.getPhase());
    }

    @Test
    public void testEnterChildOfExitedParent_throws() throws Throwable {
        TestFacility parent = new TestFacility();
        TestStation station = hopOn(parent);
        TestFacility child = new TestFacility();
        child.setParent(parent);

        AssertionError e =
                mTransitRule.runTransitionExpectingThrow(
                        AssertionError.class,
                        () ->
                                station.noopTo()
                                        .exitFacilityAnd(parent)
                                        .enterFacilityAnd(child)
                                        .complete());
        assertTrue(e.getMessage(), e.getMessage().contains("which is being exited"));
    }

    @Test
    public void testEnterChildOfAlreadyFinishedParent_throws() throws Throwable {
        TestFacility parent = new TestFacility();
        TestStation station = hopOn(parent);
        State child = new State("Child");
        child.setParent(parent);

        mShown.set(false);
        mTransitRule.runTransition(() -> station.noopTo().exitFacility(parent));

        AssertionError e =
                mTransitRule.runTransitionExpectingThrow(
                        AssertionError.class, () -> Triggers.noopTo().enterState(child));
        assertTrue(e.getMessage(), e.getMessage().contains("Phase.FINISHED"));
    }

    @Test
    public void testEnterChildOfUnenteredNewParent_throws() throws Throwable {
        hopOn();
        State parent = new State("Parent");
        State child = new State("Child");
        child.setParent(parent);

        AssertionError e =
                mTransitRule.runTransitionExpectingThrow(
                        AssertionError.class, () -> Triggers.noopTo().enterState(child));
        assertTrue(e.getMessage(), e.getMessage().contains("Phase.NEW"));
    }

    @Test
    public void testEnterFacilityWithFacilityParent_withoutContextStation() throws Throwable {
        TestFacility parent = new TestFacility();
        TestStation station1 = hopOn(parent);
        TestStation station2 = new TestStation(/* activityClass= */ null);
        mTransitRule.runTransition(
                () -> station1.noopTo().reach(Outcome.enter(station2).inNewTask()));

        // Two Stations are active, so a context Station cannot be inferred, but entering a
        // Facility that already has a Facility parent does not need one.
        TestFacility child = new TestFacility();
        child.setParent(parent);
        mTransitRule.runTransition(() -> Triggers.noopTo().enterFacility(child));

        assertSame(parent, child.getParent());
        assertSame(station1, child.getHostStation());
        assertEquals(Phase.ACTIVE, child.getPhase());
    }

    @Test
    public void testSetParent_cycle_throws() {
        State a = new State("A");
        State b = new State("B");
        a.setParent(b);

        assertThrows(AssertionError.class, () -> b.setParent(a));
    }

    @Test
    public void testHopOff_skipsNewChild() throws Throwable {
        TestStation station = hopOn();
        List<String> events = new ArrayList<>();
        State unenteredChild =
                new State("Unentered") {
                    @Override
                    protected void onTransitionFromStarted() {
                        events.add("started");
                    }

                    @Override
                    protected void onTransitionFromFinished() {
                        events.add("finished");
                    }
                };
        unenteredChild.setParent(station);

        TrafficControl.hopOffPublicTransit();

        assertEquals(Phase.NEW, unenteredChild.getPhase());
        assertTrue(events.isEmpty());
    }

    @Test
    public void testSetParent_twice_throws() throws Throwable {
        TestStation station = hopOn();
        TestFacility facility = new TestFacility();
        facility.setParent(station);

        assertThrows(AssertionError.class, () -> facility.setParent(station));
    }

    @Test
    public void testSetParent_stateParentOfFacility_throws() throws Throwable {
        TestStation station = hopOn();
        State state = enterChildState(station);

        TestFacility child = new TestFacility();

        assertThrows(AssertionError.class, () -> child.setParent(state));
    }

    // States as children.

    @Test
    public void testExitFacility_waitsForExitConditionsOfChildState() throws Throwable {
        TestFacility facility = new TestFacility();
        TestStation station = hopOn(facility);
        enterChildState(facility);

        // The Element of the child State is still shown, so exiting the Facility times out.
        TravelException e =
                mTransitRule.runTransitionExpectingThrow(
                        TravelException.class,
                        () -> station.noopTo().withTimeout(100).exitFacility(facility));
        assertTrue(e.getMessage(), e.getMessage().contains("ShownElement hidden"));
    }

    @Test
    public void testArriveAtStation_exitsChildStatesOfStationAndItsFacilities() throws Throwable {
        TestFacility facility = new TestFacility();
        TestStation station = hopOn(facility);
        State childOfStation = enterChildState(station);
        State childOfFacility = enterChildState(facility);

        mShown.set(false);
        TestStation destination = new TestStation();
        mTransitRule.runTransition(() -> station.noopTo().arriveAt(destination));

        assertEquals(Phase.FINISHED, childOfStation.getPhase());
        assertEquals(Phase.FINISHED, childOfFacility.getPhase());
        assertEquals(Phase.ACTIVE, destination.getPhase());
    }

    @Test
    public void testExitParent_exitsNestedChildStates() throws Throwable {
        TestFacility facility = new TestFacility();
        TestStation station = hopOn(facility);
        State child = enterChildState(facility);
        State grandchild = enterChildState(child);

        mShown.set(false);
        mTransitRule.runTransition(() -> station.noopTo().exitFacility(facility));

        assertEquals(Phase.FINISHED, child.getPhase());
        assertEquals(Phase.FINISHED, grandchild.getPhase());
    }

    @Test
    public void testExitChildStateThenParent() throws Throwable {
        TestFacility facility = new TestFacility();
        TestStation station = hopOn(facility);
        State child = enterChildState(facility);

        // The child State's EXIT Conditions are waited for once, when it is exited explicitly;
        // exiting the parent afterwards doesn't exit it again.
        mShown.set(false);
        mTransitRule.runTransition(() -> Triggers.noopTo().exitState(child));
        mTransitRule.runTransition(() -> station.noopTo().exitFacility(facility));

        assertEquals(Phase.FINISHED, child.getPhase());
        assertEquals(Phase.FINISHED, facility.getPhase());
    }

    @Test
    public void testHopOff_finishesChildStatesSoStaleElementsFail() throws Throwable {
        TestFacility facility = new TestFacility();
        hopOn(facility);
        State child = enterChildState(facility);
        Element<?> element = child.getElements().getElements().get(0);
        assertEquals("value", element.value());

        TrafficControl.hopOffPublicTransit();

        assertEquals(Phase.FINISHED, child.getPhase());
        assertThrows(AssertionError.class, element::value);
    }

    @Test
    public void testShouldSkipExitConditions() throws Throwable {
        TestStation station = hopOn();
        TestFacility facility = new TestFacility();
        mTransitRule.runTransition(() -> station.noopTo().enterFacility(facility));
        State skipping =
                new State("Skipping") {
                    @Override
                    boolean shouldSkipExitConditions(Set<String> destinationElementIds) {
                        return true;
                    }
                };
        enterChildState(skipping, facility);

        // The ShownElement is still shown, but its EXIT Condition is skipped. Only the
        // Facility's own exit Condition is waited for.
        TravelException e =
                mTransitRule.runTransitionExpectingThrow(
                        TravelException.class,
                        () -> station.noopTo().withTimeout(100).exitFacility(facility));
        assertTrue(e.getMessage(), e.getMessage().contains("Facility hidden"));
        assertTrue(e.getMessage(), !e.getMessage().contains("ShownElement hidden"));
    }

    @Test
    public void testChildState_usesActivityOfParent() throws Throwable {
        TestStation station = hopOn();
        State child = enterChildState(station);

        ActivityElement<?> activityElement = child.determineActivityElement();
        assertNotNull(activityElement);
        assertSame(station.determineActivityElement(), activityElement);
    }

    @Test
    public void testChildState_withOwnActivityElement_overridesParentActivity() throws Throwable {
        TestStation station = hopOn();
        State child = new State("ChildWithActivity");
        child.setParent(station);
        ActivityElement<Activity> ownActivity = child.declareActivity(Activity.class);

        assertSame(ownActivity, child.determineActivityElement());
    }

    @Test
    public void testSetParent_afterEntering_throws() throws Throwable {
        TestStation station = hopOn();
        State state = new State("NoParent");
        mTransitRule.runTransition(() -> Triggers.noopTo().enterState(state));

        assertThrows(AssertionError.class, () -> state.setParent(station));
    }
}
