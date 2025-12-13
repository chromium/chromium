// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.CallbackUtils;
import org.chromium.base.Promise;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Function;

/** Class that tracks the mapping between tokens and fully-qualified domain names (FQDNs). */
@NullMarked
public class TokenTracker {
    private final Promise<Map<String, String>> mRootPromise;
    private @MonotonicNonNull TokenGenerator mTokenGenerator;
    private final UsageStatsBridge mBridge;

    public TokenTracker(UsageStatsBridge bridge) {
        mBridge = bridge;
        mRootPromise = new Promise<>();
        mBridge.getAllTokenMappings(
                (result) -> {
                    long maxTokenValue = 0;
                    for (Map.Entry<String, String> entry : result.entrySet()) {
                        maxTokenValue = Math.max(maxTokenValue, Long.valueOf(entry.getKey()));
                    }

                    mTokenGenerator = new TokenGenerator(maxTokenValue + 1);
                    mRootPromise.fulfill(result);
                });

        // We need to add a placeholder exception handler so that Promise doesn't complain when we
        // call variants of then() that don't take a single callback. These variants set an
        // exception handler on the returned promise, so they expect there to be one on the root
        // promise.
        mRootPromise.except(CallbackUtils.<@Nullable Exception>emptyCallback());
    }

    /**
     * Associate a new token with FQDN, and return that token. If we're already tracking FQDN,
     * return the corresponding token. The returned promise will be fulfilled once persistence
     * succeeds, and rejected if persistence fails.
     */
    public Promise<String> startTrackingWebsite(String fqdn) {
        Promise<String> writePromise = new Promise<>();
        mRootPromise.then(
                (result) -> {
                    if (result.containsValue(fqdn)) {
                        // getFirstKeyForValue should be non-null because result.containsValue
                        writePromise.fulfill(assumeNonNull(getFirstKeyForValue(result, fqdn)));
                        return;
                    }

                    UsageStatsMetricsReporter.reportMetricsEvent(
                            UsageStatsMetricsEvent.START_TRACKING_TOKEN);
                    // mTokenGenerator is non-null because it is set when we get a result from
                    // mBridge.getAllTokenMappings, after which we use mRootPromise.fulfill.
                    // If mBridge.getAllTokenMappings fails, then we use mRootPromise.except.
                    // Here, we are in mRootPromise.then, so mTokenGenerator should be non-null.
                    String token = assumeNonNull(mTokenGenerator).nextToken();
                    Map<String, String> resultCopy = new HashMap<>(result);
                    resultCopy.put(token, fqdn);
                    mBridge.setTokenMappings(
                            resultCopy,
                            (didSucceed) -> {
                                if (didSucceed) {
                                    result.put(token, fqdn);
                                    writePromise.fulfill(token);
                                } else {
                                    writePromise.reject();
                                }
                            });
                },
                CallbackUtils.<@Nullable Exception>emptyCallback());

        return writePromise;
    }

    /**
     * Remove token and its associated FQDN, if we're already tracking token. The returned promise
     * will be fulfilled once persistence succeeds, and rejected if persistence fails.
     */
    public Promise<@Nullable Void> stopTrackingToken(String token) {
        Promise<@Nullable Void> writePromise = new Promise<>();
        mRootPromise.then(
                (result) -> {
                    if (!result.containsKey(token)) {
                        writePromise.fulfill(null);
                        return;
                    }

                    UsageStatsMetricsReporter.reportMetricsEvent(
                            UsageStatsMetricsEvent.STOP_TRACKING_TOKEN);
                    Map<String, String> resultCopy = new HashMap<>(result);
                    resultCopy.remove(token);
                    mBridge.setTokenMappings(
                            resultCopy,
                            (didSucceed) -> {
                                if (didSucceed) {
                                    result.remove(token);
                                    writePromise.fulfill(null);
                                } else {
                                    writePromise.reject();
                                }
                            });
                },
                CallbackUtils.<@Nullable Exception>emptyCallback());

        return writePromise;
    }

    /** Returns the token for a given FQDN, or null if we're not tracking that FQDN. */
    public Promise<@Nullable String> getTokenForFqdn(@Nullable String fqdn) {
        return mRootPromise.<@Nullable String>then(
                (Function<Map<String, String>, @Nullable String>)
                        (result) -> {
                            return getFirstKeyForValue(result, fqdn);
                        });
    }

    /** Get all the tokens we're tracking. */
    public Promise<List<String>> getAllTrackedTokens() {
        return mRootPromise.then(
                (Function<Map<String, String>, List<String>>)
                        (result) -> {
                            return new ArrayList<>(result.keySet());
                        });
    }

    private static @Nullable String getFirstKeyForValue(
            Map<String, String> map, @Nullable String value) {
        for (Map.Entry<String, String> entry : map.entrySet()) {
            if (entry.getValue().equals(value)) {
                return entry.getKey();
            }
        }

        return null;
    }
}
