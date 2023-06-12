// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Handler;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.layouts.FilterLayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;

import java.util.HashMap;
import java.util.Map;

/**
 * Manages Journey related signals, specifically those related to tab engagement.
 */
public class JourneyManager implements DestroyObserver {
    @VisibleForTesting
    static final String PREFS_FILE = "last_engagement_for_tab_id_pref";

    private static final long INVALID_TIME = -1;

    // We track this in seconds because UMA can only handle 32-bit signed integers, which 45 days
    // will overflow.

    private final TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private final TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private final LayoutStateObserver mLayoutStateObserver;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final LayoutStateProvider mLayoutStateProvider;
    private final EngagementTimeUtil mEngagementTimeUtil;

    private Map<Integer, Boolean> mDidFirstPaintPerTab = new HashMap<>();
    private Map<Integer, Runnable> mPendingRevisits = new HashMap<>();
    private final Handler mHandler = new Handler();
    private Tab mCurrentTab;

    public JourneyManager(TabModelSelector selector,
            @NonNull ActivityLifecycleDispatcher dispatcher,
            @NonNull LayoutStateProvider layoutStateProvider,
            EngagementTimeUtil engagementTimeUtil) {
        if (!VersionInfo.isLocalBuild() && !VersionInfo.isCanaryBuild()
                && !VersionInfo.isDevBuild()) {
            // We do not want this in beta/stable until it's no longer backed by SharedPreferences.
            mTabModelSelectorTabObserver = null;
            mTabModelSelectorTabModelObserver = null;
            mLayoutStateObserver = null;
            mLifecycleDispatcher = null;
            mEngagementTimeUtil = null;
            mLayoutStateProvider = null;
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
            public void onHidden(Tab tab, @TabHidingType int reason) {
                handleTabEngagementStopped(tab);
            }

            @Override
            public void onClosingStateChanged(Tab tab, boolean closing) {
                if (!closing) return;

                mCurrentTab = null;
            }

            @Override
            public void onDidStartNavigationInPrimaryMainFrame(
                    Tab tab, NavigationHandle navigationHandle) {
                if (navigationHandle.isSameDocument()) {
                    return;
                }

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
            }
        };

        mTabModelSelectorTabModelObserver = new TabModelSelectorTabModelObserver(selector) {
            @Override
            public void tabClosureCommitted(Tab tab) {
                getPrefs().edit().remove(String.valueOf(tab.getId())).apply();
            }
        };

        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateObserver =
                new FilterLayoutStateObserver(LayoutType.TAB_SWITCHER, new LayoutStateObserver() {
                    @Override
                    public void onStartedShowing(int layoutType) {
                        handleTabEngagementStopped(mCurrentTab);
                    }
                });
        mLayoutStateProvider.addObserver(mLayoutStateObserver);

        mLifecycleDispatcher = dispatcher;
        mLifecycleDispatcher.register(this);

        mEngagementTimeUtil = engagementTimeUtil;
    }

    @Override
    public void onDestroy() {
        mTabModelSelectorTabObserver.destroy();
        mTabModelSelectorTabModelObserver.destroy();
        mLayoutStateProvider.removeObserver(mLayoutStateObserver);
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

        handleTabEngagementStarted(tab);
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
    public LayoutStateObserver getOverviewModeObserver() {
        return mLayoutStateObserver;
    }
}
