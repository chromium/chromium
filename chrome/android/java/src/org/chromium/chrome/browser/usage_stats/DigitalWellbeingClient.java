// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import org.chromium.base.Promise;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Collections;
import java.util.List;

/**
 * Client for interacting with the Digital Wellbeing(DW) app on Android. This implementation is
 * inert; the implementation is downstream. All methods are async, since completing a request to DW
 * requires binding to a service, which is an async process in Android.
 */
@NullMarked
public class DigitalWellbeingClient {
    /**
     * Notify that the user's opt in state for sharing data with Digital Wellbeing has changed to be
     * <c>optInState</c>
     */
    public Promise<@Nullable Void> notifyOptInStateChange(boolean optInState) {
        // https://github.com/uber/NullAway/issues/1075#issuecomment-2698009946
        return Promise.<@Nullable Void>fulfilled(null);
    }

    /**
     * Notify DW that the user has deleted history in the range [start, end), so it should delete
     * any Chrome-originating records in the same range.
     */
    public Promise<@Nullable Void> notifyHistoryDeletion(long startTimeMs, long endTimeMs) {
        // https://github.com/uber/NullAway/issues/1075#issuecomment-2698009946
        return Promise.<@Nullable Void>fulfilled(null);
    }

    /** Notify DW that the user has deleted history for the given fqdns. */
    public Promise<@Nullable Void> notifyHistoryDeletion(List<String> fqdns) {
        // https://github.com/uber/NullAway/issues/1075#issuecomment-2698009946
        return Promise.<@Nullable Void>fulfilled(null);
    }

    /**
     * Notify DW that the user has deleted all history, so it should delete all Chrome-originating
     * records.
     */
    public Promise<@Nullable Void> notifyAllHistoryCleared() {
        // https://github.com/uber/NullAway/issues/1075#issuecomment-2698009946
        return Promise.<@Nullable Void>fulfilled(null);
    }

    /**
     * Inform DW that Chrome will no longer report usage for the domain associated with
     * <c>token</c>.
     */
    public Promise<@Nullable Void> cancelToken(String token) {
        // https://github.com/uber/NullAway/issues/1075#issuecomment-2698009946
        return Promise.<@Nullable Void>fulfilled(null);
    }

    /**
     * Inform DW that Chrome will no longer report usage for any domains, thus cancelling all tokens
     * associated with those domains.
     */
    public Promise<@Nullable Void> cancelAllTokens() {
        // https://github.com/uber/NullAway/issues/1075#issuecomment-2698009946
        return Promise.<@Nullable Void>fulfilled(null);
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
