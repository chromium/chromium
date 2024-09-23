// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.base.Callback;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Interface to fetch SuggestionEntry data backed by a service (e.g., Sync) that needs explicit
 * trigger to update.
 */
public interface SuggestionBackend {
    // Suggestions older than 24h are considered stale, and rejected.
    public static final long STALENESS_THRESHOLD_MS = TimeUnit.HOURS.toMillis(24);

    /** Cleans up class and owned objects. */
    public void destroy();

    /**
     * Triggers update on the backend, debounced. If update completes, the update listener (if set)
     * gets called. This is not guaranteed to happen in reasonable time after trigger.
     */
    public void triggerUpdate();

    /** Assigns a listener for the update that might or might not follow triggerUpdate(). */
    public void setUpdateObserver(Runnable listener);

    /**
     * Reads a list of SuggstionEntry. This should run relatively fast, and may pass back cached or
     * stale results.
     */
    public void read(Callback<List<SuggestionEntry>> callback);
}
