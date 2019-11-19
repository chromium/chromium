// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import org.chromium.base.Promise;

import java.util.Collections;
import java.util.List;

/**
 * Client for interacting with the Digital Wellbeing(DW) app on Android.
 * This implementation is inert; the implementation is downstream.
 * All methods are async, since completing a request to DW requires binding to a service, which is
 * an async process in Android.
 */
public class DigitalWellbeingClient {
    /**
     * Notify that the user's opt in state for sharing data with Digital Wellbeing has changed to
     * be <c>optInState</c>
     */
    public Promise<Void> notifyOptInStateChange(boolean optInState) {
        return Promise.fulfilled(null);
    }

    /**
     * Notify DW that the user has deleted history in the range [start, end), so it should delete
     * any Chrome-originating records in the same range.
     */
    public Promise<Void> notifyHistoryDeletion(long startTimeMs, long endTimeMs) {
        return Promise.fulfilled(null);
    }

    /**
     * Notify DW that the user has deleted history for the given fqdns.
     */
    public Promise<Void> notifyHistoryDeletion(List<String> fqdns) {
        return Promise.fulfilled(null);
    }

    /**
     * Notify DW that the user has deleted all history, so it should delete all Chrome-originating
     * records.
     */
    public Promise<Void> notifyAllHistoryCleared() {
        return Promise.fulfilled(null);
    }

    /**
     * Inform DW that Chrome will no longer report usage for the domain associated with
     * <c>token</c>.
     */
    public Promise<Void> cancelToken(String token) {
        return Promise.fulfilled(null);
    }

    /**
     * Inform DW that Chrome will no longer report usage for any domains, thus cancelling all
     * tokens associated with those domains.
     */
    public Promise<Void> cancelAllTokens() {
        return Promise.fulfilled(null);
    }

    /** Get a list of all the tokens that DW thinks Chrome is tracking. */
    public Promise<List<String>> getAllTrackedTokens() {
        return Promise.fulfilled(Collections.emptyList());
    }

    /** Get a list of all the websites that DW thinks Chrome should suspend. */
    public Promise<List<String>> getAllSuspendedWebsites() {
        return Promise.fulfilled(Collections.emptyList());
    }
}