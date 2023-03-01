// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.content.Intent;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.incognito.IncognitoTabLauncher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.ActiveTabState;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class for blocking {@link ChromeTabbedActivity} content view draw on launch until the
 * initial tab is available and recording related metrics. It will start blocking the view in
 * #onPostInflationStartup. Once the tab is available, #onActiveTabAvailable should be called stop
 * blocking.
 */
public class AppLaunchDrawBlocker {
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    @IntDef({BlockDrawForInitialTabAccuracy.BLOCKED_CORRECTLY,
            BlockDrawForInitialTabAccuracy.BLOCKED_BUT_SHOULD_NOT_HAVE,
            BlockDrawForInitialTabAccuracy.DID_NOT_BLOCK_BUT_SHOULD_HAVE,
            BlockDrawForInitialTabAccuracy.CORRECTLY_DID_NOT_BLOCK})
    @Retention(RetentionPolicy.SOURCE)
    public @interface BlockDrawForInitialTabAccuracy {
        int BLOCKED_CORRECTLY = 0;
        int BLOCKED_BUT_SHOULD_NOT_HAVE = 1;
        int DID_NOT_BLOCK_BUT_SHOULD_HAVE = 2;
        int CORRECTLY_DID_NOT_BLOCK = 3;

        int COUNT = 4;
    }

    @VisibleForTesting
    static final String APP_LAUNCH_BLOCK_DRAW_ACCURACY_UMA =
            "Android.AppLaunch.BlockDrawForInitialTabAccuracy";
    @VisibleForTesting
    static final String APP_LAUNCH_BLOCK_INITIAL_TAB_DRAW_DURATION_UMA =
            "Android.AppLaunch.DurationDrawWasBlocked.OnInitialTab";
    @VisibleForTesting
    static final String APP_LAUNCH_BLOCK_OVERVIEW_PAGE_DRAW_DURATION_UMA =
            "Android.AppLaunch.DurationDrawWasBlocked.OnOverviewPage";

    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final InflationObserver mInflationObserver;
    private final StartStopWithNativeObserver mStartStopWithNativeObserver;
    private final Supplier<View> mViewSupplier;
    private final Supplier<Intent> mIntentSupplier;
    private final Supplier<Boolean> mShouldIgnoreIntentSupplier;
    private final Supplier<Boolean> mIsTabletSupplier;
    private final Supplier<Boolean> mShouldShowOverviewPageOnStartSupplier;
    private final Supplier<Boolean> mIsInstantStartEnabledSupplier;

    /**
     * An app draw blocker that takes care of blocking the draw when we are restoring tabs with
     * Incognito.
     */
    private final IncognitoRestoreAppLaunchDrawBlocker mIncognitoRestoreAppLaunchDrawBlocker;

    /**
     * Whether to return false from #onPreDraw of the content view to prevent drawing the browser UI
     * before the tab is ready.
     */
    private boolean mBlockDrawForInitialTab;
    private boolean mBlockDrawForOverviewPage;
    private boolean mBlockDrawForIncognitoRestore;
    private long mTimeStartedBlockingDrawForInitialTab;
    private long mTimeStartedBlockingDrawForIncognitoRestore;

    /**
     * Constructor for AppLaunchDrawBlocker.
     * @param activityLifecycleDispatcher {@link ActivityLifecycleDispatcher} for the
     *        {@link ChromeTabbedActivity}.
     * @param viewSupplier {@link Supplier<Boolean>} for the Activity's content view.
     * @param intentSupplier The {@link Intent} the app was launched with.
     * @param shouldIgnoreIntentSupplier {@link Supplier<Boolean>} for whether the ignore should be
     *        ignored.
     * @param isTabletSupplier {@link Supplier<Boolean>} for whether the device is a tablet.
     * @param shouldShowTabSwitcherOnStartSupplier {@link Supplier<Boolean>} for whether the tab
     *        switcher should be shown on start.
     * @param incognitoRestoreAppLaunchDrawBlockerFactory Factory to create
     *    {@link IncognitoRestoreAppLaunchDrawBlocker}.
     */
    public AppLaunchDrawBlocker(@NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Supplier<View> viewSupplier, @NonNull Supplier<Intent> intentSupplier,
            @NonNull Supplier<Boolean> shouldIgnoreIntentSupplier,
            @NonNull Supplier<Boolean> isTabletSupplier,
            @NonNull Supplier<Boolean> shouldShowTabSwitcherOnStartSupplier,
            @NonNull Supplier<Boolean> isInstantStartEnabledSupplier,
            @NonNull IncognitoRestoreAppLaunchDrawBlockerFactory
                    incognitoRestoreAppLaunchDrawBlockerFactory) {
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mViewSupplier = viewSupplier;
        mInflationObserver = new InflationObserver() {
            @Override
            public void onPreInflationStartup() {}

            @Override
            public void onPostInflationStartup() {
                maybeBlockDraw();
                maybeBlockDrawForIncognitoRestore();
            }
        };
        mActivityLifecycleDispatcher.register(mInflationObserver);
        mStartStopWithNativeObserver = new StartStopWithNativeObserver() {
            @Override
            public void onStartWithNative() {}

            @Override
            public void onStopWithNative() {
                writeSearchEngineHadLogoPref();
            }
        };
        mActivityLifecycleDispatcher.register(mStartStopWithNativeObserver);
        mIntentSupplier = intentSupplier;
        mShouldIgnoreIntentSupplier = shouldIgnoreIntentSupplier;
        mIsTabletSupplier = isTabletSupplier;
        mShouldShowOverviewPageOnStartSupplier = shouldShowTabSwitcherOnStartSupplier;
        mIsInstantStartEnabledSupplier = isInstantStartEnabledSupplier;
        mIncognitoRestoreAppLaunchDrawBlocker = incognitoRestoreAppLaunchDrawBlockerFactory.create(
                intentSupplier, shouldIgnoreIntentSupplier, activityLifecycleDispatcher,
                this::onIncognitoRestoreUnblockConditionsFired);
    }

