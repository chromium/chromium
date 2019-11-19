// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/**
 * Public interface for all usage stats related functionality. All calls to instances of
 * UsageStatsService must be made on the UI thread.
 */
public class UsageStatsService {
    private static final String TAG = "UsageStatsService";

    private static UsageStatsService sInstance;

    private EventTracker mEventTracker;
    private NotificationSuspender mNotificationSuspender;
    private SuspensionTracker mSuspensionTracker;
    private TokenTracker mTokenTracker;
    private UsageStatsBridge mBridge;
    // PageViewObservers are scoped to a given ChromeTabbedActivity, but UsageStatsService isn't. To
    // allow for GC of the observer to happen when the activity goes away, we only hold weak
    // references here.
    private List<WeakReference<PageViewObserver>> mPageViewObservers;

    private DigitalWellbeingClient mClient;
    private boolean mOptInState;

    /** Returns if the UsageStatsService is enabled on this device */
    public static boolean isEnabled() {
        return BuildInfo.isAtLeastQ() && ChromeFeatureList.isEnabled(ChromeFeatureList.USAGE_STATS);
    }

    /** Get the global instance of UsageStatsService */
    public static UsageStatsService getInstance() {
        assert isEnabled();
        if (sInstance == null) {
            sInstance = new UsageStatsService();
        }

        return sInstance;
    }

    @VisibleForTesting
    UsageStatsService() {
        Profile profile = Profile.getLastUsedProfile().getOriginalProfile();
        mBridge = new UsageStatsBridge(profile, this);
        mEventTracker = new EventTracker(mBridge);
        mNotificationSuspender = new NotificationSuspender(profile);
        mSuspensionTracker = new SuspensionTracker(mBridge, mNotificationSuspender);
        mTokenTracker = new TokenTracker(mBridge);
        mPageViewObservers = new ArrayList<>();
        mClient = AppHooks.get().createDigitalWellbeingClient();

        mSuspensionTracker.getAllSuspendedWebsites().then(
                (suspendedSites) -> { notifyObserversOfSuspensions(suspendedSites, true); });

        mOptInState = getOptInState();
    }

    /* package */ NotificationSuspender getNotificationSuspender() {
        return mNotificationSuspender;
    }

    /**
     * Create a {@link PageViewObserver} for the given tab model selector and activity.
     * @param tabModelSelector The tab model selector that should be used to get the current tab
     *         model.
     * @param activity The activity in which page view events are occuring.
     */
    public PageViewObserver createPageViewObserver(
            TabModelSelector tabModelSelector, Activity activity) {
        ThreadUtils.assertOnUiThread();
        PageViewObserver observer = new PageViewObserver(
                activity, tabModelSelector, mEventTracker, mTokenTracker, mSuspensionTracker);
        mPageViewObservers.add(new WeakReference<>(observer));
        return observer;
    }

    /** @return Whether the user has authorized DW to access usage stats data. */
    public boolean getOptInState() {
        ThreadUtils.assertOnUiThread();
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();
        boolean enabledByPref = prefServiceBridge.getBoolean(Pref.USAGE_STATS_ENABLED);
        boolean enabledByFeature = ChromeFeatureList.isEnabled(ChromeFeatureList.USAGE_STATS);
        // If the user has previously opted in, but the feature has been turned off, we need to
        // treat it as if they opted out; otherwise they'll have no UI affordance for clearing
        // whatever data Digital Wellbeing has stored.
        if (enabledByPref && !enabledByFeature) {
            onAllHistoryDeleted();
            setOptInState(false);
        }

        return enabledByPref && enabledByFeature;
    }

    /** Sets the user's opt in state. */
    public void setOptInState(boolean state) {
        ThreadUtils.assertOnUiThread();
        PrefServiceBridge prefServiceBridge = PrefServiceBridge.getInstance();
        prefServiceBridge.setBoolean(Pref.USAGE_STATS_ENABLED, state);

        if (mOptInState == state) return;
        mOptInState = state;
        mClient.notifyOptInStateChange(mOptInState);

        if (!state) {
            getAllSuspendedWebsitesAsync().then(
                    (suspendedSites) -> { setWebsitesSuspendedAsync(suspendedSites, false); });
            getAllTrackedTokensAsync().then((tokens) -> {
                for (String token : tokens) stopTrackingTokenAsync(token);
            });
        }

        @UsageStatsMetricsEvent
        int event = state ? UsageStatsMetricsEvent.OPT_IN : UsageStatsMetricsEvent.OPT_OUT;
        UsageStatsMetricsReporter.reportMetricsEvent(event);
    }

    /** Query for all events that occurred in the half-open range [start, end) */
    public Promise<List<WebsiteEvent>> queryWebsiteEventsAsync(long start, long end) {
        ThreadUtils.assertOnUiThread();
        return mEventTracker.queryWebsiteEvents(start, end);
    }

