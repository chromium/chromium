// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import org.chromium.base.Promise;
import org.chromium.base.Promise.Function;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Class that tracks the mapping between tokens and fully-qualified domain names (FQDNs).
 */
public class TokenTracker {
    private Promise<Map<String, String>> mRootPromise;
    private TokenGenerator mTokenGenerator;
    private UsageStatsBridge mBridge;

    public TokenTracker(UsageStatsBridge bridge) {
        mBridge = bridge;
        mRootPromise = new Promise<>();
        mBridge.getAllTokenMappings((result) -> {
            long maxTokenValue = 0;
            for (Map.Entry<String, String> entry : result.entrySet()) {
                maxTokenValue = Math.max(maxTokenValue, Long.valueOf(entry.getKey()));
            }

            mTokenGenerator = new TokenGenerator(maxTokenValue + 1);
            mRootPromise.fulfill(result);
        });

        // We need to add a dummy exception handler so that Promise doesn't complain when we
        // call variants of then() that don't take a single callback. These variants set an
        // exception handler on the returned promise, so they expect there to be one on the root
        // promise.
        mRootPromise.except((e) -> {});
    }

    /**
     * Associate a new token with FQDN, and return that token.
     * If we're already tracking FQDN, return the corresponding token.
     * The returned promise will be fulfilled once persistence succeeds, and rejected if persistence
     * fails.
     */
    public Promise<String> startTrackingWebsite(String fqdn) {
        Promise<String> writePromise = new Promise<>();
        mRootPromise.then((result) -> {
            if (result.containsValue(fqdn)) {
                writePromise.fulfill(getFirstKeyForValue(result, fqdn));
                return;
            }

            UsageStatsMetricsReporter.reportMetricsEvent(
                    UsageStatsMetricsEvent.START_TRACKING_TOKEN);
            String token = mTokenGenerator.nextToken();
            Map<String, String> resultCopy = new HashMap<>(result);
            resultCopy.put(token, fqdn);
            mBridge.setTokenMappings(resultCopy, (didSucceed) -> {
                if (didSucceed) {
                    result.put(token, fqdn);
                    writePromise.fulfill(token);
                } else {
                    writePromise.reject();
                }
            });
        }, (e) -> {});

        return writePromise;
    }

    /**
     * Remove token and its associated FQDN, if we're  already tracking token.
     * The returned promise will be fulfilled once persistence succeeds, and rejected if persistence
     * fails.
     */
    public Promise<Void> stopTrackingToken(String token) {
        Promise<Void> writePromise = new Promise<>();
        mRootPromise.then((result) -> {
            if (!result.containsKey(token)) {
                writePromise.fulfill(null);
                return;
            }

            UsageStatsMetricsReporter.reportMetricsEvent(
                    UsageStatsMetricsEvent.STOP_TRACKING_TOKEN);
            Map<String, String> resultCopy = new HashMap<>(result);
            resultCopy.remove(token);
            mBridge.setTokenMappings(resultCopy, (didSucceed) -> {
                if (didSucceed) {
                    result.remove(token);
                    writePromise.fulfill(null);
                } else {
                    writePromise.reject();
                }
            });
        }, (e) -> {});

        return writePromise;
    }

    /** Returns the token for a given FQDN, or null if we're not tracking that FQDN. */
    public Promise<String> getTokenForFqdn(String fqdn) {
        return mRootPromise.then((Function<Map<String, String>, String>) (result) -> {
            return getFirstKeyForValue(result, fqdn);
        });
    }

    /** Get all the tokens we're tracking. */
    public Promise<List<String>> getAllTrackedTokens() {
        return mRootPromise.then((Function<Map<String, String>, List<String>>) (result) -> {
            return new ArrayList<>(result.keySet());
        });
    }

    private static String getFirstKeyForValue(Map<String, String> map, String value) {
        for (Map.Entry<String, String> entry : map.entrySet()) {
            if (entry.getValue().equals(value)) {
                return entry.getKey();
            }
        }

        return null;
    }
}