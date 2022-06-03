// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import java.util.Map;

/**
 * Persists user-interaction outcomes and the associated EventID used to track them.
 * This allows recording a user's interaction with the feature and sending that to
 * the server to facilitate offline quality analysis.
 */
interface ContextualSearchInteractionPersister {
    /** An EventID of 0 means no event ID is available.  Don't persist. */
    public static final long NO_EVENT_ID = 0;

    /** An interaction value of 0 means no interaction. */
    public static final int NO_INTERACTION = 0;

    /**
     * Gets the current persisted interaction and returns it, after clearing the persisted state.
     * After this call {@link #persistInteractions} must be called to store a non-empty interaction.
     * @return A new {@link PersistedInteraction} that is often empty due to the server not
     *         providing any EventID.
     */
    PersistedInteraction getAndClearPersistedInteraction();

    /**
     * Persists the given ID and outcomes to local storage.
     * The outcomes Map keys should contain @Feature int values only. The Objects that are values in
     * this map must be Booleans, but could some day be Integers.
     * @param eventId A non-zero event ID to store.
     * @param outcomesMap A map of outcome features and associated values.
     */
    void persistInteractions(
            long eventId, Map</* @Feature */ Integer, /* Boolean */ Object> outcomesMap);

    /** Provides access to a {@link PersistedInteraction} through getters. */
    interface PersistedInteraction {
        /** Gets the Event ID previously sent by the server during a resolve request. */
        long getEventId();

        /** Gets the bit-encoded user interactions associated with that event. */
        int getEncodedUserInteractions();

        /** Gets the time stamp of this user interaction in milliseconds. */
        long getTimestampMs();
    }
}
