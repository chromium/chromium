// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Handler;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Manages Journey related signals, specifically those related to tab engagement.
 */
public class JourneyManager implements Destroyable {
    @VisibleForTesting
    static final String PREFS_FILE = "last_engagement_for_tab_id_pref";

    @VisibleForTesting
    static final String TAB_REVISIT_METRIC = "Tabs.TimeSinceLastView.OnTabView";

    @VisibleForTesting
    static final String TAB_CLOSE_METRIC = "Tabs.TimeSinceLastView.OnTabClose";

    @VisibleForTesting
    static final String TAB_CLOBBER_METRIC = "Tabs.TimeSinceLastView.OnTabClobber";

    private static final long INVALID_TIME = -1;

    // We track this in seconds because UMA can only handle 32-bit signed integers, which 45 days
    // will overflow.
    private static final int MAX_ENGAGEMENT_TIME_S = (int) TimeUnit.DAYS.toSeconds(45);

    private final TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private final TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private final OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final OverviewModeBehavior mOverviewModeBehavior;
    private final EngagementTimeUtil mEngagementTimeUtil;

    private Map<Integer, Boolean> mDidFirstPaintPerTab = new HashMap<>();
    private Map<Integer, Runnable> mPendingRevisits = new HashMap<>();
    private final Handler mHandler = new Handler();
    private Tab mCurrentTab;

    public JourneyManager(TabModelSelector selector,
            @NonNull ActivityLifecycleDispatcher dispatcher,
            @NonNull OverviewModeBehavior overviewModeBehavior,
            EngagementTimeUtil engagementTimeUtil) {
        if (!ChromeVersionInfo.isLocalBuild() && !ChromeVersionInfo.isCanaryBuild()
                && !ChromeVersionInfo.isDevBuild()) {
            // We do not want this in beta/stable until it's no longer backed by SharedPreferences.
            mTabModelSelectorTabObserver = null;
            mTabModelSelectorTabModelObserver = null;
            mOverviewModeObserver = null;
            mLifecycleDispatcher = null;
            mEngagementTimeUtil = null;
            mOverviewModeBehavior = null;
            return;
        }

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onShown(Tab tab, @TabSelectionType int type) {
                if (type != TabSelectionType.FROM_USER) return;

                mCurrentTab = tab;

                recordDeferredEngagementMetric(tab);
            }

            @Override
            public void onHidden(Tab tab, @Tab.TabHidingType int reason) {
                handleTabEngagementStopped(tab);
            }

            @Override
            public void onClosingStateChanged(Tab tab, boolean closing) {
                if (!closing) return;

                mCurrentTab = null;

                recordTabCloseMetric(tab);
            }

            @Override
            public void onDidStartNavigation(Tab tab, NavigationHandle navigationHandle) {
                if (!navigationHandle.isInMainFrame() || navigationHandle.isSameDocument()) return;

                mDidFirstPaintPerTab.put(tab.getId(), false);
            }

            @Override
            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                if (!mDidFirstPaintPerTab.containsKey(tab.getId())) {
                    // If this is the first paint of this Tab in the current app lifetime, record.
                    // e.g. First load of the tab after cold start.
                    recordDeferredEngagementMetric(tab);
                }

                mCurrentTab = tab;

                mDidFirstPaintPerTab.put(tab.getId(), true);

                handleTabEngagementStarted(tab);
            }

