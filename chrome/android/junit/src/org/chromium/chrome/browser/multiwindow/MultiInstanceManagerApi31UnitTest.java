// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.util.Pair;
import android.util.SparseArray;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorFactory;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Unit tests for {@link MultiInstanceManagerApi31}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class MultiInstanceManagerApi31UnitTest {
    private static final int INVALID_INSTANCE_ID = MultiInstanceManagerApi31.INVALID_INSTANCE_ID;
    private static final int PASSED_ID_1 = 1;
    private static final int PASSED_ID_2 = 2;
    private static final int PASSED_ID_3 = 3;
    private static final int PASSED_ID_4 = 4;
    private static final int PASSED_ID_INVALID = INVALID_INSTANCE_ID;
    private static final int SAVED_ID_INVALID = INVALID_INSTANCE_ID;
    private static final int TASK_ID_56 = 56;
    private static final int TASK_ID_57 = 57;
    private static final int TASK_ID_58 = 58;
    private static final int TASK_ID_59 = 59;
    private static final int TASK_ID_60 = 60;
    private static final int TASK_ID_61 = 61;

    private TestMultiInstanceManagerApi31 mMultiInstanceManager;
    @Mock
    MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock
    ObservableSupplier<TabModelOrchestrator> mTabModelOrchestratorSupplier;
    @Mock
    TabModelOrchestrator mTabModelOrchestrator;
    @Mock
    ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock
    Activity mActivityTask56;
    @Mock
    Activity mActivityTask57;
    @Mock
    Activity mActivityTask58;
    @Mock
    Activity mActivityTask59;
    @Mock
    Activity mActivityTask60;
    @Mock
    Activity mActivityTask61;

    final Activity mCurrentActivity = mActivityTask56;

    Activity[] mActivityPool;

    private static final TabModelSelectorFactory sMockTabModelSelectorFactory =
            new TabModelSelectorFactory() {
                @Override
                public TabModelSelector buildSelector(Activity activity,
                        TabCreatorManager tabCreatorManager,
                        NextTabPolicySupplier nextTabPolicySupplier, int selectorIndex) {
                    return new MockTabModelSelector(0, 0, null);
                }
            };

    private static class TestMultiInstanceManagerApi31 extends MultiInstanceManagerApi31 {
        // Chrome activities ~ ApplicationStatus.getRunningActivities()
        // (k, v) = (instance, Activity)
        private final SparseArray<Activity> mRunningActivities = new SparseArray<Activity>();

        // Running tasks containing Chrome activity ~ ActivityManager.getAppTasks()
        private final Set<Integer> mAppTasks = new HashSet<>();

        private Activity mAdjacentInstance;

        private TestMultiInstanceManagerApi31(Activity activity,
                ObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
                MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
                ActivityLifecycleDispatcher activityLifecycleDispatcher,
                MenuOrKeyboardActionController menuOrKeyboardActionController) {
            super(activity, tabModelOrchestratorSupplier, multiWindowModeStateDispatcher,
                    activityLifecycleDispatcher, menuOrKeyboardActionController);
        }

        private void createInstance(int instanceId, Activity activity) {
            MultiInstanceManagerApi31.writeUrl(instanceId, "https://id-" + instanceId + ".com");
            mRunningActivities.put(instanceId, activity);
            updateTasks(instanceId, activity);
        }

        private void setAdjacentInstance(Activity activity) {
            mAdjacentInstance = activity;
        }

        // Called when activity instance is destroyed but its task remains alive.
        private void closeInstanceOnly(int instanceId) {
            mRunningActivities.delete(instanceId);
        }

        private void updateTasks(int instanceId, Activity activity) {
            if (instanceId == INVALID_INSTANCE_ID) {
                mAppTasks.remove(activity.getTaskId());
                int index = mRunningActivities.indexOfValue(activity);
                if (index >= 0) mRunningActivities.removeAt(index);
            } else {
                mAppTasks.add(activity.getTaskId());
            }
        }

        @Override
        protected boolean isRunningInAdjacentWindow(Activity activity) {
            return activity == mAdjacentInstance;
        }

        @Override
        protected List<Activity> getAllRunningActivities() {
            List<Activity> result = new ArrayList<>();
            for (int i = 0; i < mRunningActivities.size(); ++i) {
                result.add(mRunningActivities.valueAt(i));
            }
            return result;
        }

        @Override
        protected Set<Integer> getAllChromeTasks() {
            return mAppTasks;
        }

        @Override
        protected void installTabModelObserver() {}
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mActivityTask56.getTaskId()).thenReturn(TASK_ID_56);
        when(mActivityTask57.getTaskId()).thenReturn(TASK_ID_57);
        when(mActivityTask58.getTaskId()).thenReturn(TASK_ID_58);
        when(mActivityTask59.getTaskId()).thenReturn(TASK_ID_59);
        when(mActivityTask60.getTaskId()).thenReturn(TASK_ID_60);
        when(mActivityTask61.getTaskId()).thenReturn(TASK_ID_61);
        when(mTabModelOrchestratorSupplier.get()).thenReturn(mTabModelOrchestrator);

        mActivityPool = new Activity[] {
                mActivityTask56,
                mActivityTask57,
                mActivityTask58,
                mActivityTask59,
                mActivityTask60,
                mActivityTask61,
        };
        TabWindowManagerSingleton.setTabModelSelectorFactoryForTesting(
                sMockTabModelSelectorFactory);
        mMultiInstanceManager = new TestMultiInstanceManagerApi31(mCurrentActivity,
                mTabModelOrchestratorSupplier, mMultiWindowModeStateDispatcher,
                mActivityLifecycleDispatcher, mMenuOrKeyboardActionController);
        SharedPreferencesManager.getInstance().removeKeysWithPrefix(
                ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
    }

    @After
    public void tearDown() {
        SharedPreferencesManager.getInstance().removeKeysWithPrefix(
                ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAllocInstanceId_reachesMaximum() {
        assertTrue(mMultiInstanceManager.mMaxInstances < mActivityPool.length);
        int index = 0;
        for (; index < mMultiInstanceManager.mMaxInstances; ++index) {
            assertEquals(index, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[index]));
        }
        assertEquals(
                INVALID_INSTANCE_ID, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[index]));

        // Activity ID 1 gets removed from memory.
        closeInstanceOnly(mActivityPool[1], 1);

        // We allocated max number of instances already. Activity Id 1 is was removed but
        // remains mapped to a task still alive. No more new allocation is possible.
        assertIsNewTask(mActivityTask60.getTaskId());
        assertEquals(-1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask60));

        // New allocation becomes possible only after a task is gone.
        removeTaskOnRecentsScreen(mActivityPool[2]);
        assertIsNewTask(mActivityTask61.getTaskId());
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask61));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAllocInstanceId_destroyedInstanceMappedBackToItsTask() {
        int index = 0;
        for (; index < mMultiInstanceManager.mMaxInstances; ++index) {
            assertEquals(index, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[index]));
        }

        closeInstanceOnly(mActivityPool[1], 1);

        // New instance is assigned the instance ID 1 again when the associated task is
        // brought foreground and attempts to recreate the activity.
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[1]));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAllocInstanceId_removeTaskOnRecentScreen() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));

        removeTaskOnRecentsScreen(mActivityTask57);

        // New instantiation picks up the smallest available ID.
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask59));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testAllocInstanceId_assignPassedInstanceID() {
        // Take always the the passed ID if valid. This can be from switcher UI, explicitly
        // chosen by a user.
        assertEquals(PASSED_ID_2, allocInstanceIndex(PASSED_ID_2, mActivityTask58));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testGetInstanceInfo_size() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        mMultiInstanceManager.setAdjacentInstance(mActivityTask57);

        assertEquals(3, mMultiInstanceManager.getInstanceInfo().size());

        // Removing a task from recent screen doesn't affect instance info list.
        removeTaskOnRecentsScreen(mActivityTask58);
        assertEquals(3, mMultiInstanceManager.getInstanceInfo().size());

        // Activity destroyed in the background due to memory constraint has no impact either.
        closeInstanceOnly(mActivityTask57, mActivityTask57.getTaskId());
        assertEquals(3, mMultiInstanceManager.getInstanceInfo().size());

        // Closing an instance removes the entry.
        mMultiInstanceManager.closeInstance(1, mActivityTask57.getTaskId());
        assertEquals(2, mMultiInstanceManager.getInstanceInfo().size());
    }

    private int allocInstanceIndex(int passedId, Activity activity) {
        int index = mMultiInstanceManager.allocInstanceId(passedId, activity.getTaskId());

        // Does what TabModelOrchestrator.createTabModels() would do to simulate production code.
        Pair<Integer, TabModelSelector> pair =
                TabWindowManagerSingleton.getInstance().requestSelector(
                        activity, null, null, index);
        if (pair == null) return INVALID_INSTANCE_ID;

        mMultiInstanceManager.createInstance(pair.first, activity);
        mMultiInstanceManager.initialize(pair.first, activity.getTaskId());
        return pair.first;
    }

    // Assert that the given task is new, and not in the task map.
    private void assertIsNewTask(int taskId) {
        for (int i = 0; i < mMultiInstanceManager.mMaxInstances; ++i) {
            assertNotEquals(taskId, MultiInstanceManagerApi31.getTaskFromMap(i));
        }
    }

    // Simulate a task is removed by swiping it away. Both the task and the associated activity
    // get destroyed. Task map gets updated. The persistent state file remains intact.
    private void removeTaskOnRecentsScreen(Activity activityForTask) {
        mMultiInstanceManager.updateTasks(INVALID_INSTANCE_ID, activityForTask);
        destroyActivity(activityForTask);
    }

    // Simulate only an activity gets destroyed, leaving everything intact.
    private void closeInstanceOnly(Activity activity, int instanceId) {
        mMultiInstanceManager.closeInstanceOnly(instanceId);
        destroyActivity(activity);
    }

    private void destroyActivity(Activity activity) {
        ActivityStateListener stateListener =
                (ActivityStateListener) TabWindowManagerSingleton.getInstance();
        stateListener.onActivityStateChange(activity, ActivityState.DESTROYED);
    }
}
