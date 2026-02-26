// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
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
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowInfo;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

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
    }

    @After
    public void tearDown() {
        if (mTabbedActivity != null) {
            mTabbedActivity.finish();
        }
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOVE_TO_FRONT_IN_LAUNCH_INTENT_DISPATCHER})
    public void testBringTabToFront_CallsMoveTaskToFront() {
        Intent intent =
                IntentHandler.createTrustedBringTabToFrontIntent(
                        TEST_TAB_ID, IntentHandler.BringToFrontSource.NOTIFICATION);

        TabWindowInfo tabWindowInfo =
                new TabWindowInfo(TEST_WINDOW_ID, mTabModelSelector, mTabModel, mTab);
        when(mTabWindowManager.getTabWindowInfoById(TEST_TAB_ID)).thenReturn(tabWindowInfo);
        when(mTabWindowManager.getIdForWindow(any())).thenReturn(TEST_WINDOW_ID);

        ActivityController<TestChromeLauncherActivity> launcherActivityController =
                Robolectric.buildActivity(TestChromeLauncherActivity.class, intent);
        ChromeLauncherActivity launcherActivity = launcherActivityController.create().get();

        // Verify onNewIntent was called on the target activity
        assert mTabbedActivity.mOnNewIntentCalled;

        // Verify moveTaskToFront was called on the correct task ID
        verify(mActivityManager).moveTaskToFront(mTabbedActivity.getTaskId(), 0);

        // Verify we finished the dispatcher activity
        assert launcherActivity.isFinishing();
    }
}
