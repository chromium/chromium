// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.util.Pair;

import org.chromium.base.test.transit.ConditionalState.Phase;

import java.util.List;

/** Assertions specific to Public Transit. */
public class TransitAsserts {
    private static final String TAG = "Transit";

    /**
     * Asserts that the given station is the final one in a test method and no further transitions
     * happened.
     *
     * <p>A different instance of the same subclass {@link Station} does not count; it must be the
     * {@link Station} instance returned by the last {@link Trip} transition.
     *
     * @param expectedStation the {@link Station} expected to be the last
     * @param expectedFacilities the {@link Facility}'s expected to be the active
     * @throws AssertionError if the final station is not the same as |expected| or any expect
     *     {@link Facility} was not active.
     */
    public static void assertFinalDestination(
            Station expectedStation, Facility... expectedFacilities) {
        Station activeStation = TrafficControl.getActiveStation();
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

        for (Facility facility : expectedFacilities) {
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
     * Asserts the current station is of a given expected type.
     *
     * @param stationType the expected type of {@link Station}
     * @param situation a String describing the context of the check for a clear assert message
     * @param allowNull whether no active station is considered an expected state
     */
    public static void assertCurrentStationType(
            Class<? extends Station> stationType, String situation, boolean allowNull) {
        Station activeStation = TrafficControl.getActiveStation();
        if ((activeStation == null && !allowNull)
                || (activeStation != null && !stationType.isInstance(activeStation))) {
            raiseAssertion(
                    String.format(
                            "Expected current station to be of type <%s> at <%s>, but was actually"
                                    + " of type <%s>",
                            stationType,
                            situation,
                            activeStation != null ? activeStation.getClass() : "null"));
        }
    }

    private static void raiseAssertion(String message) {
        List<Pair<String, Station>> allStations = TrafficControl.getAllStations();
        assert false : message + "\n" + stationListToString(allStations);
    }

    private static String stationListToString(List<Pair<String, Station>> allStations) {
        StringBuilder builder = new StringBuilder();
        int i = 1;
        for (Pair<String, Station> pair : allStations) {
            Station station = pair.second;
            String testName = pair.first != null ? pair.first : "__outside_test__";
            builder.append(
                    String.format(
                            "  [%d] (%s) %s (#%s)\n",
                            i,
                            ConditionalState.phaseToShortString(station.getPhase()),
                            station,
                            testName));
            i++;
        }
        return builder.toString();
    }
}
