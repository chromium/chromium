// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.test.transit.ConditionalState.Phase;

import java.util.List;

/** Assertions specific to Public Transit. */
public class TransitAsserts {
    private static final String TAG = "Transit";

    /**
     * Asserts that the given station is the final one in a test method and no further transitions
     * happened.
     *
     * <p>A different instance of the same subclass {@link TransitStation} does not count; it must
     * be the TransitStation instance returned by the last {@link Trip} transition.
     *
     * @param expected the TransitStation expected to be the last
     * @throws AssertionError if the final station is not the same as |expected|
     */
    public static void assertFinalDestination(TransitStation expected) {
        TransitStation activeStation = TrafficControl.getActiveStation();
        if (activeStation != expected) {
            raiseAssertion(
                    String.format(
                            "Expected final destination to be %s, but was %s",
                            expected, activeStation));
        }
        if (expected.getPhase() != Phase.ACTIVE) {
            raiseAssertion(
                    String.format(
                            "Station %s expected to be the final one, but it is not ACTIVE",
                            expected));
        }
    }

    /**
     * Asserts the current station is of a given expected type.
     *
     * @param stationType the expected type of {@link TransitStation}
     * @param situation a String describing the context of the check for a clear assert message
     * @param allowNull whether no active station is considered an expected state
     */
    public static void assertCurrentStationType(
            Class<? extends TransitStation> stationType, String situation, boolean allowNull) {
        TransitStation activeStation = TrafficControl.getActiveStation();
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
        List<TransitStation> allStations = TrafficControl.getAllStations();
        assert false : message + "\n" + stationListToString(allStations);
    }

    private static String stationListToString(List<TransitStation> allStations) {
        StringBuilder builder = new StringBuilder();
        int i = 1;
        for (TransitStation station : allStations) {
            builder.append(
                    String.format(
                            "  [%d] (%s) %s\n",
                            i, ConditionalState.phaseToShortString(station.getPhase()), station));
            i++;
        }
        return builder.toString();
    }
}
