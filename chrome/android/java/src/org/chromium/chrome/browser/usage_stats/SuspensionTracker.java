// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import org.chromium.base.CallbackUtils;
import org.chromium.base.Promise;
import org.chromium.chrome.browser.notifications.NotificationSuspender;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.function.Function;

/** Class that tracks which sites are currently suspended. */
public class SuspensionTracker {
    private final UsageStatsBridge mBridge;
    private final NotificationSuspender mNotificationSuspender;
    private final Promise<List<String>> mRootPromise;
    private Promise<Void> mWritePromise;

    public SuspensionTracker(UsageStatsBridge bridge, Profile profile) {
        mBridge = bridge;
        mNotificationSuspender = new NotificationSuspender(profile);
        mRootPromise = new Promise<>();
        mBridge.getAllSuspensions(
                (result) -> {
                    mRootPromise.fulfill(result);
                });
        mWritePromise = Promise.fulfilled(null);
    }

    /**
     * Sets the status of <c>fqdns</c> to match <c>suspended</c>.
     * The returned promise will be fulfilled once persistence succeeds, and rejected if persistence
     * fails.
     */
    public Promise<Void> setWebsitesSuspended(List<String> fqdns, boolean suspended) {
        Promise<Void> newWritePromise = new Promise<>();
        mWritePromise.then(
                (placeholderResult) -> {
                    mRootPromise.then(
                            (result) -> {
                                // We copy result so that the mutation isn't reflected in result
                                // until persistence succeeds.
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
                                        resultCopy.toArray(new String[resultCopy.size()]),
                                        (didSucceed) -> {
                                            if (didSucceed) {
                                                if (suspended) {
                                                    result.addAll(fqdns);
                                                    mNotificationSuspender
                                                            .suspendNotificationsFromDomains(fqdns);
                                                } else {
                                                    result.removeAll(fqdns);
                                                    mNotificationSuspender
                                                            .unsuspendNotificationsFromDomains(
                                                                    fqdns);
                                                }
                                                newWritePromise.fulfill(null);
                                            } else {
                                                newWritePromise.reject();
                                            }
                                        });
                                // We need to add a placeholder exception handler so that Promise
                                // doesn't complain when we call variants of then() that don't
                                // take a single callback. These variants set an exception handler
                                // on the returned promise, so they expect
                                // there to be one on the root promise.
                            },
                            CallbackUtils.emptyCallback());
                });

        mWritePromise = newWritePromise;
        return newWritePromise;
    }

    /**
     * Stores the notification's resources if the notification originates from a suspended domain,
     * so that in that case it will be shown later.
     *
     * @param notification The notification whose resources to store for later display.
     * @return A {@link Promise} that resolves to whether the domain is suspended and thus the
     *     resources were stored.
     */
    public Promise<Boolean> storeNotificationResourcesIfSuspended(
            NotificationWrapper notification) {
        return getAllSuspendedWebsites()
                .then(
                        (List<String> fqdns) -> {
                            if (!fqdns.contains(
                                    NotificationSuspender.getValidFqdnOrEmptyString(
                                            notification))) {
                                return false;
                            }
                            mNotificationSuspender.storeNotificationResources(
                                    Collections.singletonList(notification));
                            return true;
                        });
    }

    public Promise<List<String>> getAllSuspendedWebsites() {
        return mRootPromise.then(
                (Function<List<String>, List<String>>)
                        (result) -> {
                            return result;
                        });
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
