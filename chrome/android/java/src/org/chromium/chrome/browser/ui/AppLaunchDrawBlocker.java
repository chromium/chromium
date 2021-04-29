// Copyright 2021 The Chromium Authors. All rights reserved.
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
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.ActiveTabState;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;

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
    static final String APP_LAUNCH_BLOCK_DRAW_DURATION_UMA =
            "Android.AppLaunch.DurationDrawWasBlocked";

    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final InflationObserver mInflationObserver;
    private final StartStopWithNativeObserver mStartStopWithNativeObserver;
    private final Supplier<View> mViewSupplier;
    private final Supplier<Intent> mIntentSupplier;
    private final Supplier<Boolean> mShouldIgnoreIntentSupplier;
    private final Supplier<Boolean> mIsTabletSupplier;
    private final Supplier<Boolean> mShouldShowTabSwitcherOnStartSupplier;

    /**
     * Whether to return false from #onPreDraw of the content view to prevent drawing the browser UI
     * before the tab is ready.
     */
    private boolean mBlockDrawForInitialTab;
    private long mTimeStartedBlockingDrawForInitialTab;

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
     */
    public AppLaunchDrawBlocker(@NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull Supplier<View> viewSupplier, @NonNull Supplier<Intent> intentSupplier,
            @NonNull Supplier<Boolean> shouldIgnoreIntentSupplier,
            @NonNull Supplier<Boolean> isTabletSupplier,
            @NonNull Supplier<Boolean> shouldShowTabSwitcherOnStartSupplier) {
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mViewSupplier = viewSupplier;
        mInflationObserver = new InflationObserver() {
            @Override
            public void onPreInflationStartup() {}

            @Override
            public void onPostInflationStartup() {
                maybeBlockDrawForInitialTab();
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
        mShouldShowTabSwitcherOnStartSupplier = shouldShowTabSwitcherOnStartSupplier;
    }

    /** Unregister lifecycle observers. */
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(mInflationObserver);
        mActivityLifecycleDispatcher.unregister(mStartStopWithNativeObserver);
    }

    /** Should be called when the initial tab is available. */
    public void onActiveTabAvailable(boolean isTabNtp) {
        recordBlockDrawForInitialTabHistograms(isTabNtp);
        mBlockDrawForInitialTab = false;
    }

    private void writeSearchEngineHadLogoPref() {
        boolean searchEngineHasLogo = TemplateUrlServiceFactory.doesDefaultSearchEngineHaveLogo();
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, searchEngineHasLogo);
    }

    /** Only block the draw if we believe the initial tab will be the NTP. */
    private void maybeBlockDrawForInitialTab() {
        if (mShouldShowTabSwitcherOnStartSupplier.get()) return;

        @ActiveTabState
        int tabState = TabPersistentStore.readLastKnownActiveTabStatePref();
        boolean searchEngineHasLogo = SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, true);
        boolean singleUrlBarMode =
                NewTabPage.isInSingleUrlBarMode(mIsTabletSupplier.get(), searchEngineHasLogo);

        String url = IntentHandler.getUrlFromIntent(mIntentSupplier.get());
        boolean hasValidIntentUrl = !mShouldIgnoreIntentSupplier.get() && !TextUtils.isEmpty(url);
        boolean isNtpUrl = ReturnToChromeExperimentsUtil.isCanonicalizedNTPUrl(url);

        boolean shouldBlockWithoutIntent = shouldBlockDrawForNtpOnColdStartWithoutIntent(
                tabState, HomepageManager.isHomepageNonNtpPreNative(), singleUrlBarMode);

        if (shouldBlockDrawForNtpOnColdStartWithIntent(hasValidIntentUrl, isNtpUrl,
                    StartSurfaceConfiguration.shouldIntentShowNewTabOmniboxFocused(
                            mIntentSupplier.get()),
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
     * @param shouldShowNewTabOmniboxFocused Whether the intent will open a new tab with omnibox
     *        focused.
     * @param shouldLaunchIncognitoTab Whether the intent is launching an incognito tab.
     * @param shouldBlockDrawForNtpOnColdStartWithoutIntent Result of
     *        {@link #shouldBlockDrawForNtpOnColdStartWithoutIntent}.
     * @return Whether the View draw should be blocked because the NTP will be shown on cold start.
     */
    private boolean shouldBlockDrawForNtpOnColdStartWithIntent(boolean hasValidIntentUrl,
            boolean isNtpUrl, boolean shouldShowNewTabOmniboxFocused,
            boolean shouldLaunchIncognitoTab,
            boolean shouldBlockDrawForNtpOnColdStartWithoutIntent) {
        if (hasValidIntentUrl && isNtpUrl) {
            // TODO(crbug.com/1199374): We should find another solution for focusing on new tab
            // rather than special casing it here.
            return !shouldShowNewTabOmniboxFocused && !shouldLaunchIncognitoTab;
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
     */
    private void recordBlockDrawForInitialTabHistograms(boolean isTabRegularNtp) {
        boolean searchEngineHasLogo = TemplateUrlServiceFactory.doesDefaultSearchEngineHaveLogo();
        boolean singleUrlBarMode =
                NewTabPage.isInSingleUrlBarMode(mIsTabletSupplier.get(), searchEngineHasLogo);
        boolean focusedOmnibox = StartSurfaceConfiguration.shouldIntentShowNewTabOmniboxFocused(
                mIntentSupplier.get());
        boolean singleUrlBarNtp = isTabRegularNtp && singleUrlBarMode;
        long durationDrawBlocked =
                SystemClock.elapsedRealtime() - mTimeStartedBlockingDrawForInitialTab;

        @BlockDrawForInitialTabAccuracy
        int enumEntry;
        boolean shouldBlockDraw = singleUrlBarNtp && !focusedOmnibox;
        if (mBlockDrawForInitialTab) {
            enumEntry = shouldBlockDraw
                    ? BlockDrawForInitialTabAccuracy.BLOCKED_CORRECTLY
                    : BlockDrawForInitialTabAccuracy.BLOCKED_BUT_SHOULD_NOT_HAVE;

            RecordHistogram.recordTimesHistogram(
                    APP_LAUNCH_BLOCK_DRAW_DURATION_UMA, durationDrawBlocked);
        } else {
            enumEntry = shouldBlockDraw
                    ? BlockDrawForInitialTabAccuracy.DID_NOT_BLOCK_BUT_SHOULD_HAVE
                    : BlockDrawForInitialTabAccuracy.CORRECTLY_DID_NOT_BLOCK;
        }
        RecordHistogram.recordEnumeratedHistogram(APP_LAUNCH_BLOCK_DRAW_ACCURACY_UMA, enumEntry,
                BlockDrawForInitialTabAccuracy.COUNT);
    }
}
