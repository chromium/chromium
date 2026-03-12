// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.content.Intent;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowInfo;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link ChromeLauncherActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeLauncherActivityTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TEST_TAB_ID = 100;
    private static final int TEST_WINDOW_ID = 2;

    // We use a real subclass so we can attach it to ApplicationStatus
    public static class MockChromeTabbedActivity extends ChromeTabbedActivity {
        public TabModelSelector mTabModelSelector;
        public boolean mAreTabModelsInitialized = true;
        public boolean mIsActivityFinishingOrDestroyed;
        public boolean mOnNewIntentCalled;
        public ActivityManager mActivityManager;

        public void setActivityManager(ActivityManager activityManager) {
            mActivityManager = activityManager;
        }

        @Override
        public TabModelSelector getTabModelSelector() {
            return mTabModelSelector;
        }

        @Override
        public boolean areTabModelsInitialized() {
            return mAreTabModelsInitialized;
        }

        @Override
        public boolean isActivityFinishingOrDestroyed() {
            return mIsActivityFinishingOrDestroyed;
        }

        @Override
        @SuppressWarnings("MissingSuperCall")
        public void onNewIntent(Intent intent) {
            mOnNewIntentCalled = true;
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.ACTIVITY_SERVICE.equals(name) && mActivityManager != null) {
                return mActivityManager;
            }
            return super.getSystemService(name);
        }

        @Override
        protected void onApplyThemeResource(
                android.content.res.Resources.Theme theme, int resid, boolean first) {
            super.onApplyThemeResource(
                    theme, org.chromium.chrome.R.style.Theme_Chromium_Activity, first);
        }
    }

    private MockChromeTabbedActivity mTabbedActivity;

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;
    @Mock private ActivityManager mActivityManager;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabModel mTabModel;

    public static class TestChromeLauncherActivity extends ChromeLauncherActivity {}

    @Before
    public void setUp() {
        // Create a real activity object (shadow)
        // Robolectric automatically registers created activities with ApplicationStatus
        mTabbedActivity = Robolectric.buildActivity(MockChromeTabbedActivity.class).create().get();
        mTabbedActivity.setActivityManager(mActivityManager);

        // Inject mocks into our shadow activity
        mTabbedActivity.mTabModelSelector = mTabModelSelector;

        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        MultiWindowUtils.setActivitySupplierForTesting(() -> mTabbedActivity);
        MultiWindowTestUtils.enableMultiInstance();
    }

    @After
    public void tearDown() {
        if (mTabbedActivity != null) {
            mTabbedActivity.finish();
        }
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        MultiWindowUtils.setActivitySupplierForTesting(null);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOVE_TO_FRONT_IN_LAUNCH_INTENT_DISPATCHER})
    public void testBringTabToFront_Fallback_CallsMoveTaskToFront() {
        Intent intent =
                IntentHandler.createTrustedBringTabToFrontIntent(
                        TEST_TAB_ID, IntentHandler.BringToFrontSource.NOTIFICATION);

        TabWindowInfo tabWindowInfo =
                new TabWindowInfo(TEST_WINDOW_ID, mTabModelSelector, mTabModel, mTab);
        when(mTabWindowManager.getTabWindowInfoById(TEST_TAB_ID)).thenReturn(tabWindowInfo);
        when(mTabWindowManager.getIdForWindow(any())).thenReturn(TEST_WINDOW_ID);

        // Ensure AppTask fallback is triggered by returning no matching tasks.
        when(mActivityManager.getAppTasks()).thenReturn(Collections.emptyList());

        ActivityController<TestChromeLauncherActivity> launcherActivityController =
                Robolectric.buildActivity(TestChromeLauncherActivity.class, intent);
        ChromeLauncherActivity launcherActivity = launcherActivityController.create().get();

        // Verify onNewIntent was called on the target activity
        Assert.assertTrue(mTabbedActivity.mOnNewIntentCalled);

        // Verify moveTaskToFront was called on the correct task ID
        verify(mActivityManager).moveTaskToFront(mTabbedActivity.getTaskId(), 0);

        // Verify we finished the dispatcher activity
        Assert.assertTrue(launcherActivity.isFinishing());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOVE_TO_FRONT_IN_LAUNCH_INTENT_DISPATCHER})
    public void testBringTabToFront_CallsAppTaskStartActivity() {
        Intent intent =
                IntentHandler.createTrustedBringTabToFrontIntent(
                        TEST_TAB_ID, IntentHandler.BringToFrontSource.NOTIFICATION);

        TabWindowInfo tabWindowInfo =
                new TabWindowInfo(TEST_WINDOW_ID, mTabModelSelector, mTabModel, mTab);
        when(mTabWindowManager.getTabWindowInfoById(TEST_TAB_ID)).thenReturn(tabWindowInfo);
        when(mTabWindowManager.getIdForWindow(any())).thenReturn(TEST_WINDOW_ID);

        // Mock an AppTask for the target activity.
        AppTask mockTask = mock(AppTask.class);
        ActivityManager.RecentTaskInfo taskInfo = new ActivityManager.RecentTaskInfo();
        taskInfo.taskId = mTabbedActivity.getTaskId();
        when(mockTask.getTaskInfo()).thenReturn(taskInfo);
        when(mActivityManager.getAppTasks()).thenReturn(List.of(mockTask));

        ActivityController<TestChromeLauncherActivity> launcherActivityController =
                Robolectric.buildActivity(TestChromeLauncherActivity.class, intent);
        ChromeLauncherActivity launcherActivity = launcherActivityController.create().get();

        // Verify startActivity was called on the AppTask
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mockTask).startActivity(any(), intentCaptor.capture(), any());
        Assert.assertEquals(0, intentCaptor.getValue().getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK);

        // Verify fallback was NOT called
        Assert.assertFalse(mTabbedActivity.mOnNewIntentCalled);
        verify(mActivityManager, never()).moveTaskToFront(anyInt(), anyInt());

        // Verify we finished the dispatcher activity
        Assert.assertTrue(launcherActivity.isFinishing());
    }
}