    /** Get all tokens that are currently being tracked. */
    public Promise<List<String>> getAllTrackedTokensAsync() {
        ThreadUtils.assertOnUiThread();
        return mTokenTracker.getAllTrackedTokens();
    }

    /**
     * Start tracking a full-qualified domain name(FQDN), returning the token used to identify it.
     * If the FQDN is already tracked, this will return the existing token.
     */
    public Promise<String> startTrackingWebsiteAsync(String fqdn) {
        ThreadUtils.assertOnUiThread();
        return mTokenTracker.startTrackingWebsite(fqdn);
    }

    /**
     * Stops tracking the site associated with the given token.
     * If the token was not associated with a site, this does nothing.
     */
    public Promise<Void> stopTrackingTokenAsync(String token) {
        ThreadUtils.assertOnUiThread();
        return mTokenTracker.stopTrackingToken(token);
    }

    /**
     * Suspend or unsuspend every site in FQDNs, depending on the value of {@code suspended}.
     */
    public Promise<Void> setWebsitesSuspendedAsync(List<String> fqdns, boolean suspended) {
        ThreadUtils.assertOnUiThread();
        notifyObserversOfSuspensions(fqdns, suspended);

        return mSuspensionTracker.setWebsitesSuspended(fqdns, suspended);
    }

    /** @return all the sites that are currently suspended. */
    public Promise<List<String>> getAllSuspendedWebsitesAsync() {
        ThreadUtils.assertOnUiThread();
        return mSuspensionTracker.getAllSuspendedWebsites();
    }

    public void onAllHistoryDeleted() {
        ThreadUtils.assertOnUiThread();
        UsageStatsMetricsReporter.reportMetricsEvent(UsageStatsMetricsEvent.CLEAR_ALL_HISTORY);
        mClient.notifyAllHistoryCleared();
        mEventTracker.clearAll().except((exception) -> {
            // Retry once; if the subsequent attempt fails, log the failure and move on.
            mEventTracker.clearAll().except((exceptionInner) -> {
                Log.e(TAG, "Failed to clear all events for history deletion");
            });
        });
    }

    public void onHistoryDeletedInRange(long startTimeMs, long endTimeMs) {
        ThreadUtils.assertOnUiThread();
        UsageStatsMetricsReporter.reportMetricsEvent(UsageStatsMetricsEvent.CLEAR_HISTORY_RANGE);
        // endTimeMs could be Long.MAX_VALUE, which doesn't play well when converting into a
        // Timestamp proto. It doesn't make any sense to delete into the future, so we can
        // reasonably cap endTimeMs at now.
        long effectiveEndTimeMs = Math.min(endTimeMs, System.currentTimeMillis());
        mClient.notifyHistoryDeletion(startTimeMs, effectiveEndTimeMs);
        mEventTracker.clearRange(startTimeMs, effectiveEndTimeMs).except((exception) -> {
            // Retry once; if the subsequent attempt fails, log the failure and move on.
            mEventTracker.clearRange(startTimeMs, endTimeMs).except((exceptionInner) -> {
                Log.e(TAG, "Failed to clear range of events for history deletion");
            });
        });
    }

    public void onHistoryDeletedForDomains(List<String> fqdns) {
        ThreadUtils.assertOnUiThread();
        UsageStatsMetricsReporter.reportMetricsEvent(UsageStatsMetricsEvent.CLEAR_HISTORY_DOMAIN);
        mClient.notifyHistoryDeletion(fqdns);
        mEventTracker.clearDomains(fqdns).except((exception) -> {
            // Retry once; if the subsequent attempt fails, log the failure and move on.
            mEventTracker.clearDomains(fqdns).except((exceptionInner) -> {
                Log.e(TAG, "Failed to clear domain events for history deletion");
            });
        });
    }

    // The below methods are dummies that are only being retained to avoid breaking the downstream
    // build. TODO(pnoland): remove these once the downstream change that converts to using promises
    // lands.
    public List<WebsiteEvent> queryWebsiteEvents(long start, long end) {
        return new ArrayList<>();
    }

    public List<String> getAllTrackedTokens() {
        return new ArrayList<>();
    }

    public String startTrackingWebsite(String fqdn) {
        return "1";
    }

    public void stopTrackingToken(String token) {
        return;
    }

    public void setWebsitesSuspended(List<String> fqdns, boolean suspended) {
        return;
    }

    public List<String> getAllSuspendedWebsites() {
        return new ArrayList<>();
    }

    private void notifyObserversOfSuspensions(List<String> fqdns, boolean suspended) {
        for (WeakReference<PageViewObserver> observerRef : mPageViewObservers) {
            PageViewObserver observer = observerRef.get();
            if (observer != null) {
                for (String fqdn : fqdns) {
                    observer.notifySiteSuspensionChanged(fqdn, suspended);
                }
            }
        }
    }
}