    /** Unregister lifecycle observers. */
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(mInflationObserver);
        mActivityLifecycleDispatcher.unregister(mStartStopWithNativeObserver);
        mIncognitoRestoreAppLaunchDrawBlocker.destroy();
    }

    /** Should be called when the initial tab is available. */
    public void onActiveTabAvailable(boolean isTabNtp) {
        // If the draw is blocked because of overview, the histograms would be recorded in
        // #onOverviewPageAvailable.
        if (!mBlockDrawForOverviewPage) {
            recordBlockDrawForInitialTabHistograms(
                    isTabNtp, /*isOverviewShownWithoutInstantStart=*/false);
        }
        mBlockDrawForInitialTab = false;
    }

    /** Should be called when the overview page is available. */
    public void onOverviewPageAvailable(boolean isOverviewShownWithoutInstantStart) {
        if (mBlockDrawForOverviewPage) {
            recordBlockDrawForInitialTabHistograms(
                    /*isTabRegularNtp=*/false, isOverviewShownWithoutInstantStart);
        }
        mBlockDrawForOverviewPage = false;
    }

    /**
     * A method that is passed as a {@link Runnable} to {@link
     * IncognitoRestoreAppLaunchDrawBlocker}.
     *
     * This gets fired when all the conditions needed to unblock the draw from the Incognito restore
     * are fired.
     */
    @VisibleForTesting
    public void onIncognitoRestoreUnblockConditionsFired() {
        if (mBlockDrawForIncognitoRestore) {
            mBlockDrawForIncognitoRestore = false;
            RecordHistogram.recordTimesHistogram(
                    "Android.AppLaunch.DurationDrawWasBlocked.OnIncognitoReauth",
                    SystemClock.elapsedRealtime() - mTimeStartedBlockingDrawForIncognitoRestore);
        }
    }

    private void writeSearchEngineHadLogoPref() {
        boolean searchEngineHasLogo =
                TemplateUrlServiceFactory.getForProfile(Profile.getLastUsedRegularProfile())
                        .doesDefaultSearchEngineHaveLogo();
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, searchEngineHasLogo);
    }

    /**
     * Conditionally blocks the draw independently from the other clients for the Incognito restore
     * use-case.
     */
    private void maybeBlockDrawForIncognitoRestore() {
        if (!mIncognitoRestoreAppLaunchDrawBlocker.shouldBlockDraw()) return;
        mBlockDrawForIncognitoRestore = true;
        mTimeStartedBlockingDrawForIncognitoRestore = SystemClock.elapsedRealtime();
        ViewDrawBlocker.blockViewDrawUntilReady(
                mViewSupplier.get(), () -> !mBlockDrawForIncognitoRestore);
    }

    /** Only block the draw if we believe the initial tab will be the NTP. */
    private void maybeBlockDraw() {
        if (mShouldShowOverviewPageOnStartSupplier.get()) {
            if (!mIsInstantStartEnabledSupplier.get()) {
                mTimeStartedBlockingDrawForInitialTab = SystemClock.elapsedRealtime();
                mBlockDrawForOverviewPage = true;
                ViewDrawBlocker.blockViewDrawUntilReady(
                        mViewSupplier.get(), () -> !mBlockDrawForOverviewPage);
            }
            return;
        }

        @ActiveTabState
        int tabState = TabPersistentStore.readLastKnownActiveTabStatePref();
        boolean searchEngineHasLogo = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, true);
        boolean singleUrlBarMode =
                NewTabPage.isInSingleUrlBarMode(mIsTabletSupplier.get(), searchEngineHasLogo);

        String url = IntentHandler.getUrlFromIntent(mIntentSupplier.get());
        boolean hasValidIntentUrl = !mShouldIgnoreIntentSupplier.get() && !TextUtils.isEmpty(url);
        boolean isNtpUrl = UrlUtilities.isCanonicalizedNTPUrl(url);

        boolean shouldBlockWithoutIntent = shouldBlockDrawForNtpOnColdStartWithoutIntent(
                tabState, HomepageManager.isHomepageNonNtpPreNative(), singleUrlBarMode);

        if (shouldBlockDrawForNtpOnColdStartWithIntent(hasValidIntentUrl, isNtpUrl,
                    IncognitoTabLauncher.didCreateIntent(mIntentSupplier.get()),
                    shouldBlockWithoutIntent)) {
            mTimeStartedBlockingDrawForInitialTab = SystemClock.elapsedRealtime();
            mBlockDrawForInitialTab = true;
            ViewDrawBlocker.blockViewDrawUntilReady(
                    mViewSupplier.get(), () -> !mBlockDrawForInitialTab);
        }
    }

    /**
     * @param lastKnownActiveTabState Last known {@link @ActiveTabState}.
     * @param homepageNonNtp Whether the homepage is Non-Ntp.
     * @param singleUrlBarMode Whether in single UrlBar mode, i.e. the url bar is shown in-line in
     *        the NTP.
     * @return Whether the View draw should be blocked because the NTP will be shown on cold start.
     */
    private boolean shouldBlockDrawForNtpOnColdStartWithoutIntent(
            @ActiveTabState int lastKnownActiveTabState, boolean homepageNonNtp,
            boolean singleUrlBarMode) {
        boolean willShowNtp = lastKnownActiveTabState == ActiveTabState.NTP
                || (lastKnownActiveTabState == ActiveTabState.EMPTY && !homepageNonNtp);
        return willShowNtp && singleUrlBarMode;
    }

    /**
     * @param hasValidIntentUrl Whether there is an intent that isn't ignored with a non-empty Url.
     * @param isNtpUrl Whether the intent has NTP Url.
     * @param shouldLaunchIncognitoTab Whether the intent is launching an incognito tab.
     * @param shouldBlockDrawForNtpOnColdStartWithoutIntent Result of
     *        {@link #shouldBlockDrawForNtpOnColdStartWithoutIntent}.
     * @return Whether the View draw should be blocked because the NTP will be shown on cold start.
     */
    private boolean shouldBlockDrawForNtpOnColdStartWithIntent(boolean hasValidIntentUrl,
            boolean isNtpUrl, boolean shouldLaunchIncognitoTab,
            boolean shouldBlockDrawForNtpOnColdStartWithoutIntent) {
        if (hasValidIntentUrl && isNtpUrl) {
            return !shouldLaunchIncognitoTab;
        } else if (hasValidIntentUrl && !isNtpUrl) {
            return false;
        } else {
            return shouldBlockDrawForNtpOnColdStartWithoutIntent;
        }
    }

    /**
     * Record histograms related to blocking draw for the initial tab. These histograms record
     * whether the prediction for blocking the view draw was accurate and the duration the draw was
     * blocked for.
     * @param isTabRegularNtp Whether the tab is regular NTP, not incognito.
     * @param isOverviewShownWithoutInstantStart Whether it's on overview page without Instant Start
     *         enabled.
     */
    private void recordBlockDrawForInitialTabHistograms(
            boolean isTabRegularNtp, boolean isOverviewShownWithoutInstantStart) {
        long durationDrawBlocked =
                SystemClock.elapsedRealtime() - mTimeStartedBlockingDrawForInitialTab;

        boolean singleUrlBarNtp = false;
        if (!isOverviewShownWithoutInstantStart) {
            boolean searchEngineHasLogo =
                    TemplateUrlServiceFactory.getForProfile(Profile.getLastUsedRegularProfile())
                            .doesDefaultSearchEngineHaveLogo();
            boolean singleUrlBarMode =
                    NewTabPage.isInSingleUrlBarMode(mIsTabletSupplier.get(), searchEngineHasLogo);
            singleUrlBarNtp = isTabRegularNtp && singleUrlBarMode;
        }

        @BlockDrawForInitialTabAccuracy
        int enumEntry;
        boolean shouldBlockDraw = singleUrlBarNtp || isOverviewShownWithoutInstantStart;
        if (mBlockDrawForInitialTab || mBlockDrawForOverviewPage) {
            enumEntry = shouldBlockDraw
                    ? BlockDrawForInitialTabAccuracy.BLOCKED_CORRECTLY
                    : BlockDrawForInitialTabAccuracy.BLOCKED_BUT_SHOULD_NOT_HAVE;

            RecordHistogram.recordTimesHistogram(mBlockDrawForInitialTab
                            ? APP_LAUNCH_BLOCK_INITIAL_TAB_DRAW_DURATION_UMA
                            : APP_LAUNCH_BLOCK_OVERVIEW_PAGE_DRAW_DURATION_UMA,
                    durationDrawBlocked);
        } else {
            enumEntry = shouldBlockDraw
                    ? BlockDrawForInitialTabAccuracy.DID_NOT_BLOCK_BUT_SHOULD_HAVE
                    : BlockDrawForInitialTabAccuracy.CORRECTLY_DID_NOT_BLOCK;
        }
        RecordHistogram.recordEnumeratedHistogram(APP_LAUNCH_BLOCK_DRAW_ACCURACY_UMA, enumEntry,
                BlockDrawForInitialTabAccuracy.COUNT);
    }
}
