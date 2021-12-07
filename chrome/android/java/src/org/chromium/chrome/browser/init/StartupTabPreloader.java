// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Intent;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.metrics.ActivityTabStartupMetricsTracker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class attempts to preload the tab if the url is known from the intent when the profile
 * is created. This is done to improve startup latency.
 */
public class StartupTabPreloader implements ProfileManager.Observer, DestroyObserver,
                                            ActivityTabStartupMetricsTracker.Observer {
    public static final String EXTRA_DISABLE_STARTUP_TAB_PRELOADER =
            "org.chromium.chrome.browser.init.DISABLE_STARTUP_TAB_PRELOADER";
    private static boolean sFailNextTabMatchForTesting;

    // These values are persisted in histograms. Please do not renumber. Append only.
    @VisibleForTesting
    @IntDef({LoadDecisionReason.DISABLED_BY_INTENT, LoadDecisionReason.INCOGNITO,
            LoadDecisionReason.INTENT_IGNORED, LoadDecisionReason.NO_URL,
            LoadDecisionReason.NO_TAB_CREATOR, LoadDecisionReason.WRONG_TAB_CREATOR,
            LoadDecisionReason.DISABLED_BY_FEATURE, LoadDecisionReason.ALL_SATISFIED})
    @Retention(RetentionPolicy.SOURCE)
    @interface LoadDecisionReason {
        int DISABLED_BY_INTENT = 0;
        int INCOGNITO = 1;
        int INTENT_IGNORED = 2;
        int NO_URL = 3;
        int NO_TAB_CREATOR = 4;
        int WRONG_TAB_CREATOR = 5;
        int DISABLED_BY_FEATURE = 6;
        int ALL_SATISFIED = 7;

        int NUM_ENTRIES = 8;
    }

    private final Supplier<Intent> mIntentSupplier;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final WindowAndroid mWindowAndroid;
    private final TabCreatorManager mTabCreatorManager;
    private final IntentHandler mIntentHandler;
    private LoadUrlParams mLoadUrlParams;
    private Tab mTab;
    private StartupTabObserver mObserver;
    private ActivityTabStartupMetricsTracker mStartupMetricsTracker;

    // The time at which the tab preload decision was made. Recorded only for non-incognito
    // startups.
    private long mLoadDecisionMs;
    // Records whether a preload was triggered.
    boolean mTriggerPreload;
    // Records the reason for the last preload decision.
    @LoadDecisionReason
    int mLoadDecisionReason;
    // Records whether a preloaded tab matched.
    boolean mTabMatches;
    // Records whether we have already recorded the histogram for the duration between the load
    // decision and the match decision; this histogram should be recorded only on the first match
    // decision.
    private boolean mRecordedLoadDecisionToMatchDecisionHistogram;

    // Whether a tab preload was prevented only by the ElideTabPreloadAtStartup feature.
    private boolean mPreloadPreventedOnlyByFeature;
    // The params that would have been used for the preload if not prevented by the feature.
    // NOTE: We explicitly track only the params that are necessary for comparison of tab matching
    // to avoid calling IntentHandler#createLoadUrlParamsForIntent(). The latter is undesirable
    // because it is destructive to the intent metadata, which is problematic in the case where the
    // LoadUrlParams are not intended for usage (as they're not here).
    private String mUrlForPreloadPreventedOnlyByFeature;
    private String mReferrerForPreloadPreventedOnlyByFeature;
    // Whether the tab preload that was prevented only by the feature would have matched.
    private boolean mPreloadPreventedOnlyByFeatureWouldHaveMatched;

    // The time at which the first navigation start occurred.
    private long mFirstNavigationStartMs;

    public static void failNextTabMatchForTesting() {
        sFailNextTabMatchForTesting = true;
    }

    public StartupTabPreloader(Supplier<Intent> intentSupplier,
            ActivityLifecycleDispatcher activityLifecycleDispatcher, WindowAndroid windowAndroid,
            TabCreatorManager tabCreatorManager, IntentHandler intentHandler,
            ActivityTabStartupMetricsTracker startupMetricsTracker) {
        mIntentSupplier = intentSupplier;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mWindowAndroid = windowAndroid;
        mTabCreatorManager = tabCreatorManager;
        mIntentHandler = intentHandler;
        mStartupMetricsTracker = startupMetricsTracker;

        mActivityLifecycleDispatcher.register(this);
        ProfileManager.addObserver(this);
        ActivityTabStartupMetricsTracker.addObserver(this);
    }

    // Returns true if a startup tab preload either (a) was triggered or (b) was prevented
    // from triggering only by the ElideTabPreloadAtStartup Feature.
    private boolean preloadWasViable() {
        return mTriggerPreload || mPreloadPreventedOnlyByFeature;
    }

    // Returns true if a match of a preloaded tab either (a) occurred or (b) was prevented from
    // occurring only by the ElideTabPreloadAtStartup Feature.
    private boolean tabMatchWasViable() {
        return mTabMatches || mPreloadPreventedOnlyByFeatureWouldHaveMatched;
    }

    @Override
    public void onDestroy() {
        if (mTab != null) mTab.destroy();
        mTab = null;

        ProfileManager.removeObserver(this);
        ActivityTabStartupMetricsTracker.removeObserver(this);
        mActivityLifecycleDispatcher.unregister(this);
    }

    @Override
    public void onFirstNavigationStart() {
        mFirstNavigationStartMs = SystemClock.uptimeMillis();
    }

    @Override
    public void onFirstVisibleContent() {
        recordDurationFromLoadDecisionIntoPostTabMatchHistogram(
                "Android.StartupTabPreloader.LoadDecisionToFirstVisibleContent");
    }

    @Override
    public void onFirstNavigationCommit() {
        recordDurationFromLoadDecisionIntoPostTabMatchHistogram(
                "Android.StartupTabPreloader.LoadDecisionToFirstNavigationCommit");

        // We record the metric for navigation start here as well, as we want that metric to be
        // recorded only for navigations that result in the first navigation commit startup metric
        // being recorded.
        assert mFirstNavigationStartMs > 0;
        recordDurationFromLoadDecisionToEventTimeIntoPreTabMatchHistogram(
                "Android.StartupTabPreloader.LoadDecisionToFirstNavigationStart",
                mFirstNavigationStartMs);
    }

    @Override
    public void onFirstContentfulPaint() {
        recordDurationFromLoadDecisionIntoPostTabMatchHistogram(
                "Android.StartupTabPreloader.LoadDecisionToFirstContentfulPaint");
    }

    // Records the duration from the load decision to the current time into |histogram| (suffixed
    // by the state of the tab preload decision). To be used when the current time is before the
    // tab match decision has occurred.
    private void recordDurationFromLoadDecisionIntoPreTabMatchHistogram(String histogram) {
        recordDurationFromLoadDecisionToEventTimeIntoPreTabMatchHistogram(
                histogram, SystemClock.uptimeMillis());
    }

    // Records the duration from the load decision to |eventTimeMs| into |histogram| (suffixed
    // by the state of the tab preload decision). To be used when the corresponding event occurred
    // before the tab match decision has occurred.
    private void recordDurationFromLoadDecisionToEventTimeIntoPreTabMatchHistogram(
            String histogram, long eventTimeMs) {
        if (mLoadDecisionMs == 0 || eventTimeMs == 0) return;

        long triggerpointToEventTimeMs = eventTimeMs - mLoadDecisionMs;

        String suffix = preloadWasViable() ? ".Load" : ".NoLoad";
        RecordHistogram.recordMediumTimesHistogram(histogram + suffix, triggerpointToEventTimeMs);
    }

    // Records the duration from the load decision to the current time into |histogram| (suffixed
    // by the state of the tab preload and match decisions). To be used when the tab match decision
    // may have already occurred at the present time.
    private void recordDurationFromLoadDecisionIntoPostTabMatchHistogram(String histogram) {
        if (mLoadDecisionMs == 0) return;

        long currentTimeMs = SystemClock.uptimeMillis();
        long triggerpointToCurrentTimeMs = currentTimeMs - mLoadDecisionMs;

        String suffix = ".NoLoad";
        if (preloadWasViable()) {
            // Check whether the tab match decision has yet occurred. It is still pending if (1) the
            // preloaded tab is non-null, or (2) in the case where preloading is disabled by
            // feature, the stored state used to calculate whether the preload would have matched is
            // non-null.
            boolean tabMatchStillPending =
                    mTab != null || mUrlForPreloadPreventedOnlyByFeature != null;
            if (tabMatchStillPending) {
                suffix = ".LoadPreMatch";
            } else if (tabMatchWasViable()) {
                suffix = ".LoadAndMatch";
            } else {
                suffix = ".LoadAndMismatch";
            }
        }

        RecordHistogram.recordMediumTimesHistogram(histogram + suffix, triggerpointToCurrentTimeMs);
    }

    // Returns whether the state specified for the tab preload and the actual load match.
    private boolean doesPreloadStateMatch(@TabLaunchType int preconnectLaunchType,
            @TabLaunchType int loadLaunchType, LoadUrlParams preconnectParams,
            LoadUrlParams loadParams) {
        boolean tabMatch = preconnectLaunchType == loadLaunchType
                && doLoadUrlParamsMatchForWarmupManagerNavigation(preconnectParams, loadParams)
                && !sFailNextTabMatchForTesting;
        sFailNextTabMatchForTesting = false;

        return tabMatch;
    }

    /**
     * Returns the Tab if loadUrlParams and type match, otherwise the Tab is discarded.
     *
     * @param loadUrlParams The actual parameters of the url load.  @param type The actual launch
     * type type.  @return The results of maybeNavigate() if they match loadUrlParams and type or
     * null otherwise.
     */
    public Tab takeTabIfMatchingOrDestroy(LoadUrlParams loadUrlParams, @TabLaunchType int type) {
        if (!mRecordedLoadDecisionToMatchDecisionHistogram
                && mStartupMetricsTracker.isTrackingStartupMetrics()) {
            // NOTE: This histogram is segmented only by state of the load decision as it covers the
            // duration from the load decision *up to* the tab match decision. Additionally, note
            // that we record the metric only when tracking startup metrics to ensure that the
            // latencies of this metric are comparable to those ones.
            recordDurationFromLoadDecisionIntoPreTabMatchHistogram(
                    "Android.StartupTabPreloader.LoadDecisionToMatchDecision");
            mRecordedLoadDecisionToMatchDecisionHistogram = true;

            // Similarly, we record this metric only now to avoid recording cases where startup
            // metrics are tracked at the time of the load decision but have been cancelled by the
            // time of the tab match decision (e.g., due to a decision being made to show an
            // overview).
            RecordHistogram.recordMediumTimesHistogram(
                    "Android.StartupTabPreloader.ActivityStartToLoadDecision",
                    mLoadDecisionMs - mStartupMetricsTracker.getActivityStartTimeMs());
        }

        if (mTab == null) {
            if (mUrlForPreloadPreventedOnlyByFeature != null) {
                // Construct a LoadUrlParams object for the preload that would have occurred.
                LoadUrlParams loadUrlParamsForPreloadPreventedOnlyByFeature =
                        new LoadUrlParams(mUrlForPreloadPreventedOnlyByFeature);
                if (mReferrerForPreloadPreventedOnlyByFeature != null) {
                    loadUrlParamsForPreloadPreventedOnlyByFeature.setReferrer(new Referrer(
                            mReferrerForPreloadPreventedOnlyByFeature, ReferrerPolicy.DEFAULT));
                }

                // Calculate whether a tab match *would have* occurred if the preload wasn't
                // prevented by the feature. This is used later for metrics tracking.
                mPreloadPreventedOnlyByFeatureWouldHaveMatched =
                        doesPreloadStateMatch(TabLaunchType.FROM_EXTERNAL_APP, type,
                                loadUrlParamsForPreloadPreventedOnlyByFeature, loadUrlParams);
                mUrlForPreloadPreventedOnlyByFeature = null;
                mReferrerForPreloadPreventedOnlyByFeature = null;
            }

            return null;
        }

        mTabMatches =
                doesPreloadStateMatch(mTab.getLaunchType(), type, mLoadUrlParams, loadUrlParams);

        RecordHistogram.recordBooleanHistogram(
                "Startup.Android.StartupTabPreloader.TabTaken", mTabMatches);

        if (!mTabMatches) {
            mStartupMetricsTracker.onStartupTabPreloadDropped();

            mTab.destroy();
            mTab = null;
            mLoadUrlParams = null;
            return null;
        }

        Tab tab = mTab;
        mTab = null;
        mLoadUrlParams = null;
        tab.removeObserver(mObserver);
        return tab;
    }

    @VisibleForTesting
    static boolean doLoadUrlParamsMatchForWarmupManagerNavigation(
            LoadUrlParams preconnectParams, LoadUrlParams loadParams) {
        if (!TextUtils.equals(preconnectParams.getUrl(), loadParams.getUrl())) return false;

        String preconnectReferrer = preconnectParams.getReferrer() != null
                ? preconnectParams.getReferrer().getUrl()
                : null;
        String loadParamsReferrer =
                loadParams.getReferrer() != null ? loadParams.getReferrer().getUrl() : null;

        return TextUtils.equals(preconnectReferrer, loadParamsReferrer);
    }

    /**
     * Called by the ProfileManager when a profile has been created. This occurs during startup
     * and it's the earliest point at which we can create and load a tab. If the url can be
     * determined from the intent, then a tab will be loaded and potentially adopted by
     * {@link ChromeTabCreator}.
     */
    @Override
    public void onProfileAdded(Profile profile) {
        try (TraceEvent e = TraceEvent.scoped("StartupTabPreloader.onProfileAdded")) {
            // We only care about the first non-incognito profile that's created during startup.
            if (profile.isOffTheRecord()) return;

            ProfileManager.removeObserver(this);
            mTriggerPreload = shouldLoadTab();
            mLoadDecisionMs = SystemClock.uptimeMillis();

            if (mTriggerPreload) loadTab();
            RecordHistogram.recordBooleanHistogram(
                    "Startup.Android.StartupTabPreloader.TabLoaded", mTriggerPreload);
            RecordHistogram.recordEnumeratedHistogram(
                    "Startup.Android.StartupTabPreloader.LoadDecisionReason",
                    getLoadDecisionReason(), LoadDecisionReason.NUM_ENTRIES);
        }
    }

    @Override
    public void onProfileDestroyed(Profile profile) {}

    /**
     * @returns The reason for the decision returned by the most recent invocation of
     * shouldLoadTab().
     */
    @VisibleForTesting
    @LoadDecisionReason
    int getLoadDecisionReason() {
        return mLoadDecisionReason;
    }

    /**
     * @returns True if based on the intent we should load the tab, returns false otherwise.
     */
    @VisibleForTesting
    boolean shouldLoadTab() {
        // If mTab isn't null we've been called before and there is nothing to do.
        if (mTab != null) return false;

        mPreloadPreventedOnlyByFeature = false;

        Intent intent = mIntentSupplier.get();
        if (IntentUtils.safeGetBooleanExtra(intent, EXTRA_DISABLE_STARTUP_TAB_PRELOADER, false)) {
            mLoadDecisionReason = LoadDecisionReason.DISABLED_BY_INTENT;
            return false;
        }

        // We don't support incognito tabs. NOTE: This check is before the check for whether the
        // intent should be ignored to allow capturing the metric for this case separately, as
        // IntentHandler also disallows incognito tab intents not sent by Chrome (i.e.,
        // IntentHandler#shouldIgnoreIntent() returns true in this case).
        boolean incognito = IntentUtils.safeGetBooleanExtra(
                intent, IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);
        if (incognito) {
            mLoadDecisionReason = LoadDecisionReason.INCOGNITO;
            return false;
        }
        if (mIntentHandler.shouldIgnoreIntent(intent, /*startedActivity=*/true)) {
            mLoadDecisionReason = LoadDecisionReason.INTENT_IGNORED;
            return false;
        }
        if (getUrlFromIntent(intent) == null) {
            mLoadDecisionReason = LoadDecisionReason.NO_URL;
            return false;
        }

        // The TabCreatorManager throws an IllegalStateException if it is not ready to provide a
        // TabCreator.
        TabCreator tabCreator;
        try {
            tabCreator = mTabCreatorManager.getTabCreator(incognito);
        } catch (IllegalStateException e) {
            mLoadDecisionReason = LoadDecisionReason.NO_TAB_CREATOR;
            return false;
        }

        // We want to get the TabDelegateFactory but only ChromeTabCreator has one.
        if (!(tabCreator instanceof ChromeTabCreator)) {
            mLoadDecisionReason = LoadDecisionReason.WRONG_TAB_CREATOR;
            return false;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ELIDE_TAB_PRELOAD_AT_STARTUP)) {
            mPreloadPreventedOnlyByFeature = true;
            mLoadDecisionReason = LoadDecisionReason.DISABLED_BY_FEATURE;
            GURL url = UrlFormatter.fixupUrl(getUrlFromIntent(intent));

            // NOTE: We avoid calling IntentHandler.createLoadUrlParamsForIntent() here as that
            // call is destructive to the metadata associated with the intent. Instead, we save the
            // parameters of the load that are used in the check for matching later. This is
            // fragile, but this state is used only for evaluating the effectiveness of startup tab
            // preloading.
            mUrlForPreloadPreventedOnlyByFeature = url.getSpec();
            mReferrerForPreloadPreventedOnlyByFeature =
                    IntentHandler.getReferrerUrlIncludingExtraHeaders(intent);

            return false;
        }

        mLoadDecisionReason = LoadDecisionReason.ALL_SATISFIED;
        return true;
    }

    private void loadTab() {
        Intent intent = mIntentSupplier.get();
        GURL url = UrlFormatter.fixupUrl(getUrlFromIntent(intent));

        ChromeTabCreator chromeTabCreator =
                (ChromeTabCreator) mTabCreatorManager.getTabCreator(false);
        WebContents webContents =
                WebContentsFactory.createWebContents(Profile.getLastUsedRegularProfile(), false);

        mLoadUrlParams = mIntentHandler.createLoadUrlParamsForIntent(url.getSpec(), intent);

        // Create a detached tab, but don't add it to the tab model yet. We'll do that
        // later if the loadUrlParams etc... match.
        mTab = TabBuilder.createLiveTab(false)
                       .setIncognito(false)
                       .setLaunchType(TabLaunchType.FROM_EXTERNAL_APP)
                       .setWindow(mWindowAndroid)
                       .setWebContents(webContents)
                       .setDelegateFactory(chromeTabCreator.createDefaultTabDelegateFactory())
                       .build();

        mObserver = new StartupTabObserver();
        mTab.addObserver(mObserver);
        mTab.loadUrl(mLoadUrlParams);
    }

    private static String getUrlFromIntent(Intent intent) {
        String action = intent.getAction();
        if (Intent.ACTION_VIEW.equals(action) || Intent.ACTION_MAIN.equals(action)
                || (action == null
                        && ChromeTabbedActivity.MAIN_LAUNCHER_ACTIVITY_NAME.equals(
                                intent.getComponent().getClassName()))) {
            // TODO(alexclarke): For ACTION_MAIN maybe refactor TabPersistentStore so we can
            // instantiate (a subset of that) here to extract the URL if it's not set in the
            // intent.
            return IntentHandler.getUrlFromIntent(intent);
        } else {
            return null;
        }
    }

    private class StartupTabObserver extends EmptyTabObserver {
        @Override
        public void onCrash(Tab tab) {
            onDestroy();
        }
    }
}