            @Override
            public void onLoadUrl(Tab tab, LoadUrlParams params, int loadType) {
                // The transition source (e.g. FROM_ADDRESS_BAR = 0x02000000) is bitwise OR'ed with
                // the transition method (e.g. TYPED = 0x01) and we are only interested in whether
                // a navigation happened from the address bar.
                if ((params.getTransitionType() & PageTransition.FROM_ADDRESS_BAR) == 0) return;

                int tabId = tab.getId();

                if (!mPendingRevisits.containsKey(tabId)) {
                    return;
                }

                mHandler.removeCallbacks(mPendingRevisits.get(tabId));
                mPendingRevisits.remove(tabId);

                recordTabClobberMetric(tab, params.getInputStartTimestamp());
            }
        };

        mTabModelSelectorTabModelObserver = new TabModelSelectorTabModelObserver(selector) {
            @Override
            public void tabClosureCommitted(Tab tab) {
                getPrefs().edit().remove(String.valueOf(tab.getId())).apply();
            }
        };

        mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeStartedShowing(boolean showToolbar) {
                handleTabEngagementStopped(mCurrentTab);
            }
        };
        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);

        mLifecycleDispatcher = dispatcher;
        mLifecycleDispatcher.register(this);

        mEngagementTimeUtil = engagementTimeUtil;
    }

    @Override
    public void destroy() {
        mTabModelSelectorTabObserver.destroy();
        mTabModelSelectorTabModelObserver.destroy();
        mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        mLifecycleDispatcher.unregister(this);
    }

    private void handleTabEngagementStarted(Tab tab) {
        if (tab == null) return;

        Boolean didFirstPaint = mDidFirstPaintPerTab.get(tab.getId());
        if (didFirstPaint == null || !didFirstPaint) return;

        storeLastEngagement(tab.getId(), mEngagementTimeUtil.currentTime());
    }

    private void handleTabEngagementStopped(Tab tab) {
        if (tab == null) return;

        if (mPendingRevisits.containsKey(tab.getId())) {
            mHandler.removeCallbacks(mPendingRevisits.get(tab.getId()));
            mPendingRevisits.remove(tab.getId());
        }

        long lastEngagementMs = mEngagementTimeUtil.currentTime();

        Boolean didFirstPaint = mDidFirstPaintPerTab.get(tab.getId());
        if (didFirstPaint == null || !didFirstPaint) {
            return;
        }

        storeLastEngagement(tab.getId(), lastEngagementMs);
    }

    private void storeLastEngagement(int tabId, long lastEngagementTimestampMs) {
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                getPrefs().edit().putLong(String.valueOf(tabId), lastEngagementTimestampMs).apply();
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {}
        }.executeOnExecutor(AsyncTask.SERIAL_EXECUTOR);
    }

    private SharedPreferences getPrefs() {
        // TODO(mattsimmons): Add a native counterpart to this class and don't write directly to
        //  shared prefs.
        return ContextUtils.getApplicationContext().getSharedPreferences(
                PREFS_FILE, Context.MODE_PRIVATE);
    }

    private long getLastEngagementTimestamp(Tab tab) {
        return getPrefs().getLong(String.valueOf(tab.getId()), INVALID_TIME);
    }

    private void recordDeferredEngagementMetric(Tab tab) {
        final long currentEngagementTimeMs = mEngagementTimeUtil.currentTime();

        assert (!mPendingRevisits.containsKey(tab.getId()));

        Runnable viewMetricTask = () -> recordViewMetric(tab, currentEngagementTimeMs);
        mPendingRevisits.put(tab.getId(), viewMetricTask);
        mHandler.postDelayed(viewMetricTask, mEngagementTimeUtil.tabClobberThresholdMillis());
    }

    private void recordViewMetric(Tab tab, long viewTimeMs) {
        mPendingRevisits.remove(tab.getId());

        long lastEngagement = getLastEngagementTimestamp(tab);

        if (lastEngagement == INVALID_TIME) return;

        long elapsedMs = mEngagementTimeUtil.timeSinceLastEngagement(lastEngagement, viewTimeMs);

        recordEngagementMetric(TAB_REVISIT_METRIC, elapsedMs);

        handleTabEngagementStarted(tab);
    }

    private void recordTabCloseMetric(Tab tab) {
        long lastEngagement = getLastEngagementTimestamp(tab);

        if (lastEngagement == INVALID_TIME) return;

        long elapsedMs = mEngagementTimeUtil.timeSinceLastEngagement(lastEngagement);

        recordEngagementMetric(TAB_CLOSE_METRIC, elapsedMs);
    }

    private void recordTabClobberMetric(Tab tab, long inputStartTimeTicksMs) {
        long lastEngagement = getLastEngagementTimestamp(tab);

        if (lastEngagement == INVALID_TIME) return;

        long elapsedMs = mEngagementTimeUtil.timeSinceLastEngagementFromTimeTicksMs(
                lastEngagement, inputStartTimeTicksMs);

        recordEngagementMetric(TAB_CLOBBER_METRIC, elapsedMs);
    }

    private void recordEngagementMetric(String name, long elapsedMs) {
        if (elapsedMs == INVALID_TIME) return;

        int elapsedSeconds = (int) TimeUnit.MILLISECONDS.toSeconds(elapsedMs);

        RecordHistogram.recordCustomCountHistogram(
                name, elapsedSeconds, 1, MAX_ENGAGEMENT_TIME_S, 50);
    }

    @VisibleForTesting
    public TabObserver getTabModelSelectorTabObserver() {
        return mTabModelSelectorTabObserver;
    }

    @VisibleForTesting
    public TabModelObserver getTabModelSelectorTabModelObserver() {
        return mTabModelSelectorTabModelObserver;
    }

    @VisibleForTesting
    public OverviewModeBehavior.OverviewModeObserver getOverviewModeObserver() {
        return mOverviewModeObserver;
    }
}
