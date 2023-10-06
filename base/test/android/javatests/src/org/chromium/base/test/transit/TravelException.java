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
    public TravelException(
            @Nullable TransitStation fromStation, TransitStation toStation, Throwable cause) {
        super(
                "Did not complete transition from "
                        + (fromStation != null ? fromStation.toString() : "<entry point>")
                        + " to "
                        + toStation,
                cause);
    }

    public TravelException(String message, StationFacility facility, Throwable cause) {
        super(message + " " + facility, cause);
    }

    static TravelException newEnterFacilityException(StationFacility facility, Throwable cause) {
        return new TravelException("Did not enter", facility, cause);
    }

    static TravelException newExitFacilityException(StationFacility facility, Throwable cause) {
        return new TravelException("Did not exit", facility, cause);
    }
}
