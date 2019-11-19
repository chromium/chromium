// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.SharedPreferences;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for JourneyManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowRecordHistogram.class, BackgroundShadowAsyncTask.class})
public final class JourneyManagerTest {
    private static final int LAST_ENGAGEMENT_ELAPSED_MS = 5000;
    private static final int LAST_ENGAGEMENT_ELAPSED_S = 5;
    private static final int TAB_ID = 123;
    private static final long BASE_TIME_MS = 1000000L;
    private static final long NO_TIME_MS = 0L;
    private static final long DEFER_TIME_MS = 10L;

    @Mock
    private TabModel mTabModel;

    @Mock
    private TabModelSelector mTabModelSelector;

    @Mock
    private OverviewModeBehavior.OverviewModeObserver mOverviewModeObserver;

    @Mock
    private OverviewModeBehavior mOverviewModeBehavior;

    @Mock
    private Tab mTab;

    @Mock
    private TabList mTabList;

    @Mock
    private ActivityLifecycleDispatcher mDispatcher;

    @Mock
    private EngagementTimeUtil mEngagementTimeUtil;

    private JourneyManager mJourneyManager;

    private TabObserver mTabModelSelectorTabObserver;

    private TabModelObserver mTabModelSelectorTabModelObserver;

    private SharedPreferences mSharedPreferences;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        Robolectric.getBackgroundThreadScheduler().reset();

        MockitoAnnotations.initMocks(this);

        mSharedPreferences = ContextUtils.getApplicationContext().getSharedPreferences(
                JourneyManager.PREFS_FILE, Context.MODE_PRIVATE);
        mSharedPreferences.edit().clear().commit();

        mJourneyManager = new JourneyManager(
                mTabModelSelector, mDispatcher, mOverviewModeBehavior, mEngagementTimeUtil);
        mTabModelSelectorTabObserver = mJourneyManager.getTabModelSelectorTabObserver();
        mTabModelSelectorTabModelObserver = mJourneyManager.getTabModelSelectorTabModelObserver();
        mOverviewModeObserver = mJourneyManager.getOverviewModeObserver();

        verify(mDispatcher).register(mJourneyManager);

        // Set up a tab.
        doReturn(TAB_ID).when(mTab).getId();

        // Set up tab model, returning tab above as current.
        List<TabModel> tabModels = new ArrayList<>();
        tabModels.add(mTabModel);
        doReturn(tabModels).when(mTabModelSelector).getModels();
        doReturn(mTab).when(mTabModelSelector).getCurrentTab();
        doReturn(mTabList).when(mTabModel).getComprehensiveModel();
        doReturn(0).when(mTabList).getCount();

