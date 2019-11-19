// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.google.android.libraries.feed.api.client.scope.ProcessScope;
import com.google.android.libraries.feed.api.client.scope.ProcessScopeBuilder;
import com.google.android.libraries.feed.api.host.config.ApplicationInfo;
import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.DebugBehavior;
import com.google.android.libraries.feed.api.host.network.NetworkClient;
import com.google.android.libraries.feed.api.host.storage.ContentStorageDirect;
import com.google.android.libraries.feed.api.host.storage.JournalStorageDirect;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.feed.tooltip.BasicTooltipSupportedApi;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.profiles.Profile;

/** Holds singleton {@link ProcessScope} and some of the scope's host implementations. */
public class FeedProcessScopeFactory {
    private static final String TAG = "FeedProcessScopeFtry";

    /**
     * Flag that tracks whether we've ever been disabled via enterprise policy. Should only be
     * accessed through isFeedProcessScopeEnabled().
     */
    private static boolean sEverDisabledForPolicy;

    /**
     * Tracks whether the article suggestions should be visible to the user during the current
     * session. If user opts in to the suggestions during the current session, the suggestions
     * services will be immediately warmed up. If user opts out during the current session,
     * the suggestions services will not shut down until the next session.
     */
    private static boolean sArticlesVisibleDuringSession;

    private static PrefChangeRegistrar sPrefChangeRegistrar;
    private static FeedAppLifecycle sFeedAppLifecycle;
    private static ProcessScope sProcessScope;
    private static FeedScheduler sFeedScheduler;
    private static FeedOfflineIndicator sFeedOfflineIndicator;
    private static NetworkClient sTestNetworkClient;
    private static FeedLoggingBridge sFeedLoggingBridge;

    /** @return The shared {@link ProcessScope} instance. Null if the Feed is disabled. */
    public static @Nullable ProcessScope getFeedProcessScope() {
        if (sProcessScope == null) {
            initialize();
        }
        return sProcessScope;
    }

    /**
     * @return The {@link FeedScheduler} that was given to the {@link ProcessScope}. Null if
     * the Feed is disabled.
     */
    public static @Nullable FeedScheduler getFeedScheduler() {
        if (sFeedScheduler == null) {
            initialize();
        }
        return sFeedScheduler;
    }

    /**
     * @return The {@link Runnable} to notify feed has been consumed.
     */
    public static Runnable getFeedConsumptionObserver() {
        Runnable consumptionObserver = () -> {
            FeedScheduler scheduler = getFeedScheduler();
            if (scheduler != null) {
                scheduler.onSuggestionConsumed();
            }
        };
        return consumptionObserver;
    }

    /**
     * @return The {@link FeedOfflineIndicator} that was given to the {@link ProcessScope}.
     * Null if the Feed is disabled.
     */
    public static @Nullable FeedOfflineIndicator getFeedOfflineIndicator() {
        if (sFeedOfflineIndicator == null) {
            initialize();
        }
        return sFeedOfflineIndicator;
    }

    /*
     * @return The global instance of {@link FeedAppLifecycle} for the process.
     *         Null if the Feed is disabled.
     */
    public static @Nullable FeedAppLifecycle getFeedAppLifecycle() {
        if (sFeedAppLifecycle == null) {
            initialize();
        }
        return sFeedAppLifecycle;
    }

    /** @return The {@link FeedLoggingBridge} that was given to the {@link FeedStreamScope}. Null if
     * the Feed is disabled. */
    public static @Nullable FeedLoggingBridge getFeedLoggingBridge() {
        if (sFeedLoggingBridge == null) {
            initialize();
        }
        return sFeedLoggingBridge;
    }

