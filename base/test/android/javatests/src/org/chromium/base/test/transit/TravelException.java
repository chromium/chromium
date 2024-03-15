// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

/**
 * {@link RuntimeException}s thrown by Public Transit transitions; the message of the wrapping
 * Exception give context to when the underlying Exception happened.
 */
public class TravelException extends RuntimeException {

    // Private, call one of the public factory methods instead.
    private TravelException(String message, Throwable cause) {
        super(message, cause);
    }

    /**
     * Factory method for TravelException from a raw String message.
     *
     * @param message the error message
     * @param cause the root cause
     * @return a new TravelException instance
     */
    public static TravelException newTravelException(String message, Throwable cause) {
        TravelException travelException = new TravelException(message, cause);
        PublicTransitConfig.onTravelException(travelException);
        return travelException;
    }

    /**
     * Factory method for TravelException for an error during a {@link Trip} between {@link
     * TransitStation}s.
     *
     * @param fromStation the origin station
     * @param toStation the destination station
     * @param cause the root cause
     * @return a new TravelException instance
     */
    public static TravelException newTripException(
            @Nullable TransitStation fromStation, TransitStation toStation, Throwable cause) {
        return newTravelException(
                "Did not complete transition from "
                        + (fromStation != null ? fromStation.toString() : "<entry point>")
                        + " to "
                        + toStation,
                cause);
    }

    /**
     * Factory method for TravelException for an error during a {@link FacilityCheckIn} into a
     * {@link StationFacility}.
     *
     * @param facility the facility being entered
     * @param cause the root cause
     * @return a new TravelException instance
     */
    public static TravelException newEnterFacilityException(
            StationFacility facility, Throwable cause) {
        return newFacilityTransitionException("Did not enter", facility, cause);
    }

    /**
     * Factory method for TravelException for an error during a {@link FacilityCheckOut} out of a
     * {@link StationFacility}.
     *
     * @param facility the facility being exited
     * @param cause the root cause
     * @return a new TravelException instance
     */
    public static TravelException newExitFacilityException(
            StationFacility facility, Throwable cause) {
        return newFacilityTransitionException("Did not exit", facility, cause);
    }

    private static TravelException newFacilityTransitionException(
            String message, StationFacility facility, Throwable cause) {
        return newTravelException(message + " " + facility, cause);
    }
}