        doReturn(BASE_TIME_MS).when(mEngagementTimeUtil).currentTime();
        doReturn(NO_TIME_MS).when(mEngagementTimeUtil).tabClobberThresholdMillis();
    }

    @Test
    public void onTabShown_noPreviousEngagement() {
        mTabModelSelectorTabObserver.onShown(mTab, TabSelectionType.FROM_USER);

        assertEquals(0,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_REVISIT_METRIC, LAST_ENGAGEMENT_ELAPSED_S));
    }

    @Test
    public void didFirstVisuallyNonEmptyPaint_previousEngagementExists_firstLoad() {
        mSharedPreferences.edit().putLong(String.valueOf(mTab.getId()), BASE_TIME_MS).apply();
        flushAsyncPrefs();

        // Move time forward.
        doReturn((long) LAST_ENGAGEMENT_ELAPSED_MS)
                .when(mEngagementTimeUtil)
                .timeSinceLastEngagement(anyLong(), anyLong());

        // Paint to set did paint flag and record metric.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);

        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_REVISIT_METRIC, LAST_ENGAGEMENT_ELAPSED_S));
    }

    @Test
    public void didFirstVisuallyNonEmptyPaint_previousEngagementExists_notFirstLoad() {
        mSharedPreferences.edit().putLong(String.valueOf(mTab.getId()), BASE_TIME_MS).apply();
        flushAsyncPrefs();

        // Move time forward.
        doReturn((long) LAST_ENGAGEMENT_ELAPSED_MS)
                .when(mEngagementTimeUtil)
                .timeSinceLastEngagement(anyLong(), anyLong());

        // Paint to set did paint flag and record first metric.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        flushAsyncPrefs();

        // Paint again.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);

        // Should still only record once.
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_REVISIT_METRIC, LAST_ENGAGEMENT_ELAPSED_S));
    }

    @Test
    public void onTabShown_previousEngagementExists() {
        // Paint to set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        flushAsyncPrefs();

        // Move time forward.
        doReturn((long) LAST_ENGAGEMENT_ELAPSED_MS)
                .when(mEngagementTimeUtil)
                .timeSinceLastEngagement(anyLong(), anyLong());

        mTabModelSelectorTabObserver.onShown(mTab, TabSelectionType.FROM_USER);

        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_REVISIT_METRIC, LAST_ENGAGEMENT_ELAPSED_S));
    }

    @Test
    public void onTabShown_previousEngagementExists_contentNotYetPainted() {
        // Set did paint flag.
        mTabModelSelectorTabObserver.onShown(mTab, TabSelectionType.FROM_USER);
        flushAsyncPrefs();

        // Advance time.
        doReturn(BASE_TIME_MS + LAST_ENGAGEMENT_ELAPSED_MS).when(mEngagementTimeUtil).currentTime();

        mTabModelSelectorTabObserver.onShown(mTab, TabSelectionType.FROM_USER);

        assertEquals(-1, mSharedPreferences.getLong(String.valueOf(mTab.getId()), -1));
    }

    @Test
    public void onTabShown_previousEngagementExists_notSelectedByUser() {
        // Set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        flushAsyncPrefs();

        // Advance time.
        doReturn((long) LAST_ENGAGEMENT_ELAPSED_MS)
                .when(mEngagementTimeUtil)
                .timeSinceLastEngagement(anyLong());

        mTabModelSelectorTabObserver.onShown(mTab, TabSelectionType.FROM_EXIT);

        assertEquals(0,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_REVISIT_METRIC, LAST_ENGAGEMENT_ELAPSED_S));
    }

    @Test
    public void onTabHidden_shouldSaveLastEngagement() {
        // Set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        flushAsyncPrefs();

        // Advance time.
        doReturn(BASE_TIME_MS + LAST_ENGAGEMENT_ELAPSED_MS).when(mEngagementTimeUtil).currentTime();

        mTabModelSelectorTabObserver.onHidden(mTab, Tab.TabHidingType.ACTIVITY_HIDDEN);
        flushAsyncPrefs();

        assertEquals(BASE_TIME_MS + LAST_ENGAGEMENT_ELAPSED_MS,
                mSharedPreferences.getLong(String.valueOf(mTab.getId()), -1));
    }

    @Test
    public void onTabHidden_shouldRemovePendingTasks() {
        // Paint to set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        flushAsyncPrefs();

        doReturn(DEFER_TIME_MS).when(mEngagementTimeUtil).tabClobberThresholdMillis();

        // Move time forward.
        doReturn((long) LAST_ENGAGEMENT_ELAPSED_MS)
                .when(mEngagementTimeUtil)
                .timeSinceLastEngagement(anyLong(), anyLong());

        mTabModelSelectorTabObserver.onShown(mTab, TabSelectionType.FROM_USER);
        mTabModelSelectorTabObserver.onHidden(mTab, TabSelectionType.FROM_USER);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertEquals(0,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_REVISIT_METRIC, LAST_ENGAGEMENT_ELAPSED_S));
    }

    @Test
    public void onClosingStateChanged_noPreviousEngagement() {
        mTabModelSelectorTabObserver.onShown(mTab, TabSelectionType.FROM_USER);
        flushAsyncPrefs();

        mTabModelSelectorTabObserver.onClosingStateChanged(mTab, true);

        assertEquals(0,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_CLOSE_METRIC, LAST_ENGAGEMENT_ELAPSED_S));
    }

    @Test
    public void onClosingStateChanged_previousEngagementExists_tabClosureNotCommitted() {
        // Set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        flushAsyncPrefs();

        // Advance time.
        doReturn((long) LAST_ENGAGEMENT_ELAPSED_MS)
                .when(mEngagementTimeUtil)
                .timeSinceLastEngagement(anyLong());

        mTabModelSelectorTabObserver.onClosingStateChanged(mTab, true);

        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_CLOSE_METRIC, LAST_ENGAGEMENT_ELAPSED_S));

        assertTrue(mSharedPreferences.contains(String.valueOf(mTab.getId())));
    }

    @Test
    public void onClosingStateChanged_previousEngagementExists_notClosing() {
        // Set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        flushAsyncPrefs();

        // Advance time.
        doReturn((long) LAST_ENGAGEMENT_ELAPSED_MS)
                .when(mEngagementTimeUtil)
                .timeSinceLastEngagement(anyLong());

        mTabModelSelectorTabObserver.onClosingStateChanged(mTab, false);

        assertEquals(0,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_CLOSE_METRIC, LAST_ENGAGEMENT_ELAPSED_S));
    }

    @Test
    public void onClosingStateChanged_previousEngagementExists_tabClosureCommitted() {
        // Set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        flushAsyncPrefs();

        // Advance time.
        doReturn((long) LAST_ENGAGEMENT_ELAPSED_MS)
                .when(mEngagementTimeUtil)
                .timeSinceLastEngagement(anyLong());

        mTabModelSelectorTabObserver.onClosingStateChanged(mTab, true);

        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_CLOSE_METRIC, LAST_ENGAGEMENT_ELAPSED_S));

        mTabModelSelectorTabModelObserver.tabClosureCommitted(mTab);
        flushAsyncPrefs();

        assertFalse(mSharedPreferences.contains(String.valueOf(mTab.getId())));
    }

    @Test
    public void onLoadUrl_notFromAddressBar() {
        // Paint to set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        flushAsyncPrefs();

        // Set up a pending view metric.
        doReturn(DEFER_TIME_MS).when(mEngagementTimeUtil).tabClobberThresholdMillis();

        // Move time forward.
        doReturn((long) LAST_ENGAGEMENT_ELAPSED_MS)
                .when(mEngagementTimeUtil)
                .timeSinceLastEngagementFromTimeTicksMs(anyLong(), anyLong());

        mTabModelSelectorTabObserver.onShown(mTab, TabSelectionType.FROM_USER);

        LoadUrlParams params = new LoadUrlParams("http://google.com", PageTransition.FORWARD_BACK);

        mTabModelSelectorTabObserver.onLoadUrl(mTab, params, 0);

        assertEquals(0,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_CLOBBER_METRIC, LAST_ENGAGEMENT_ELAPSED_S));
    }

    @Test
    public void onLoadUrl_fromAddressBar_withinThreshold() {
        // Paint to set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        flushAsyncPrefs();

        // Set up a pending view metric.
        doReturn(DEFER_TIME_MS).when(mEngagementTimeUtil).tabClobberThresholdMillis();

        // Move time forward.
        doReturn((long) LAST_ENGAGEMENT_ELAPSED_MS)
                .when(mEngagementTimeUtil)
                .timeSinceLastEngagementFromTimeTicksMs(anyLong(), anyLong());

        mTabModelSelectorTabObserver.onShown(mTab, TabSelectionType.FROM_USER);

        LoadUrlParams params = new LoadUrlParams(
                "http://google.com", PageTransition.FROM_ADDRESS_BAR | PageTransition.TYPED);

        mTabModelSelectorTabObserver.onLoadUrl(mTab, params, 0);

        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_CLOBBER_METRIC, LAST_ENGAGEMENT_ELAPSED_S));
    }

    @Test
    public void onLoadUrl_fromAddressBar_afterThreshold() {
        // Paint to set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        flushAsyncPrefs();

        // Set up a pending view metric.
        doReturn(NO_TIME_MS).when(mEngagementTimeUtil).tabClobberThresholdMillis();

        // Move time forward.
        doReturn((long) LAST_ENGAGEMENT_ELAPSED_MS)
                .when(mEngagementTimeUtil)
                .timeSinceLastEngagementFromTimeTicksMs(anyLong(), anyLong());

        mTabModelSelectorTabObserver.onShown(mTab, TabSelectionType.FROM_USER);

        LoadUrlParams params = new LoadUrlParams(
                "http://google.com", PageTransition.FROM_ADDRESS_BAR | PageTransition.TYPED);

        mTabModelSelectorTabObserver.onLoadUrl(mTab, params, 0);

        assertEquals(0,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        JourneyManager.TAB_CLOBBER_METRIC, LAST_ENGAGEMENT_ELAPSED_S));
    }

    @Test
    public void onOverviewModeStartedShowing_storesLastEngagement() {
        // Set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);

        flushAsyncPrefs();

        // Advance time.
        doReturn(BASE_TIME_MS + LAST_ENGAGEMENT_ELAPSED_MS).when(mEngagementTimeUtil).currentTime();

        mOverviewModeObserver.onOverviewModeStartedShowing(true);
        flushAsyncPrefs();

        assertEquals(BASE_TIME_MS + LAST_ENGAGEMENT_ELAPSED_MS,
                mSharedPreferences.getLong(String.valueOf(mTab.getId()), -1));
    }

    @Test
    public void destroy_unregistersLifecycleObserver() {
        mJourneyManager.destroy();
        verify(mDispatcher).unregister(mJourneyManager);
        verify(mOverviewModeBehavior).removeOverviewModeObserver(mOverviewModeObserver);
    }

    private void flushAsyncPrefs() {
        try {
            BackgroundShadowAsyncTask.runBackgroundTasks();
        } catch (Exception ex) {
        } finally {
            mSharedPreferences.edit().commit();
        }
    }
}