    /**
     * @return Whether the dependencies provided by this class are allowed to be created. The feed
     *         process is disabled if supervised user or enterprise policy has once been added
     *         within the current session.
     */
    public static boolean isFeedProcessEnabled() {
        // Once true, sEverDisabledForPolicy will be true forever. If it isn't true yet, we need to
        // check the pref every time. Two reasons for this. 1) We want to notice when we start in a
        // disabled state, shouldn't allow Feed to enabled until a restart. 2) A different
        // subscriber to  this pref change event might check in with this method, and we cannot
        // assume who will be called first. See https://crbug.com/896468.
        if (!sEverDisabledForPolicy) {
            sEverDisabledForPolicy =
                    !PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_SECTION_ENABLED);
        }
        return !sEverDisabledForPolicy;
    }

    private static void initialize() {
        assert sProcessScope == null && sFeedScheduler == null && sFeedOfflineIndicator == null
                && sFeedAppLifecycle == null && sFeedLoggingBridge == null;
        if (!isFeedProcessEnabled()) return;

        sArticlesVisibleDuringSession =
                PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE);
        sPrefChangeRegistrar = new PrefChangeRegistrar();
        sPrefChangeRegistrar.addObserver(Pref.NTP_ARTICLES_SECTION_ENABLED,
                FeedProcessScopeFactory::articlesEnabledPrefChange);

        Profile profile = Profile.getLastUsedProfile().getOriginalProfile();
        Configuration configHostApi = FeedConfiguration.createConfiguration();
        ApplicationInfo applicationInfo = FeedApplicationInfo.createApplicationInfo();

        FeedSchedulerBridge schedulerBridge = new FeedSchedulerBridge(profile);
        sFeedScheduler = schedulerBridge;
        ContentStorageDirect contentStorageDirect =
                new FeedContentStorageDirect(new FeedContentStorage(profile));
        JournalStorageDirect journalStorageDirect =
                new FeedJournalStorageDirect(new FeedJournalStorage(profile));
        NetworkClient networkClient = sTestNetworkClient == null ?
            new FeedNetworkBridge(profile) : sTestNetworkClient;
        sFeedLoggingBridge = new FeedLoggingBridge(profile);

        SequencedTaskRunner sequencedTaskRunner =
                PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE_MAY_BLOCK);

        sProcessScope = new ProcessScopeBuilder(configHostApi, sequencedTaskRunner::postTask,
                sFeedLoggingBridge, networkClient, schedulerBridge, DebugBehavior.SILENT,
                ContextUtils.getApplicationContext(), applicationInfo,
                new BasicTooltipSupportedApi())
                                .setContentStorageDirect(contentStorageDirect)
                                .setJournalStorageDirect(journalStorageDirect)
                                .build();
        schedulerBridge.initializeFeedDependencies(sProcessScope.getRequestManager());

        sFeedOfflineIndicator = new FeedOfflineBridge(profile, sProcessScope.getKnownContent());

        sFeedAppLifecycle = new FeedAppLifecycle(sProcessScope.getAppLifecycleListener(),
                new FeedLifecycleBridge(profile), sFeedScheduler);
    }

    /**
     * Creates a {@link ProcessScope} using the provided host implementations. Call {@link
     * #clearFeedProcessScopeForTesting()} to reset the ProcessScope after testing is complete.
     *
     * @param feedScheduler A {@link FeedScheduler} to use for testing.
     * @param networkClient A {@link NetworkClient} to use for testing.
     * @param feedOfflineIndicator A {@link FeedOfflineIndicator} to use for testing.
     * @param feedAppLifecycle A {@link FeedAppLifecycle} to use for testing.
     * @param loggingBridge A {@link FeedLoggingBridge} to use for testing.
     */
    @VisibleForTesting
    static void createFeedProcessScopeForTesting(FeedScheduler feedScheduler,
            NetworkClient networkClient, FeedOfflineIndicator feedOfflineIndicator,
            FeedAppLifecycle feedAppLifecycle, FeedLoggingBridge loggingBridge,
            ContentStorageDirect contentStorage, JournalStorageDirect journalStorage) {
        Configuration configHostApi = FeedConfiguration.createConfiguration();

        sFeedScheduler = feedScheduler;
        sFeedLoggingBridge = loggingBridge;
        sFeedOfflineIndicator = feedOfflineIndicator;
        sFeedAppLifecycle = feedAppLifecycle;
        ApplicationInfo applicationInfo =
                new ApplicationInfo.Builder(ContextUtils.getApplicationContext()).build();

        SequencedTaskRunner sequencedTaskRunner =
                PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE_MAY_BLOCK);

        sProcessScope = new ProcessScopeBuilder(configHostApi, sequencedTaskRunner::postTask,
                sFeedLoggingBridge, networkClient, sFeedScheduler, DebugBehavior.SILENT,
                ContextUtils.getApplicationContext(), applicationInfo,
                new BasicTooltipSupportedApi())
                                .setContentStorageDirect(contentStorage)
                                .setJournalStorageDirect(journalStorage)
                                .build();
    }

    /** Use supplied NetworkClient instead of real one, for tests. */
    @VisibleForTesting
    public static void setTestNetworkClient(NetworkClient client) {
        if (client == null) {
            sTestNetworkClient = null;
        } else if (sProcessScope == null) {
            sTestNetworkClient = client;
        } else {
            throw(new IllegalStateException(
                    "TestNetworkClient can not be set after ProcessScope has initialized."));
        }
    }

    /** Resets the ProcessScope after testing is complete. */
    @VisibleForTesting
    static void clearFeedProcessScopeForTesting() {
        destroy();
    }

    /**
     * @return Whether article suggestions are prepared to be shown based on user preference. If
     *         article suggestions are set hidden within a session, this will still return true
     *         until the next restart.
     */
    static boolean areArticlesVisibleDuringSession() {
        // Skip the native call if sArticlesVisibleDuringSession is already true to reduce overhead.
        if (!sArticlesVisibleDuringSession
                && PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE)) {
            sArticlesVisibleDuringSession = true;
        }

        return sArticlesVisibleDuringSession;
    }

    private static void articlesEnabledPrefChange() {
        // Cannot assume this is called because of an actual change. May be going from true to true.
        if (!PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_SECTION_ENABLED)) {
            // There have been quite a few crashes/bugs that happen when code does not correctly
            // handle the scenario where Feed suddenly becomes disabled and the above getters start
            // returning nulls. Having this log a warning helps diagnose this pattern from the
            // logcat.
            Log.w(TAG, "Disabling Feed because of policy.");
            sEverDisabledForPolicy = true;
            destroy();
        }
    }

    /** Clears out all static state. */
    private static void destroy() {
        if (sPrefChangeRegistrar != null) {
            sPrefChangeRegistrar.removeObserver(Pref.NTP_ARTICLES_SECTION_ENABLED);
            sPrefChangeRegistrar.destroy();
            sPrefChangeRegistrar = null;
        }
        if (sProcessScope != null) {
            sProcessScope.onDestroy();
            sProcessScope = null;
        }
        if (sFeedScheduler != null) {
            sFeedScheduler.destroy();
            sFeedScheduler = null;
        }
        if (sFeedOfflineIndicator != null) {
            sFeedOfflineIndicator.destroy();
            sFeedOfflineIndicator = null;
        }
        if (sFeedAppLifecycle != null) {
            sFeedAppLifecycle.destroy();
            sFeedAppLifecycle = null;
        }
        if (sFeedLoggingBridge != null) {
            sFeedLoggingBridge.destroy();
            sFeedLoggingBridge = null;
        }
    }
}
