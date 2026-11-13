// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.Context;
import android.content.Intent;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.AndroidTaskUtils;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link IncognitoNotificationServiceImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class IncognitoNotificationServiceUnitTest {
    private @Nullable IncognitoTabHost mRegisteredHost;

    private static class TestChromeTabbedActivity extends ChromeTabbedActivity {
        private int mTaskId;
        private @SupportedProfileType int mSupportedProfileType;
        private @Nullable TabModelSelector mTabModelSelector;
        private boolean mAreTabModelsInitialized;
        private boolean mIsFinishingOrDestroyed;

        @Override
        public int getTaskId() {
            return mTaskId;
        }

        @Override
        public @SupportedProfileType int getSupportedProfileType() {
            return mSupportedProfileType;
        }

        @Override
        public boolean isActivityFinishingOrDestroyed() {
            return mIsFinishingOrDestroyed || super.isActivityFinishingOrDestroyed();
        }

        @Override
        public boolean areTabModelsInitialized() {
            return mAreTabModelsInitialized;
        }

        @Override
        public TabModelSelector getTabModelSelector() {
            if (!mAreTabModelsInitialized || mTabModelSelector == null) {
                throw new AssertionError(
                        "TabModelSelector accessed when not initialized or selector is null");
            }
            return mTabModelSelector;
        }
    }

    private static class TestActivity extends Activity {
        private int mTaskId;

        @Override
        public int getTaskId() {
            return mTaskId;
        }
    }

    @After
    public void tearDown() {
        if (mRegisteredHost != null) {
            IncognitoTabHostRegistry.getInstance().unregister(mRegisteredHost);
            mRegisteredHost = null;
        }
        ApplicationStatus.destroyForJUnitTests();
    }

    @Test
    public void testCloseAllIncognitoNotification_removesOnlyNonRegularStoppedTasks() {
        ActivityManager am =
                (ActivityManager)
                        RuntimeEnvironment.application.getSystemService(Context.ACTIVITY_SERVICE);
        Map<AppTask, RecentTaskInfo> taskInfos = new HashMap<>();

        AppTask regularUninitializedTask = createTabbedTask(taskInfos, 1);
        AppTask regularNoIncognitoTask = createTabbedTask(taskInfos, 2);
        AppTask visibleMixedTask = createTabbedTask(taskInfos, 3);
        AppTask incognitoTask = createTabbedTask(taskInfos, 4);
        AppTask mixedProfileTask = createTabbedTask(taskInfos, 5);
        AppTask sharedTask = createTabbedTask(taskInfos, 6);
        AppTask regularWithIncognitoTask = createTabbedTask(taskInfos, 7);
        AppTask finishingRegularTask = createTabbedTask(taskInfos, 8);
        AppTask deadRecentsTask = createTabbedTask(taskInfos, -1);

        Shadows.shadowOf(am)
                .setAppTasks(
                        List.of(
                                regularUninitializedTask,
                                regularNoIncognitoTask,
                                visibleMixedTask,
                                incognitoTask,
                                mixedProfileTask,
                                sharedTask,
                                regularWithIncognitoTask,
                                finishingRegularTask,
                                deadRecentsTask));
        AndroidTaskUtils.setTaskInfosForTesting(taskInfos);

        registerTabbedActivity(
                1,
                SupportedProfileType.REGULAR,
                ActivityState.STOPPED,
                /* areTabModelsInitialized= */ false,
                /* tabModelSelector= */ null,
                /* isFinishingOrDestroyed= */ false);
        registerTabbedActivity(
                2,
                SupportedProfileType.REGULAR,
                ActivityState.STOPPED,
                /* areTabModelsInitialized= */ true,
                createMockTabModelSelector(/* incognitoTabCount= */ 0),
                /* isFinishingOrDestroyed= */ false);
        registerTabbedActivity(
                3,
                SupportedProfileType.MIXED,
                ActivityState.RESUMED,
                /* areTabModelsInitialized= */ false,
                /* tabModelSelector= */ null,
                /* isFinishingOrDestroyed= */ false);
        registerTabbedActivity(
                4,
                SupportedProfileType.OFF_THE_RECORD,
                ActivityState.STOPPED,
                /* areTabModelsInitialized= */ false,
                /* tabModelSelector= */ null,
                /* isFinishingOrDestroyed= */ false);
        registerTabbedActivity(
                5,
                SupportedProfileType.MIXED,
                ActivityState.STOPPED,
                /* areTabModelsInitialized= */ false,
                /* tabModelSelector= */ null,
                /* isFinishingOrDestroyed= */ false);
        registerTabbedActivity(
                6,
                SupportedProfileType.REGULAR,
                ActivityState.STOPPED,
                /* areTabModelsInitialized= */ false,
                /* tabModelSelector= */ null,
                /* isFinishingOrDestroyed= */ false);
        registerTabbedActivity(
                7,
                SupportedProfileType.REGULAR,
                ActivityState.STOPPED,
                /* areTabModelsInitialized= */ true,
                createMockTabModelSelector(/* incognitoTabCount= */ 1),
                /* isFinishingOrDestroyed= */ false);
        registerTabbedActivity(
                8,
                SupportedProfileType.REGULAR,
                ActivityState.STOPPED,
                /* areTabModelsInitialized= */ true,
                /* tabModelSelector= */ null,
                /* isFinishingOrDestroyed= */ true);

        TestActivity sharedOtherActivity = Robolectric.buildActivity(TestActivity.class).get();
        sharedOtherActivity.mTaskId = 6;
        ApplicationStatus.onStateChangeForTesting(sharedOtherActivity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(sharedOtherActivity, ActivityState.STOPPED);

        // Simulate closing incognito tabs destroying the stacked non-regular activity before the
        // recents sweep runs.
        mRegisteredHost =
                new IncognitoTabHost() {
                    @Override
                    public boolean hasIncognitoTabs() {
                        return false;
                    }

                    @Override
                    public void closeAllIncognitoTabs() {
                        ApplicationStatus.onStateChangeForTesting(
                                sharedOtherActivity, ActivityState.DESTROYED);
                    }

                    @Override
                    public void closeAllIncognitoTabsOnInit() {}

                    @Override
                    public boolean isActiveModel() {
                        return false;
                    }
                };
        IncognitoTabHostRegistry.getInstance().register(mRegisteredHost);

        IncognitoNotificationServiceImpl service = new IncognitoNotificationServiceImpl();
        service.onHandleIntent(null);

        verify(regularUninitializedTask, never()).finishAndRemoveTask();
        verify(regularNoIncognitoTask, never()).finishAndRemoveTask();
        verify(visibleMixedTask, never()).finishAndRemoveTask();
        verify(incognitoTask).finishAndRemoveTask();
        verify(mixedProfileTask).finishAndRemoveTask();
        verify(sharedTask).finishAndRemoveTask();
        verify(regularWithIncognitoTask).finishAndRemoveTask();
        verify(finishingRegularTask).finishAndRemoveTask();
        verify(deadRecentsTask).finishAndRemoveTask();
    }

    private static AppTask createTabbedTask(Map<AppTask, RecentTaskInfo> taskInfos, int taskId) {
        AppTask task = mock(AppTask.class);
        RecentTaskInfo info = new RecentTaskInfo();
        info.id = taskId;
        info.baseIntent = new Intent();
        info.baseIntent.setClassName(
                RuntimeEnvironment.application, ChromeTabbedActivity.class.getName());
        taskInfos.put(task, info);
        return task;
    }

    private static TabModelSelector createMockTabModelSelector(int incognitoTabCount) {
        TabModelSelector selector = mock(TabModelSelector.class);
        TabModel incognitoTabModel = mock(TabModel.class);
        when(incognitoTabModel.getCount()).thenReturn(incognitoTabCount);
        when(selector.getModel(/* incognito= */ true)).thenReturn(incognitoTabModel);
        return selector;
    }

    private static TestChromeTabbedActivity registerTabbedActivity(
            int taskId,
            @SupportedProfileType int profileType,
            @ActivityState int state,
            boolean areTabModelsInitialized,
            @Nullable TabModelSelector tabModelSelector,
            boolean isFinishingOrDestroyed) {
        TestChromeTabbedActivity activity =
                Robolectric.buildActivity(TestChromeTabbedActivity.class).get();
        activity.mTaskId = taskId;
        activity.mSupportedProfileType = profileType;
        activity.mAreTabModelsInitialized = areTabModelsInitialized;
        activity.mTabModelSelector = tabModelSelector;
        activity.mIsFinishingOrDestroyed = isFinishingOrDestroyed;
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        ApplicationStatus.onStateChangeForTesting(activity, state);
        return activity;
    }
}
