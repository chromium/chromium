// Copyright 2019 The Chromium Authors
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
import org.robolectric.annotation.LooperMode;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.test.BackgroundShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for JourneyManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {BackgroundShadowAsyncTask.class})
@LooperMode(LooperMode.Mode.LEGACY)
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
    private LayoutStateObserver mLayoutStateObserver;

    @Mock
    private LayoutStateProvider mLayoutStateProvider;

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
        Robolectric.getBackgroundThreadScheduler().reset();

        MockitoAnnotations.initMocks(this);

        mSharedPreferences = ContextUtils.getApplicationContext().getSharedPreferences(
                JourneyManager.PREFS_FILE, Context.MODE_PRIVATE);
        mSharedPreferences.edit().clear().commit();

        mJourneyManager = new JourneyManager(
                mTabModelSelector, mDispatcher, mLayoutStateProvider, mEngagementTimeUtil);
        mTabModelSelectorTabObserver = mJourneyManager.getTabModelSelectorTabObserver();
        mTabModelSelectorTabModelObserver = mJourneyManager.getTabModelSelectorTabModelObserver();
        mLayoutStateObserver = mJourneyManager.getOverviewModeObserver();

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
    public void onTabHidden_shouldSaveLastEngagement() {
        // Set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);
        flushAsyncPrefs();

        // Advance time.
        doReturn(BASE_TIME_MS + LAST_ENGAGEMENT_ELAPSED_MS).when(mEngagementTimeUtil).currentTime();

        mTabModelSelectorTabObserver.onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);
        flushAsyncPrefs();

        assertEquals(BASE_TIME_MS + LAST_ENGAGEMENT_ELAPSED_MS,
                mSharedPreferences.getLong(String.valueOf(mTab.getId()), -1));
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

        assertTrue(mSharedPreferences.contains(String.valueOf(mTab.getId())));
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

        mTabModelSelectorTabModelObserver.tabClosureCommitted(mTab);
        flushAsyncPrefs();

        assertFalse(mSharedPreferences.contains(String.valueOf(mTab.getId())));
    }

    @Test
    public void onOverviewModeStartedShowing_storesLastEngagement() {
        // Set did paint flag.
        mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(mTab);

        flushAsyncPrefs();

        // Advance time.
        doReturn(BASE_TIME_MS + LAST_ENGAGEMENT_ELAPSED_MS).when(mEngagementTimeUtil).currentTime();

        mLayoutStateObserver.onStartedShowing(LayoutType.TAB_SWITCHER);
        flushAsyncPrefs();

        assertEquals(BASE_TIME_MS + LAST_ENGAGEMENT_ELAPSED_MS,
                mSharedPreferences.getLong(String.valueOf(mTab.getId()), -1));
    }

    @Test
    public void destroy_unregistersLifecycleObserver() {
        mJourneyManager.onDestroy();
        verify(mDispatcher).unregister(mJourneyManager);
        verify(mLayoutStateProvider).removeObserver(mLayoutStateObserver);
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
