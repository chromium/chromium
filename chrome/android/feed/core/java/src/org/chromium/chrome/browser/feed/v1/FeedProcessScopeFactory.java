// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.feed.library.api.client.scope.ProcessScope;
import org.chromium.chrome.browser.feed.library.api.client.scope.ProcessScopeBuilder;
import org.chromium.chrome.browser.feed.library.api.host.config.ApplicationInfo;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.api.host.network.NetworkClient;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorageDirect;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.common.time.SystemClockImpl;
import org.chromium.chrome.browser.feed.shared.FeedFeatures;
import org.chromium.chrome.browser.feed.v1.tooltip.BasicTooltipSupportedApi;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Holds singleton {@link ProcessScope} and some of the scope's host implementations. */
public class FeedProcessScopeFactory {
    private static final String TAG = "FeedProcessScopeFtry";

    /**
     * Tracks whether the article suggestions should be visible to the user during the current
     * session. If user opts in to the suggestions during the current session, the suggestions
     * services will be immediately warmed up. If user opts out during the current session,
     * the suggestions services will not shut down until the next session.
     */
    private static boolean sArticlesVisibleDuringSession;

    private static FeedAppLifecycle sFeedAppLifecycle;
    private static ProcessScope sProcessScope;
    private static FeedScheduler sFeedScheduler;
    private static FeedOfflineIndicator sFeedOfflineIndicator;
    private static NetworkClient sTestNetworkClient;
    private static ContentStorageDirect sTestContentStorageDirect;
    private static JournalStorageDirect sTestJournalStorageDirect;
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

    /**
     * @return The {@link FeedLoggingBridge} that was given to the {@link FeedStreamScope}. Null if
     * the Feed is disabled.
     */
    public static @Nullable FeedLoggingBridge getFeedLoggingBridge() {
        if (sFeedLoggingBridge == null) {
            initialize();
        }
        return sFeedLoggingBridge;
    }

    private static void initialize() {
        assert !FeedFeatures.isV2Enabled();
        assert sProcessScope == null && sFeedScheduler == null && sFeedOfflineIndicator == null
                && sFeedAppLifecycle == null && sFeedLoggingBridge == null;
        if (!FeedFeatures.isFeedEnabled()) return;

        sArticlesVisibleDuringSession = getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE);

        Profile profile = Profile.getLastUsedRegularProfile();
        Configuration configHostApi = FeedConfiguration.createConfiguration();
        ApplicationInfo applicationInfo = FeedApplicationInfo.createApplicationInfo();

        FeedSchedulerBridge schedulerBridge = new FeedSchedulerBridge(profile);
        sFeedScheduler = schedulerBridge;
        ContentStorageDirect contentStorageDirect = sTestContentStorageDirect == null
                ? new FeedContentStorageDirect(new FeedContentStorage(profile))
                : sTestContentStorageDirect;
        JournalStorageDirect journalStorageDirect = sTestJournalStorageDirect == null
                ? new FeedJournalStorageDirect(new FeedJournalStorage(profile))
                : sTestJournalStorageDirect;
        NetworkClient networkClient =
                sTestNetworkClient == null ? new FeedNetworkBridge(profile) : sTestNetworkClient;

        sFeedLoggingBridge = new FeedLoggingBridge(profile, new SystemClockImpl());

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

    /** Use supplied ContentStorageDirect instead of real one, for tests. */
    @VisibleForTesting
    public static void setTestContentStorageDirect(ContentStorageDirect storage) {
        if (storage == null) {
            sTestContentStorageDirect = null;
        } else if (sProcessScope == null) {
            sTestContentStorageDirect = storage;
        } else {
            throw new IllegalStateException(
                    "TestContentStorageDirect can not be set after ProcessScope has initialized.");
        }
    }

    /** Use supplied JournalStorageDirect instead of real one, for tests. */
    @VisibleForTesting
    public static void setTestJournalStorageDirect(JournalStorageDirect storage) {
        if (storage == null) {
            sTestJournalStorageDirect = null;
        } else if (sProcessScope == null) {
            sTestJournalStorageDirect = storage;
        } else {
            throw new IllegalStateException(
                    "TestJournalStorageDirect can not be set after ProcessScope has initialized.");
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
                && getPrefService().getBoolean(Pref.ARTICLES_LIST_VISIBLE)) {
            sArticlesVisibleDuringSession = true;
        }

        return sArticlesVisibleDuringSession;
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    /** Clears out all static state. */
    public static void destroy() {
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
