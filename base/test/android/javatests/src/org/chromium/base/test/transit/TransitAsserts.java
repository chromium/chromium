// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.assertNotEquals;

import android.app.Activity;
import android.util.Pair;

import org.chromium.base.test.transit.ConditionalState.Phase;
import org.chromium.build.annotations.NullMarked;

import java.util.List;

/** Assertions specific to Public Transit. */
@NullMarked
public class TransitAsserts {
    private static final String TAG = "Transit";

    /**
     * Asserts that the given station is the final one in a test method and no further transitions
     * happened.
     *
     * <p>A different instance of the same subclass {@link Station} does not count; it must be the
     * {@link Station} instance returned by the last transition that traveled to a Station.
     *
     * @param expectedStation the {@link Station} expected to be the last
     * @param expectedFacilities the {@link Facility}'s expected to be the active
     * @throws AssertionError if the final station is not the same as |expected| or any expect
     *     {@link Facility} was not active.
     */
    public static void assertFinalDestination(
            Station<?> expectedStation, Facility<?>... expectedFacilities) {
        List<Station<?>> activeStations = TrafficControl.getActiveStations();
        if (activeStations.size() != 1) {
            raiseAssertion(
                    String.format(
                            "Expected exactly one active station, but found %d",
                            activeStations.size()));
        }
        Station<?> activeStation = activeStations.get(0);
        if (activeStation != expectedStation) {
            raiseAssertion(
                    String.format(
                            "Expected final destination to be %s, but was %s",
                            expectedStation, activeStation));
        }
        @Phase int phase = expectedStation.getPhase();
        if (phase != Phase.ACTIVE) {
            raiseAssertion(
                    String.format(
                            "Station %s expected to be the final one and ACTIVE, but it is %s",
                            expectedStation, ConditionalState.phaseToString(phase)));
        }

        for (Facility<?> facility : expectedFacilities) {
            phase = facility.getPhase();
            if (phase != Phase.ACTIVE) {
                raiseAssertion(
                        String.format(
                                "Facility %s expected to be ACTIVE at the end, but it is in %s",
                                facility, ConditionalState.phaseToString(phase)));
            }
        }
    }

    /**
     * Asserts that the given stations are the final ones in a test method and no further
     * transitions happened.
     *
     * <p>Version of {@link #assertFinalDestination(Station, Facility...)} when ending with multiple
     * windows.
     */
    public static void assertFinalDestinations(Station<?>... expectedStations) {
        List<Station<?>> activeStations = TrafficControl.getActiveStations();
        for (Station<?> expectedStation : expectedStations) {
            if (!activeStations.contains(expectedStation)) {
                raiseAssertion(
                        String.format(
                                "Expected %s to be one of the final destinations, but it was not"
                                        + " active",
                                expectedStation));
            }
        }
        if (activeStations.size() > expectedStations.length) {
            raiseAssertion("Too many stations were active");
        }
    }

    /**
     * Asserts the current station is of a given expected type.
     *
     * @param stationType the expected type of {@link Station}
     * @param situation a String describing the context of the check for a clear assert message
     * @param allowNull whether no active station is considered an expected state
     */
    public static void assertCurrentStationType(
            Class<? extends Station<?>> stationType, String situation, boolean allowNull) {
        List<Station<?>> activeStations = TrafficControl.getActiveStations();
        if (activeStations.size() == 0) {
            if (!allowNull) {
                raiseAssertion(
                        String.format(
                                "Expected exactly one active station, but found %d",
                                activeStations.size()));
            }
        } else if (activeStations.size() == 1) {
            Station<?> activeStation = activeStations.get(0);
            if (!stationType.isInstance(activeStation)) {
                raiseAssertion(
                        String.format(
                                "Expected current station to be of type <%s> at <%s>, but was"
                                        + " actually of type <%s>",
                                stationType,
                                situation,
                                activeStation != null ? activeStation.getClass() : "null"));
            }
        } else { // if (activeStations.size() > 1)
            raiseAssertion(
                    String.format(
                            "Expected exactly one active station, but found %d",
                            activeStations.size()));
        }
    }

    private static void raiseAssertion(String message) {
        List<Pair<String, String>> allStationsNames = TrafficControl.getAllStationsNames();
        assert false : message + "\n" + stationListToString(allStationsNames);
    }

    private static String stationListToString(List<Pair<String, String>> allStations) {
        StringBuilder builder = new StringBuilder();
        int i = 1;
        for (Pair<String, String> pair : allStations) {
            String stationName = pair.second;
            String testName = pair.first != null ? pair.first : "__outside_test__";
            builder.append(String.format("  (%s) %s (#%s)\n", i, stationName, testName));
            i++;
        }
        return builder.toString();
    }

    /** Asserts that the given stations are in different Activities and Tasks. */
    public static void assertInDifferentTasks(Station<?> station1, Station<?> station2) {
        Activity activity1 = station1.getActivity();
        Activity activity2 = station2.getActivity();

        assertNotEquals(activity1, activity2);
        assertNotEquals(activity1.getTaskId(), activity2.getTaskId());
    }
}
