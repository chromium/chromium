// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.os.SystemClock;

import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.base.ColdStartTracker;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetrics;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Records UMA page load metrics for the first navigation on a cold start.
 *
 * <p>Uses different cold start heuristics from {@link ActivityTabStartupMetricsTracker}. These
 * heuristics are currently experimental.
 *
 * <p>One pair of histograms is based on {@link ColdStartTracker}. The other heuristic is more
 * speculative, it assumes that cold start finishes as soon as post-inflation startup is complete.
 */
public class ExperimentalStartupMetricsTracker {

    private boolean mFirstNavigationCommitted;

    private class TabObserver extends TabModelSelectorTabObserver {

        private boolean mFirstLoadStarted;

        public TabObserver(TabModelSelector selector) {
            super(selector);
        }

        @Override
        public void onShown(Tab tab, @TabSelectionType int type) {
            if (tab != null && tab.isNativePage()) {
                // Avoid recording metrics when the NTP is shown.
                destroy();
            }
        }

        @Override
        public void onPageLoadStarted(Tab tab, GURL url) {
            // Discard startup navigation measurements when the user started another navigation.
            if (!mFirstLoadStarted) {
                mFirstLoadStarted = true;
            } else {
                destroy();
            }
        }

        @Override
        public void onDidFinishNavigationInPrimaryMainFrame(
                Tab tab, @NonNull NavigationHandle navigation) {
            if (!mShouldTrack || mFirstNavigationCommitted) return;
            boolean shouldTrack =
                    navigation.hasCommitted()
                            && !navigation.isErrorPage()
                            && UrlUtilities.isHttpOrHttps(navigation.getUrl())
                            && !navigation.isSameDocument();
            if (!shouldTrack) {
                // When navigation leads to an error page, download or chrome:// URLs, avoid
                // recording both commit and FCP.
                //
                // In rare cases a same-document navigation can commit before all other
                // http(s)+non-error navigations (crbug.com/1492721). Filter out such scenarios
                // since they are counter-intuitive.
                destroy();
            } else {
                mFirstNavigationCommitted = true;
                recordNavigationCommitMetrics(SystemClock.uptimeMillis() - mActivityStartTimeMs);
            }
        }
    }

    private class PageObserver implements PageLoadMetrics.Observer {
        private static final long NO_NAVIGATION_ID = -1;
        private long mNavigationId = NO_NAVIGATION_ID;

        @Override
        public void onNewNavigation(
                WebContents webContents,
                long navigationId,
                boolean isFirstNavigationInWebContents) {
            if (mNavigationId != NO_NAVIGATION_ID) return;
            mNavigationId = navigationId;
        }

        @Override
        public void onFirstContentfulPaint(
                WebContents webContents,
                long navigationId,
                long navigationStartMicros,
                long firstContentfulPaintMs) {
            recordFcpMetricsIfNeeded(navigationId, navigationStartMicros, firstContentfulPaintMs);
            destroy();
        }

        private void recordFcpMetricsIfNeeded(
                long navigationId, long navigationStartMicros, long firstContentfulPaintMs) {
            if (navigationId != mNavigationId || !mShouldTrack || !mFirstNavigationCommitted) {
                return;
            }
            recordFcpMetrics(
                    navigationStartMicros / 1000 + firstContentfulPaintMs - mActivityStartTimeMs);
        }
    }

    // The time of the activity onCreate(). All metrics (such as time to first visible content) are
    // reported in uptimeMillis relative to this value.
    private final long mActivityStartTimeMs;

    private TabModelSelectorTabObserver mTabObserver;
    private PageObserver mPageObserver;
    private boolean mShouldTrack = true;
    private final boolean mCreatedWhileBrowserNotFullyInitialized;

    public ExperimentalStartupMetricsTracker(
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mActivityStartTimeMs = SystemClock.uptimeMillis();
        mCreatedWhileBrowserNotFullyInitialized =
                !ChromeBrowserInitializer.getInstance().isPostInflationStartupComplete();
        tabModelSelectorSupplier.addObserver(this::registerObservers);
    }

    private void registerObservers(TabModelSelector tabModelSelector) {
        if (!mShouldTrack) return;
        mTabObserver = new TabObserver(tabModelSelector);
        mPageObserver = new PageObserver();
        PageLoadMetrics.addObserver(mPageObserver, /* supportPrerendering= */ false);
    }

    public void destroy() {
        mShouldTrack = false;
        if (mTabObserver != null) {
            mTabObserver.destroy();
            mTabObserver = null;
        }
        if (mPageObserver != null) {
            PageLoadMetrics.removeObserver(mPageObserver);
            mPageObserver = null;
        }
    }

    private void recordHistogram(String name, String suffix, long ms) {
        RecordHistogram.recordMediumTimesHistogram(
                "Startup.Android.Experimental." + name + ".Tabbed." + suffix, ms);
    }

    private void recordNavigationCommitMetrics(long firstCommitMs) {
        if (!SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()) return;
        if (ColdStartTracker.wasColdOnFirstActivityCreationOrNow()) {
            recordHistogram("FirstNavigationCommit", "ColdStartTracker", firstCommitMs);
        }
        if (mCreatedWhileBrowserNotFullyInitialized) {
            recordHistogram("FirstNavigationCommit", "ActivityCreatedWhileInit", firstCommitMs);
        }
    }

    private void recordFcpMetrics(long firstFcpMs) {
        if (!SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()) return;
        if (ColdStartTracker.wasColdOnFirstActivityCreationOrNow()) {
            recordHistogram("FirstContentfulPaint", "ColdStartTracker", firstFcpMs);
        }
        if (mCreatedWhileBrowserNotFullyInitialized) {
            recordHistogram("FirstContentfulPaint", "ActivityCreatedWhileInit", firstFcpMs);
        }
    }
}
