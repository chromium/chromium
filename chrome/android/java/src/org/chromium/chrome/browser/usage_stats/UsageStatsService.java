// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.app.Activity;
import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CollectionUtil;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/**
 * Public interface for all usage stats related functionality. All calls to instances of
 * UsageStatsService must be made on the UI thread.
 */
public class UsageStatsService implements Destroyable {
    private static final String TAG = "UsageStatsService";

    private static ProfileKeyedMap<UsageStatsService> sProfileMap =
            ProfileKeyedMap.createMapOfDestroyables(
                    ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);

    private Profile mProfile;
    private EventTracker mEventTracker;
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
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
    }

    /** Return the {@link UsageStatsService} for the given {@link Profile}. */
    public static UsageStatsService getForProfile(Profile profile) {
        assert isEnabled();
        return sProfileMap.getForProfile(profile, UsageStatsService::new);
    }

    /**
     * Creates a UsageStatsService for the given Activity if the feature is enabled.
     *
     * @param activity The activity in which page view events are occurring.
     * @param profile The {@link Profile} associated with the activity.
     * @param activityTabProvider The provider of the active tab for the activity.
     * @param tabContentManagerSupplier Supplier of the current {@link TabContentManager}.
     */
    public static void createPageViewObserverIfEnabled(
            Activity activity,
            Profile profile,
            ActivityTabProvider activityTabProvider,
            Supplier<TabContentManager> tabContentManagerSupplier) {
        if (!isEnabled()) return;

        getForProfile(profile)
                .createPageViewObserver(activity, activityTabProvider, tabContentManagerSupplier);
    }

    @VisibleForTesting
    UsageStatsService(Profile profile) {
        mProfile = profile;
        mBridge = new UsageStatsBridge(mProfile, this);
        mEventTracker = new EventTracker(mBridge);
        mSuspensionTracker = new SuspensionTracker(mBridge, mProfile);
        mTokenTracker = new TokenTracker(mBridge);
        mPageViewObservers = new ArrayList<>();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            mClient = ServiceLoaderUtil.maybeCreate(DigitalWellbeingClient.class);
        }
        if (mClient == null) {
            mClient = new DigitalWellbeingClient();
        }

        mSuspensionTracker
                .getAllSuspendedWebsites()
                .then(
                        (suspendedSites) -> {
                            notifyObserversOfSuspensions(suspendedSites, true);
                        });

        mOptInState = getOptInState();
    }

    @Override
    public void destroy() {
        mBridge.destroy();
    }

    public SuspensionTracker getSuspensionTracker() {
        return mSuspensionTracker;
    }

    /**
     * Create a {@link PageViewObserver} for the given tab model selector and activity.
     * @param activity The activity in which page view events are occurring.
     * @param activityTabProvider The provider of the active tab for the activity.
     * @param tabContentManagerSupplier Supplier of the current {@link TabContentManager}.
     */
    private PageViewObserver createPageViewObserver(
            Activity activity,
            ActivityTabProvider activityTabProvider,
            Supplier<TabContentManager> tabContentManagerSupplier) {
        ThreadUtils.assertOnUiThread();
        PageViewObserver observer =
                new PageViewObserver(
                        activity,
                        activityTabProvider,
                        mEventTracker,
                        mTokenTracker,
                        mSuspensionTracker,
                        tabContentManagerSupplier);
        mPageViewObservers.add(new WeakReference<>(observer));
        return observer;
    }

    /** @return Whether the user has authorized DW to access usage stats data. */
    boolean getOptInState() {
        ThreadUtils.assertOnUiThread();
        return UserPrefs.get(mProfile).getBoolean(Pref.USAGE_STATS_ENABLED);
    }

    /** Sets the user's opt in state. */
    void setOptInState(boolean state) {
        ThreadUtils.assertOnUiThread();
        UserPrefs.get(mProfile).setBoolean(Pref.USAGE_STATS_ENABLED, state);

        if (mOptInState == state) return;
        mOptInState = state;
        mClient.notifyOptInStateChange(mOptInState);

        if (!state) {
            getAllSuspendedWebsitesAsync()
                    .then(
                            (suspendedSites) -> {
                                setWebsitesSuspendedAsync(suspendedSites, false);
                            });
            getAllTrackedTokensAsync()
                    .then(
                            (tokens) -> {
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

    /** Suspend or unsuspend every site in FQDNs, depending on the value of {@code suspended}. */
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
        mEventTracker
                .clearAll()
                .except(
                        (exception) -> {
                            // Retry once; if the subsequent attempt fails, log the failure and move
                            // on.
                            mEventTracker
                                    .clearAll()
                                    .except(
                                            (exceptionInner) -> {
                                                Log.e(
                                                        TAG,
                                                        "Failed to clear all events for history"
                                                                + " deletion");
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
        mEventTracker
                .clearRange(startTimeMs, effectiveEndTimeMs)
                .except(
                        (exception) -> {
                            // Retry once; if the subsequent attempt fails, log the failure and move
                            // on.
                            mEventTracker
                                    .clearRange(startTimeMs, endTimeMs)
                                    .except(
                                            (exceptionInner) -> {
                                                Log.e(
                                                        TAG,
                                                        "Failed to clear range of events for"
                                                                + " history deletion");
                                            });
                        });
    }

    public void onHistoryDeletedForDomains(List<String> fqdns) {
        ThreadUtils.assertOnUiThread();
        UsageStatsMetricsReporter.reportMetricsEvent(UsageStatsMetricsEvent.CLEAR_HISTORY_DOMAIN);
        mClient.notifyHistoryDeletion(fqdns);
        mEventTracker
                .clearDomains(fqdns)
                .except(
                        (exception) -> {
                            // Retry once; if the subsequent attempt fails, log the failure and move
                            // on.
                            mEventTracker
                                    .clearDomains(fqdns)
                                    .except(
                                            (exceptionInner) -> {
                                                Log.e(
                                                        TAG,
                                                        "Failed to clear domain events for history"
                                                                + " deletion");
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

    public void stopTrackingToken(String token) {}

    public void setWebsitesSuspended(List<String> fqdns, boolean suspended) {}

    public List<String> getAllSuspendedWebsites() {
        return new ArrayList<>();
    }

    private void notifyObserversOfSuspensions(List<String> fqdns, boolean suspended) {
        for (PageViewObserver observer : CollectionUtil.strengthen(mPageViewObservers)) {
            for (String fqdn : fqdns) {
                observer.notifySiteSuspensionChanged(fqdn, suspended);
            }
        }
    }
}
