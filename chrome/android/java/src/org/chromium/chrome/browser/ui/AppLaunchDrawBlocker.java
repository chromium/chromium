// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.content.Intent;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.incognito.IncognitoTabLauncher;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.ActiveTabState;
import org.chromium.components.embedder_support.util.UrlUtilities;

/**
 * Helper class for blocking {@link ChromeTabbedActivity} content view draw on launch until the
 * initial tab is available and recording related metrics. It will start blocking the view in
 * #onPostInflationStartup. Once the tab is available, #onActiveTabAvailable should be called stop
 * blocking.
 */
public class AppLaunchDrawBlocker {
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final InflationObserver mInflationObserver;
    private final StartStopWithNativeObserver mStartStopWithNativeObserver;
    private final Supplier<View> mViewSupplier;
    private final Supplier<Intent> mIntentSupplier;
    private final Supplier<Boolean> mShouldIgnoreIntentSupplier;
    private final Supplier<Boolean> mIsTabletSupplier;
    private final ObservableSupplier<Profile> mProfileSupplier;

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
     *
     * @param activityLifecycleDispatcher {@link ActivityLifecycleDispatcher} for the {@link
     *     ChromeTabbedActivity}.
     * @param viewSupplier {@link Supplier<Boolean>} for the Activity's content view.
     * @param intentSupplier The {@link Intent} the app was launched with.
     * @param shouldIgnoreIntentSupplier {@link Supplier<Boolean>} for whether the ignore should be
     *     ignored.
     * @param isTabletSupplier {@link Supplier<Boolean>} for whether the device is a tablet.
     * @param incognitoRestoreAppLaunchDrawBlockerFactory Factory to create {@link
     *     IncognitoRestoreAppLaunchDrawBlocker}.
     */
    public AppLaunchDrawBlocker(
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Supplier<View> viewSupplier,
            @NonNull Supplier<Intent> intentSupplier,
            @NonNull Supplier<Boolean> shouldIgnoreIntentSupplier,
            @NonNull Supplier<Boolean> isTabletSupplier,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull
                    IncognitoRestoreAppLaunchDrawBlockerFactory
                            incognitoRestoreAppLaunchDrawBlockerFactory) {
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mViewSupplier = viewSupplier;
        mInflationObserver =
                new InflationObserver() {
                    @Override
                    public void onPreInflationStartup() {}

                    @Override
                    public void onPostInflationStartup() {
                        maybeBlockDraw();
                        maybeBlockDrawForIncognitoRestore();
                    }
                };
        mActivityLifecycleDispatcher.register(mInflationObserver);
        mStartStopWithNativeObserver =
                new StartStopWithNativeObserver() {
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
        mProfileSupplier = profileSupplier;
        mIncognitoRestoreAppLaunchDrawBlocker =
                incognitoRestoreAppLaunchDrawBlockerFactory.create(
                        intentSupplier,
                        shouldIgnoreIntentSupplier,
                        activityLifecycleDispatcher,
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
        mBlockDrawForInitialTab = false;
    }

    /** Should be called when the overview page is available. */
    public void onOverviewPageAvailable() {
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
        Profile profile = mProfileSupplier.get();
        if (profile == null) return;
        boolean searchEngineHasLogo =
                TemplateUrlServiceFactory.getForProfile(profile.getOriginalProfile())
                        .doesDefaultSearchEngineHaveLogo();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO,
                        searchEngineHasLogo);
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
        @ActiveTabState int tabState = TabPersistentStore.readLastKnownActiveTabStatePref();
        boolean searchEngineHasLogo =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, true);
        boolean singleUrlBarMode =
                NewTabPage.isInSingleUrlBarMode(mIsTabletSupplier.get(), searchEngineHasLogo);

        String url = IntentHandler.getUrlFromIntent(mIntentSupplier.get());
        boolean hasValidIntentUrl = !mShouldIgnoreIntentSupplier.get() && !TextUtils.isEmpty(url);
        boolean isNtpUrl = UrlUtilities.isCanonicalizedNtpUrl(url);

        boolean shouldBlockWithoutIntent =
                shouldBlockDrawForNtpOnColdStartWithoutIntent(
                        tabState,
                        HomepageManager.getInstance().isHomepageNonNtp(),
                        singleUrlBarMode);

        if (shouldBlockDrawForNtpOnColdStartWithIntent(
                hasValidIntentUrl,
                isNtpUrl,
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
            @ActiveTabState int lastKnownActiveTabState,
            boolean homepageNonNtp,
            boolean singleUrlBarMode) {
        boolean willShowNtp =
                lastKnownActiveTabState == ActiveTabState.NTP
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
    private boolean shouldBlockDrawForNtpOnColdStartWithIntent(
            boolean hasValidIntentUrl,
            boolean isNtpUrl,
            boolean shouldLaunchIncognitoTab,
            boolean shouldBlockDrawForNtpOnColdStartWithoutIntent) {
        if (hasValidIntentUrl && isNtpUrl) {
            return !shouldLaunchIncognitoTab;
        } else if (hasValidIntentUrl && !isNtpUrl) {
            return false;
        } else {
            return shouldBlockDrawForNtpOnColdStartWithoutIntent;
        }
    }
}
