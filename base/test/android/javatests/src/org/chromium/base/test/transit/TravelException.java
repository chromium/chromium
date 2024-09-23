// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

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
     * @return a new TravelException instance
     */
    public static TravelException newTravelException(String message) {
        return newTravelException(message, /* cause= */ null);
    }

    /**
     * Factory method for TravelException from a raw String message with an underlying cause.
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
}
