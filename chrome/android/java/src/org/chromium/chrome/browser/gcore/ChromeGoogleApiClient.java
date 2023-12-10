// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gcore;

/**
 * Client used to communicate with GmsCore using Google API. Meant to be
 * used in a synchronous way with {@link ConnectedTask}.
 */
public interface ChromeGoogleApiClient {
    /**
     * Connects the GMS Core client.
     * Does nothing if the client is already connected.
     * @param timeoutMillis after which connection is considered unsuccessful
     * @return whether connection was successful
     */
    boolean connectWithTimeout(long timeoutMillis);

    /**
     * Disconnects client.
     * Can be safely called on disconnected clients. It will then just clear the queued calls
     * submitted since the last disconnection.
     */
    void disconnect();

    /** Checks if Google Play Services are available. */
    boolean isGooglePlayServicesAvailable();
}
