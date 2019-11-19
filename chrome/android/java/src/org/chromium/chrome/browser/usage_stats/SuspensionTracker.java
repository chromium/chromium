// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import org.chromium.base.Promise;
import org.chromium.base.Promise.Function;

import java.util.ArrayList;
import java.util.List;

/**
 * Class that tracks which sites are currently suspended.
 */
public class SuspensionTracker {
    private final UsageStatsBridge mBridge;
    private final NotificationSuspender mNotificationSuspender;
    private final Promise<List<String>> mRootPromise;
    private Promise<Void> mWritePromise;

    public SuspensionTracker(UsageStatsBridge bridge, NotificationSuspender notificationSuspender) {
        mBridge = bridge;
        mNotificationSuspender = notificationSuspender;
        mRootPromise = new Promise<>();
        mBridge.getAllSuspensions((result) -> { mRootPromise.fulfill(result); });
        mWritePromise = Promise.fulfilled(null);
    }

    /**
     * Sets the status of <c>fqdns</c> to match <c>suspended</c>.
     * The returned promise will be fulfilled once persistence succeeds, and rejected if persistence
     * fails.
     */
    public Promise<Void> setWebsitesSuspended(List<String> fqdns, boolean suspended) {
        Promise<Void> newWritePromise = new Promise<>();
        mWritePromise.then((dummyResult) -> {
            mRootPromise.then((result) -> {
                // We copy result so that the mutation isn't reflected in result until persistence
                // succeeds.
                List<String> resultCopy = new ArrayList<>(result);
                if (suspended) {
                    UsageStatsMetricsReporter.reportMetricsEvent(
                            UsageStatsMetricsEvent.SUSPEND_SITES);
                    resultCopy.addAll(fqdns);
                } else {
                    UsageStatsMetricsReporter.reportMetricsEvent(
                            UsageStatsMetricsEvent.UNSUSPEND_SITES);
                    resultCopy.removeAll(fqdns);
                }

                mBridge.setSuspensions(
                        resultCopy.toArray(new String[resultCopy.size()]), (didSucceed) -> {
                            if (didSucceed) {
                                if (suspended) {
                                    result.addAll(fqdns);
                                } else {
                                    result.removeAll(fqdns);
                                }
                                mNotificationSuspender.setWebsitesSuspended(fqdns, suspended);
                                newWritePromise.fulfill(null);
                            } else {
                                newWritePromise.reject();
                            }
                        });
                // We need to add a dummy exception handler so that Promise doesn't complain when we
                // call variants of then() that don't take a single callback. These variants set an
                // exception handler on the returned promise, so they expect there to be one on the
                // root promise.
            }, (e) -> {});
        });

        mWritePromise = newWritePromise;
        return newWritePromise;
    }

    public Promise<List<String>> getAllSuspendedWebsites() {
        return mRootPromise.then(
                (Function<List<String>, List<String>>) (result) -> { return result; });
    }

    public boolean isWebsiteSuspended(String fqdn) {
        // We special case isWebsiteSuspended to return a value immediately because its only
        // consumer(PageViewOsberver) only cares about immediate results.
        if (mRootPromise != null && mRootPromise.isFulfilled()) {
            return mRootPromise.getResult().contains(fqdn);
        }

        return false;
    }
}