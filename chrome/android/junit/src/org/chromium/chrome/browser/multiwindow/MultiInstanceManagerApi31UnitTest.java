// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseArray;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManagerApi31UnitTest.ShadowApplicationStatus;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorFactory;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link MultiInstanceManagerApi31}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowApplicationStatus.class})
public class MultiInstanceManagerApi31UnitTest {

    @Rule public JniMocker mMocker = new JniMocker();

    @Mock TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;

    /** Shadows {@link ApplicationStatus} class for testing. */
    @Implements(ApplicationStatus.class)
    public static class ShadowApplicationStatus {
        private static final SparseArray<Activity> sRunningActivities = new SparseArray<>();

        public static void addRunningActivity(int instanceId, Activity activity) {
            sRunningActivities.put(instanceId, activity);
        }

        public static void deleteRunningActivity(int instanceId) {
            sRunningActivities.delete(instanceId);
        }

        public static void removeRunningActivity(Activity activity) {
            int index = sRunningActivities.indexOfValue(activity);
            if (index >= 0) sRunningActivities.removeAt(index);
        }

        @Implementation
        public static List<Activity> getRunningActivities() {
            List<Activity> result = new ArrayList<>();
            for (int i = 0; i < sRunningActivities.size(); ++i) {
                result.add(sRunningActivities.valueAt(i));
            }
            return result;
        }
    }

    private static final int INVALID_INSTANCE_ID = MultiInstanceManagerApi31.INVALID_INSTANCE_ID;
    private static final int INSTANCE_ID_1 = 1;
    private static final int INSTANCE_ID_2 = 2;
    private static final int NON_EXISTANT_INSTANCE_ID = 4;
    private static final int PASSED_ID_2 = 2;
    private static final int PASSED_ID_INVALID = INVALID_INSTANCE_ID;
    private static final int TASK_ID_56 = 56;
    private static final int TASK_ID_57 = 57;
    private static final int TASK_ID_58 = 58;
    private static final int TASK_ID_59 = 59;
    private static final int TASK_ID_60 = 60;
    private static final int TASK_ID_61 = 61;
    private static final int TASK_ID_62 = 62;
    private static final int TASK_ID_63 = 63;

    private static final String TITLE1 = "title1";
    private static final String TITLE2 = "title2";
    private static final String TITLE3 = "title3";
    private static final GURL URL1 = JUnitTestGURLs.URL_1;
    private static final GURL URL2 = JUnitTestGURLs.URL_2;
    private static final GURL URL3 = JUnitTestGURLs.URL_3;

    private TestMultiInstanceManagerApi31 mMultiInstanceManager;
    @Mock MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock ObservableSupplier<TabModelOrchestrator> mTabModelOrchestratorSupplier;
    @Mock TabModelOrchestrator mTabModelOrchestrator;
    @Mock ActivityManager mActivityManager;
    @Mock ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock Supplier<DesktopWindowStateProvider> mDesktopWindowStateProviderSupplier;
    @Mock DesktopWindowStateProvider mDesktopWindowStateProvider;
    @Mock AppHeaderState mAppHeaderState;

    @Mock TabGroupSyncService mTabGroupSyncService;
    @Mock Profile mProfile;
    @Mock Profile mIncognitoProfile;
    @Mock ProfileProvider mProfileProvider;

    @Mock TabModelSelectorBase mTabModelSelector;
    @Mock TabModel mNormalTabModel;
    @Mock TabModel mIncognitoTabModel;
    @Mock Tab mTab1;
    @Mock Tab mTab2;
    @Mock Tab mTab3;

    @Mock Activity mActivityTask56;
    @Mock Activity mActivityTask57;
    @Mock Activity mActivityTask58;
    @Mock Activity mActivityTask59;
    @Mock Activity mActivityTask60;
    @Mock Activity mActivityTask61;
    @Mock ChromeTabbedActivity mTabbedActivityTask62;
    @Mock ChromeTabbedActivity mTabbedActivityTask63;
    @Mock ChromeTabbedActivity mTabbedActivityTask64;
    @Mock ChromeTabbedActivity mTabbedActivityTask65;
    @Mock ChromeTabbedActivity mTabbedActivityTask66;

    Activity mCurrentActivity;

    Activity[] mActivityPool;

    Activity[] mTabbedActivityPool;

    private int mNormalTabCount;
    private int mIncognitoTabCount;

    private OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();

    private final MismatchedIndicesHandler mMismatchedIndicesHandler =
            (activityAtRequestedIndex, isActivityInAppTasks, isActivityInSameTask) -> false;

    private static class TestMultiInstanceManagerApi31 extends MultiInstanceManagerApi31 {
        // Running tasks containing Chrome activity ~ ActivityManager.getAppTasks()
        private final Set<Integer> mAppTaskIds = new HashSet<>();

        private Activity mAdjacentInstance;

        // To save instances info, if desired by the test.
        protected boolean mTestBuildInstancesList;
        private final List<InstanceInfo> mTestInstanceInfos = new ArrayList<>();

        private TestMultiInstanceManagerApi31(
                Activity activity,
                ObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
                MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
                ActivityLifecycleDispatcher activityLifecycleDispatcher,
                ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
                MenuOrKeyboardActionController menuOrKeyboardActionController,
                Supplier<DesktopWindowStateProvider> desktopWindowStateProviderSupplier) {
            super(
                    activity,
                    tabModelOrchestratorSupplier,
                    multiWindowModeStateDispatcher,
                    activityLifecycleDispatcher,
                    modalDialogManagerSupplier,
                    menuOrKeyboardActionController,
                    desktopWindowStateProviderSupplier);
        }

        private void createInstance(int instanceId, Activity activity) {
            MultiInstanceManagerApi31.writeUrl(instanceId, "https://id-" + instanceId + ".com");
            ShadowApplicationStatus.addRunningActivity(instanceId, activity);
            updateTasks(instanceId, activity);
            addInstanceInfo(instanceId, activity.getTaskId());
        }

        private void setAdjacentInstance(Activity activity) {
            mAdjacentInstance = activity;
        }

        // Called when activity instance is destroyed but its task remains alive.
        private void closeInstanceOnly(int instanceId) {
            ShadowApplicationStatus.deleteRunningActivity(instanceId);
        }

        private void addInstanceInfo(int instanceId, int taskId) {
            if (mTestBuildInstancesList) {
                int numberOfInstances = mTestInstanceInfos.size();
                int type =
                        (numberOfInstances == 0)
                                ? InstanceInfo.Type.CURRENT
                                : InstanceInfo.Type.ADJACENT;
                mTestInstanceInfos.add(
                        new InstanceInfo(
                                instanceId,
                                taskId,
                                type,
                                MultiInstanceManagerApi31.readUrl(instanceId),
                                "",
                                0,
                                0,
                                false));
            }
        }

        private void updateTasks(int instanceId, Activity activity) {
            if (instanceId == INVALID_INSTANCE_ID) {
                mAppTaskIds.remove(activity.getTaskId());
                ShadowApplicationStatus.removeRunningActivity(activity);
            } else {
                mAppTaskIds.add(activity.getTaskId());
            }
        }

        private void updateTasksWithoutDestroyingActivity(int instanceId, Activity activity) {
            if (instanceId == INVALID_INSTANCE_ID) {
                mAppTaskIds.remove(activity.getTaskId());
            } else {
                mAppTaskIds.add(activity.getTaskId());
            }
        }

        @Override
        protected boolean isRunningInAdjacentWindow(
                SparseBooleanArray visibleTasks, Activity activity) {
            return activity == mAdjacentInstance;
        }

        @Override
        protected Set<Integer> getAllAppTaskIds(List<AppTask> allTasks) {
            return mAppTaskIds;
        }

        @Override
        protected void installTabModelObserver() {}

        @Override
        public List<InstanceInfo> getInstanceInfo() {
            if (mTestBuildInstancesList) {
                return mTestInstanceInfos;
            }
            return super.getInstanceInfo();
        }

        @Override
        void setupIntentForReparenting(Tab tab, Intent intent, Runnable finalizeCallback) {}

        @Override
        void beginReparenting(
                Tab tab, Intent intent, Bundle startActivityOptions, Runnable finalizeCallback) {}
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mMocker.mock(TabGroupSyncFeaturesJni.TEST_HOOKS, mTabGroupSyncFeaturesJniMock);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(any())).thenReturn(true);

        when(mActivityTask56.getTaskId()).thenReturn(TASK_ID_56);
        when(mActivityTask57.getTaskId()).thenReturn(TASK_ID_57);
        when(mActivityTask58.getTaskId()).thenReturn(TASK_ID_58);
        when(mActivityTask59.getTaskId()).thenReturn(TASK_ID_59);
        when(mActivityTask60.getTaskId()).thenReturn(TASK_ID_60);
        when(mActivityTask61.getTaskId()).thenReturn(TASK_ID_61);
        when(mTabbedActivityTask62.getTaskId()).thenReturn(TASK_ID_62);
        when(mTabbedActivityTask63.getTaskId()).thenReturn(TASK_ID_63);

        when(mActivityTask56.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        when(mActivityTask57.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        when(mActivityTask58.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        when(mActivityTask58.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        when(mActivityTask60.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        when(mActivityTask61.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        when(mTabbedActivityTask62.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        when(mTabbedActivityTask63.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);

        when(mActivityManager.getAppTasks()).thenReturn(new ArrayList());
        when(mTabModelOrchestratorSupplier.get()).thenReturn(mTabModelOrchestrator);

        mProfileProviderSupplier.set(mProfileProvider);
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);

        mActivityPool =
                new Activity[] {
                    mActivityTask56,
                    mActivityTask57,
                    mActivityTask58,
                    mActivityTask59,
                    mActivityTask60,
                    mActivityTask61,
                    mTabbedActivityTask62,
                    mTabbedActivityTask63,
                };
        mCurrentActivity = mActivityTask56;
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        TabWindowManagerSingleton.setTabModelSelectorFactoryForTesting(
                new TabModelSelectorFactory() {
                    @Override
                    public TabModelSelector buildSelector(
                            Context context,
                            OneshotSupplier<ProfileProvider> profileProviderSupplier,
                            TabCreatorManager tabCreatorManager,
                            NextTabPolicySupplier nextTabPolicySupplier) {
                        return new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null);
                    }
                });
        mMultiInstanceManager =
                Mockito.spy(
                        new TestMultiInstanceManagerApi31(
                                mCurrentActivity,
                                mTabModelOrchestratorSupplier,
                                mMultiWindowModeStateDispatcher,
                                mActivityLifecycleDispatcher,
                                mModalDialogManagerSupplier,
                                mMenuOrKeyboardActionController,
                                mDesktopWindowStateProviderSupplier));
        ApplicationStatus.setCachingEnabled(true);
        ApplicationStatus.onStateChangeForTesting(mCurrentActivity, ActivityState.CREATED);
        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP);

        mTabbedActivityPool =
                new Activity[] {
                    mTabbedActivityTask62,
                    mTabbedActivityTask63,
                    mTabbedActivityTask64,
                    mTabbedActivityTask65,
                    mTabbedActivityTask66,
                };

        when(mDesktopWindowStateProviderSupplier.get()).thenReturn(mDesktopWindowStateProvider);
        when(mDesktopWindowStateProvider.getAppHeaderState()).thenReturn(mAppHeaderState);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKeysWithPrefix(ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        ApplicationStatus.destroyForJUnitTests();
        mMultiInstanceManager.mTestBuildInstancesList = false;
        ApplicationStatus.setCachingEnabled(false);
    }

    @Test
    @SmallTest
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
    @EnableFeatures(ChromeFeatureList.MUlTI_INSTANCE_APPLICATION_STATUS_CLEANUP)
    public void testAllocInstanceId_removeTaskOnRecentScreen_withoutDestroy() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));

        // Remove the app task without calling other methods to indicate Activity was destroyed
        removeTaskWithoutDestroyingActivity(mActivityTask56);

        // New instantiation picks up the smallest available ID.
        // assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        Pair<Integer, Integer> instanceIdInfo =
                mMultiInstanceManager.allocInstanceId(
                        PASSED_ID_INVALID, mActivityTask57.getTaskId(), false);
        int index = instanceIdInfo.first;

        // Does what TabModelOrchestrator.createTabModels() would do to simulate production code.
        Pair<Integer, TabModelSelector> pair =
                TabWindowManagerSingleton.getInstance()
                        .requestSelector(
                                mActivityTask57,
                                mProfileProviderSupplier,
                                null,
                                null,
                                mMismatchedIndicesHandler,
                                index);
        int instanceId = pair.first;
        assertEquals(0, instanceId);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.MUlTI_INSTANCE_APPLICATION_STATUS_CLEANUP)
    public void testAllocInstanceId_removeTaskOnRecentScreen_withoutDestroy_fixDisabled() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));

        // Remove the app task without calling other methods to indicate Activity was destroyed
        removeTaskWithoutDestroyingActivity(mActivityTask56);

        // New instantiation picks up the smallest available ID.
        // assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        Pair<Integer, Integer> instanceIdInfo =
                mMultiInstanceManager.allocInstanceId(
                        PASSED_ID_INVALID, mActivityTask57.getTaskId(), false);
        int index = instanceIdInfo.first;

        // Does what TabModelOrchestrator.createTabModels() would do to simulate production code.
        Pair<Integer, TabModelSelector> pair =
                TabWindowManagerSingleton.getInstance()
                        .requestSelector(
                                mActivityTask57,
                                mProfileProviderSupplier,
                                null,
                                null,
                                mMismatchedIndicesHandler,
                                index);
        int instanceId = pair.first;

        // This is the "wrong" id, exercising code path where flag is disabled.
        assertEquals(1, instanceId);
    }

    @Test
    @SmallTest
    public void testAllocInstanceId_assignPassedInstanceID() {
        // Take always the the passed ID if valid. This can be from switcher UI, explicitly
        // chosen by a user.
        assertEquals(PASSED_ID_2, allocInstanceIndex(PASSED_ID_2, mActivityTask58));
    }

    @Test
    @SmallTest
    public void testAllocInstanceId_ignoreWrongPassedInstanceID() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));

        // Go through the ordinary allocation logic if the passed ID is out of range.
        assertEquals(1, allocInstanceIndex(10, mActivityTask59));
    }

    @Test
    @SmallTest
    public void testAllocInstanceId_createFreshNewInstance() {
        int index = 0;
        final int finalIndex = mMultiInstanceManager.mMaxInstances - 1;

        // Allocate all except |finalIndex|.
        for (; index < finalIndex; ++index) {
            assertEquals(index, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[index]));
        }

        removeTaskOnRecentsScreen(mActivityPool[1]);

        // New instantiation picks up the one without persistent state data if asked to do so.
        // Note that ID for mActivityPool[1] is not chosen since it is still mapped to a task
        // internally.
        assertEquals(
                finalIndex,
                allocInstanceIndex(PASSED_ID_INVALID, mActivityTask61, /* preferNew= */ true));
    }

    @Test
    @SmallTest
    public void testAllocInstance_pickMruInstance() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));

        removeTaskOnRecentsScreen(mActivityTask57);
        removeTaskOnRecentsScreen(mActivityTask58);

        // New instantiation picks up the most recently used one.
        MultiInstanceManagerApi31.writeLastAccessedTime(1);
        MultiInstanceManagerApi31.writeLastAccessedTime(2); // Accessed most recently.

        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask59));
        removeTaskOnRecentsScreen(mActivityTask59);

        MultiInstanceManagerApi31.writeLastAccessedTime(1); // instance ID 1 is now the MRU.
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask60));
    }

    @Test
    @SmallTest
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

    @Test
    @SmallTest
    public void testGetInstanceInfo_currentInfoAtTop() {
        // Ensure the single instance at non-zero position is handled okay.
        assertEquals(2, allocInstanceIndex(2, mActivityTask56));
        List<InstanceInfo> info = mMultiInstanceManager.getInstanceInfo();
        assertEquals(1, info.size());
        assertEquals(InstanceInfo.Type.CURRENT, info.get(0).type);

        assertEquals(1, allocInstanceIndex(1, mActivityTask58));
        info = mMultiInstanceManager.getInstanceInfo();
        assertEquals(2, info.size());
        // Current instance (56) is always positioned at the top of the list.
        assertEquals(InstanceInfo.Type.CURRENT, info.get(0).type);

        assertEquals(0, allocInstanceIndex(0, mActivityTask57));
        info = mMultiInstanceManager.getInstanceInfo();
        assertEquals(3, info.size());
        assertEquals(InstanceInfo.Type.CURRENT, info.get(0).type);
    }

    @Test
    @SmallTest
    public void testCurrentInstanceId() {
        // Ensure the single instance at non-zero position is handled okay.
        int expected = 2;
        assertEquals(expected, allocInstanceIndex(expected, mActivityTask56));
        int id = mMultiInstanceManager.getCurrentInstanceId();
        assertEquals("Current instanceId is not as expected", expected, id);
    }

    @Test
    @SmallTest
    public void testSelectedTabUpdatesInstanceInfo() {
        when(mTabModelOrchestratorSupplier.get()).thenReturn(mTabModelOrchestrator);
        when(mTabModelOrchestrator.getTabModelSelector()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getModels()).thenReturn(Collections.emptyList());
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mNormalTabModel.index()).thenReturn(0);

        when(mTab1.isIncognito()).thenReturn(false);
        when(mTab1.getOriginalUrl()).thenReturn(URL1);
        when(mTab1.getTitle()).thenReturn(TITLE1);
        when(mTab2.isIncognito()).thenReturn(false);
        when(mTab2.getOriginalUrl()).thenReturn(URL2);
        when(mTab2.getTitle()).thenReturn(TITLE2);
        when(mTab3.isIncognito()).thenReturn(true);
        when(mTab3.getOriginalUrl()).thenReturn(URL3);
        when(mTab3.getTitle()).thenReturn(TITLE3);

        MultiInstanceManagerApi31 multiInstanceManager =
                new MultiInstanceManagerApi31(
                        mCurrentActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mModalDialogManagerSupplier,
                        mMenuOrKeyboardActionController,
                        mDesktopWindowStateProviderSupplier);
        multiInstanceManager.initialize(INSTANCE_ID_1, TASK_ID_57);
        TabModelObserver tabModelObserver = multiInstanceManager.getTabModelObserverForTesting();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        triggerSelectTab(tabModelObserver, mTab1);
        assertFalse(
                "Normal tab should be selected",
                MultiInstanceManagerApi31.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals(
                "Title should be from the active normal tab",
                TITLE1,
                MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL1.getSpec(),
                MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1));

        // Update url/title as a new normal tab is selected.
        triggerSelectTab(tabModelObserver, mTab2);
        assertFalse(
                "Normal tab should be selected",
                MultiInstanceManagerApi31.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals(
                "Title should be from the active normal tab",
                TITLE2,
                MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL2.getSpec(),
                MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1));

        // Incognito tab doesn't affect url/title when selected.
        triggerSelectTab(tabModelObserver, mTab3);
        assertTrue(
                "Incognito tab should be selected",
                MultiInstanceManagerApi31.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals(
                "Title should be from the active normal tab",
                TITLE2,
                MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL2.getSpec(),
                MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1));

        // Nulled-tab doesn't affect url/title either.
        triggerSelectTab(tabModelObserver, null);
        assertTrue(
                "Incognito tab should be selected",
                MultiInstanceManagerApi31.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals(
                "Null tab should not affect the title",
                TITLE2,
                MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1));
        assertEquals(
                "Null tab should not affect the URL",
                URL2.getSpec(),
                MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1));
    }

    @Test
    @SmallTest
    public void testTabEventsUpdatesTabCounts() {
        when(mTabModelOrchestratorSupplier.get()).thenReturn(mTabModelOrchestrator);
        when(mTabModelOrchestrator.getTabModelSelector()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getModels()).thenReturn(Collections.emptyList());
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        Answer normalTabCount = invocation -> mNormalTabCount;
        when(mNormalTabModel.getCount()).then(normalTabCount);
        Answer incognitoTabCount = invocation -> mIncognitoTabCount;
        when(mIncognitoTabModel.getCount()).then(incognitoTabCount);
        Answer normalActiveTab = invocation -> mNormalTabCount > 0 ? 0 : TabModel.INVALID_TAB_INDEX;
        when(mNormalTabModel.index()).then(normalActiveTab);

        MultiInstanceManagerApi31 multiInstanceManager =
                new MultiInstanceManagerApi31(
                        mCurrentActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mModalDialogManagerSupplier,
                        mMenuOrKeyboardActionController,
                        mDesktopWindowStateProviderSupplier);
        multiInstanceManager.initialize(INSTANCE_ID_1, TASK_ID_57);
        TabModelObserver tabModelObserver = multiInstanceManager.getTabModelObserverForTesting();

        when(mTab1.isIncognito()).thenReturn(false);
        when(mTab2.isIncognito()).thenReturn(false);
        when(mTab3.isIncognito()).thenReturn(true);

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        final String normalTabMessage = "Normal tab count does not match";
        final String incognitoTabMessage = "Normal tab count does not match";
        triggerAddTab(tabModelObserver, mTab1); // normal tab added
        assertEquals(normalTabMessage, 1, MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                0,
                MultiInstanceManagerApi31.readIncognitoTabCount(INSTANCE_ID_1));

        triggerAddTab(tabModelObserver, mTab2); // normal tab added
        assertEquals(normalTabMessage, 2, MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                0,
                MultiInstanceManagerApi31.readIncognitoTabCount(INSTANCE_ID_1));

        triggerAddTab(tabModelObserver, mTab3); // incognito tab added
        assertEquals(normalTabMessage, 2, MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                1,
                MultiInstanceManagerApi31.readIncognitoTabCount(INSTANCE_ID_1));

        triggerOnFinishingTabClosure(tabModelObserver, mTab1);
        assertEquals(normalTabMessage, 1, MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                1,
                MultiInstanceManagerApi31.readIncognitoTabCount(INSTANCE_ID_1));

        triggerTabRemoved(tabModelObserver, mTab3);
        assertEquals(normalTabMessage, 1, MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                0,
                MultiInstanceManagerApi31.readIncognitoTabCount(INSTANCE_ID_1));

        triggerTabRemoved(tabModelObserver, mTab2);
        assertEquals(normalTabMessage, 0, MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                0,
                MultiInstanceManagerApi31.readIncognitoTabCount(INSTANCE_ID_1));
    }

    @Test
    @SmallTest
    public void testZeroNormalTabClearsUrlTitle() {
        when(mTabModelOrchestratorSupplier.get()).thenReturn(mTabModelOrchestrator);
        when(mTabModelOrchestrator.getTabModelSelector()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getModels()).thenReturn(Collections.emptyList());
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        Answer normalTabCount = invocation -> mNormalTabCount;
        when(mNormalTabModel.getCount()).then(normalTabCount);
        Answer incognitoTabCount = invocation -> mIncognitoTabCount;
        when(mIncognitoTabModel.getCount()).then(incognitoTabCount);
        Answer normalActiveTab = invocation -> mNormalTabCount > 0 ? 0 : TabModel.INVALID_TAB_INDEX;

        when(mNormalTabModel.index()).then(normalActiveTab);
        when(mTab1.isIncognito()).thenReturn(false);
        when(mTab1.getOriginalUrl()).thenReturn(URL1);
        when(mTab1.getTitle()).thenReturn(TITLE1);
        when(mTab2.isIncognito()).thenReturn(false);
        when(mTab2.getOriginalUrl()).thenReturn(URL2);
        when(mTab2.getTitle()).thenReturn(TITLE2);

        MultiInstanceManagerApi31 multiInstanceManager =
                new MultiInstanceManagerApi31(
                        mCurrentActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mModalDialogManagerSupplier,
                        mMenuOrKeyboardActionController,
                        mDesktopWindowStateProviderSupplier);
        multiInstanceManager.initialize(INSTANCE_ID_1, TASK_ID_57);
        TabModelObserver tabModelObserver = multiInstanceManager.getTabModelObserverForTesting();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        triggerAddTab(tabModelObserver, mTab1);
        triggerSelectTab(tabModelObserver, mTab1);
        assertEquals(
                "Title should be from the active normal tab",
                TITLE1,
                MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL1.getSpec(),
                MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1));

        triggerAddTab(tabModelObserver, mTab2);
        triggerSelectTab(tabModelObserver, mTab2);
        assertEquals(
                "Title should be from the active normal tab",
                TITLE2,
                MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL2.getSpec(),
                MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1));

        triggerOnFinishingTabClosure(tabModelObserver, mTab1);
        triggerTabRemoved(tabModelObserver, mTab2);
        assertEquals(
                "Tab count should be zero",
                0,
                MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertTrue(
                "Title was not cleared",
                TextUtils.isEmpty(MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1)));
        assertTrue(
                "URL was not cleared",
                TextUtils.isEmpty(MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1)));
    }

    @Test
    @SmallTest
    @Config(sdk = 31)
    public void testGetWindowIdsOfRunningTabbedActivities() {
        // Create 1 activity that is not a ChromeTabbedActivity and 2 ChromeTabbedActivity's.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));

        // Remove ChromeTabbedActivity |mTabbedActivityTask62|, this will be considered a
        // non-running activity subsequently.
        removeTaskOnRecentsScreen(mTabbedActivityTask62);

        SparseIntArray runningTabbedActivityIds =
                MultiInstanceManagerApi31.getWindowIdsOfRunningTabbedActivities();
        assertEquals(
                "There should be only 1 running ChromeTabbedActivity.",
                1,
                runningTabbedActivityIds.size());
        assertEquals(
                "The window ID of the running ChromeTabbedActivity should match.",
                2,
                runningTabbedActivityIds.valueAt(0));
    }

    @Test
    @SmallTest
    @Config(sdk = 31)
    public void testGetRunningTabbedActivityCount() {
        // Create 1 activity that is not a ChromeTabbedActivity and 2 ChromeTabbedActivity's.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));

        // Remove ChromeTabbedActivity |mTabbedActivityTask62|, this will be considered a
        // non-running activity subsequently.
        removeTaskOnRecentsScreen(mTabbedActivityTask62);

        int runningTabbedActivityCount = MultiInstanceManagerApi31.getRunningTabbedActivityCount();
        assertEquals(
                "There should be only 1 running ChromeTabbedActivity.",
                1,
                runningTabbedActivityCount);
    }

    private void triggerSelectTab(TabModelObserver tabModelObserver, Tab tab) {
        // Set up the mocks to have |TabModelUtils.getCurrentTab(selector.getModel(false))|
        // return the last active normal tab.
        if (tab != null && !tab.isIncognito()) when(mNormalTabModel.getTabAt(0)).thenReturn(tab);
        tabModelObserver.didSelectTab(tab, 0, 0);
    }

    private void triggerAddTab(TabModelObserver tabModelObserver, Tab tab) {
        if (tab.isIncognito()) {
            mIncognitoTabCount++;
        } else {
            mNormalTabCount++;
        }
        tabModelObserver.didAddTab(tab, 0, 0, false);
    }

    private void triggerOnFinishingTabClosure(TabModelObserver tabModelObserver, Tab tab) {
        if (tab.isIncognito()) {
            mIncognitoTabCount--;
        } else {
            mNormalTabCount--;
        }
        tabModelObserver.onFinishingTabClosure(tab);
    }

    private void triggerTabRemoved(TabModelObserver tabModelObserver, Tab tab) {
        if (tab.isIncognito()) {
            mIncognitoTabCount--;
        } else {
            mNormalTabCount--;
        }
        tabModelObserver.tabRemoved(tab);
    }

    private int allocInstanceIndex(int passedId, Activity activity) {
        return allocInstanceIndex(passedId, activity, /* preferNew= */ false);
    }

    private int allocInstanceIndex(int passedId, Activity activity, boolean preferNew) {
        Pair<Integer, Integer> instanceIdInfo =
                mMultiInstanceManager.allocInstanceId(passedId, activity.getTaskId(), preferNew);
        int index = instanceIdInfo.first;

        // Does what TabModelOrchestrator.createTabModels() would do to simulate production code.
        Pair<Integer, TabModelSelector> pair =
                TabWindowManagerSingleton.getInstance()
                        .requestSelector(
                                activity,
                                mProfileProviderSupplier,
                                null,
                                null,
                                mMismatchedIndicesHandler,
                                index);
        if (pair == null) return INVALID_INSTANCE_ID;

        int instanceId = pair.first;
        mMultiInstanceManager.createInstance(instanceId, activity);
        mMultiInstanceManager.initialize(instanceId, activity.getTaskId());

        // Store minimal data to get the instance recognized.
        MultiInstanceManagerApi31.writeUrl(instanceId, "url" + instanceId);
        MultiInstanceManagerApi31.writeLastAccessedTime(index);
        ChromeSharedPreferences.getInstance()
                .writeInt(MultiInstanceManagerApi31.tabCountKey(index), 1);
        return instanceId;
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

    private void removeTaskWithoutDestroyingActivity(Activity activityForTask) {
        mMultiInstanceManager.updateTasksWithoutDestroyingActivity(
                INVALID_INSTANCE_ID, activityForTask);
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

    @Test
    @SmallTest
    @Config(sdk = 31)
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    public void testTabMove_MoveTabToNewWindow_calledWithDesiredParameters() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        MultiWindowTestUtils.enableMultiInstance();
        // Allocate and create two instances.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62, true));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63, true));
        doNothing()
                .when(mMultiInstanceManager)
                .moveAndReparentTabToNewWindow(
                        eq(mTab1), eq(INVALID_INSTANCE_ID), eq(true), eq(false), eq(true));

        // Action
        mMultiInstanceManager.moveTabToNewWindow(mTab1);

        // Verify the call is made with desired parameters. The moveAndReparentTabToNewWindow method
        // is validated in integration test here
        // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/javatests/src/org/chromium/chrome/browser/multiwindow/MultiWindowIntegrationTest.java
        verify(mMultiInstanceManager, times(1))
                .moveAndReparentTabToNewWindow(
                        any(), eq(INVALID_INSTANCE_ID), eq(true), eq(false), eq(true));
    }

    @Test
    @SmallTest
    @Config(sdk = 31)
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    public void testTabMove_MoveTabToNewWindow_BeyondMaxWindows_CallsOnly_OpenNewWindow() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        MultiWindowTestUtils.enableMultiInstance();
        // Create max instances first before asking to move a tab from one to another.
        for (int index = 0; index < mMultiInstanceManager.mMaxInstances; ++index) {
            assertEquals(
                    index, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityPool[index], true));
        }
        assertEquals(
                mMultiInstanceManager.mMaxInstances,
                mMultiInstanceManager.getInstanceInfo().size());

        doNothing()
                .when(mMultiInstanceManager)
                .openNewWindow(eq("Android.WindowManager.NewWindow"));

        // Action
        mMultiInstanceManager.moveTabToNewWindow(mTab1);

        // Verify only openNewWindow is called and moveAndReparentTabToNewWindow is not called.
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabToNewWindow(
                        any(), eq(INVALID_INSTANCE_ID), eq(true), eq(false), eq(true));
        verify(mMultiInstanceManager, times(1)).openNewWindow(any());
    }

    @Test
    @Config(sdk = 31)
    public void testTabMove_MoveTabToCurrentWindow_calledWithDesiredParameters() {
        int tabAtIndex = 0;
        mMultiInstanceManager.mTestBuildInstancesList = true;
        // Create two instances first before asking to move a tab from one to current.
        assertEquals(INSTANCE_ID_1, allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask62, true));
        assertEquals(INSTANCE_ID_2, allocInstanceIndex(INSTANCE_ID_2, mTabbedActivityTask63, true));
        assertEquals(2, mMultiInstanceManager.getInstanceInfo().size());

        doNothing().when(mMultiInstanceManager).moveTabAction(any(), eq(mTab1), eq(tabAtIndex));

        // Action
        mMultiInstanceManager.moveTabToWindow(mTabbedActivityTask63, mTab1, tabAtIndex);

        // Verify moveTabAction and getCurrentInstanceInfo are each called once.
        verify(mMultiInstanceManager, times(1)).moveTabAction(any(), eq(mTab1), eq(tabAtIndex));
        verify(mMultiInstanceManager, times(1)).getInstanceInfoFor(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    public void testTabMove_MoveTabAction_WithTabIndex_success() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        // Create two instances first before asking to move a tab from one to current.
        assertEquals(INSTANCE_ID_1, allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask62, true));
        assertEquals(INSTANCE_ID_2, allocInstanceIndex(INSTANCE_ID_2, mTabbedActivityTask63, true));
        assertEquals(2, mMultiInstanceManager.getInstanceInfo().size());

        // Action
        InstanceInfo info = mMultiInstanceManager.getInstanceInfoFor(mTabbedActivityTask63);
        mMultiInstanceManager.moveTabAction(info, mTab1, /* atIndex= */ 0);

        // Verify reparentTabToRunningActivity is called once.
        verify(mMultiInstanceManager, times(1))
                .reparentTabToRunningActivity(any(), eq(mTab1), eq(0));
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabToNewWindow(
                        eq(mTab1), eq(INVALID_INSTANCE_ID), eq(false), eq(true), eq(true));
    }

    @Test
    @Config(sdk = 31)
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    public void testTabMove_MoveTabAction_WithNonExistantInstance_success() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        // Create two instances first before asking to move a tab from one to current.
        assertEquals(INSTANCE_ID_1, allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask62, true));
        assertEquals(
                INSTANCE_ID_2, allocInstanceIndex(INSTANCE_ID_2, mTabbedActivityTask63, false));
        assertEquals(2, mMultiInstanceManager.getInstanceInfo().size());
        MultiWindowTestUtils.enableMultiInstance();
        Mockito.doAnswer(
                        invocation -> {
                            // Change the last parameter to false to bypass calling
                            // IntentUtils.addTrustedIntentExtras() for testing.
                            mMultiInstanceManager.moveAndReparentTabToNewWindow(
                                    mTab1, NON_EXISTANT_INSTANCE_ID, false, true, false);
                            return null;
                        })
                .when(mMultiInstanceManager)
                .moveAndReparentTabToNewWindow(
                        eq(mTab1), eq(NON_EXISTANT_INSTANCE_ID), eq(false), eq(true), eq(true));

        // Action
        InstanceInfo info =
                new InstanceInfo(
                        NON_EXISTANT_INSTANCE_ID,
                        NON_EXISTANT_INSTANCE_ID,
                        InstanceInfo.Type.ADJACENT,
                        "https://id-4.com",
                        "",
                        0,
                        0,
                        false);

        mMultiInstanceManager.moveTabAction(info, mTab1, /* atIndex= */ 0);

        // Verify moveAndReparentTabToNewWindow is called made with desired parameters once. The
        // method is validated in integration test here
        // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/javatests/src/org/chromium/chrome/browser/multiwindow/MultiWindowIntegrationTest.java.
        // Also reparentTabToRunningActivity is not called.
        verify(mMultiInstanceManager, times(1))
                .moveAndReparentTabToNewWindow(
                        eq(mTab1), eq(NON_EXISTANT_INSTANCE_ID), eq(false), eq(true), eq(false));
        verify(mMultiInstanceManager, times(0))
                .reparentTabToRunningActivity(any(), eq(mTab1), eq(0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @Config(sdk = 31)
    public void testCloseChromeWindowIfEmpty_closed() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        MultiWindowTestUtils.enableMultiInstance();
        // Create an empty instance before asking it to close. The flag that provides permission to
        // close is enabled.
        assertEquals(INSTANCE_ID_1, allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask62, true));
        assertEquals(1, mMultiInstanceManager.getInstanceInfo().size());

        // Action
        assertTrue(
                "Chrome instance should be closed.",
                mMultiInstanceManager.closeChromeWindowIfEmpty(INSTANCE_ID_1));

        verify(mMultiInstanceManager, times(1))
                .closeInstance(anyInt(), eq(MultiWindowUtils.INVALID_TASK_ID));
    }

    @Test
    @Config(sdk = 31)
    public void testCloseChromeWindowIfEmpty_inDesktopWindow() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        MultiWindowTestUtils.enableMultiInstance();
        // Create an empty instance before asking it to close.
        assertEquals(INSTANCE_ID_1, allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask62, true));
        assertEquals(1, mMultiInstanceManager.getInstanceInfo().size());
        // Assume that Chrome is in a desktop window.
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);

        // Action
        assertTrue(
                "Chrome instance should be closed.",
                mMultiInstanceManager.closeChromeWindowIfEmpty(INSTANCE_ID_1));

        verify(mMultiInstanceManager, times(1))
                .closeInstance(anyInt(), eq(MultiWindowUtils.INVALID_TASK_ID));
    }

    @Test
    @Config(sdk = 31)
    public void testCloseChromeWindowIfEmpty_notInDesktopWindow() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        MultiWindowTestUtils.enableMultiInstance();
        // Create an empty instance before asking it to close.
        assertEquals(INSTANCE_ID_1, allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask62, true));
        assertEquals(1, mMultiInstanceManager.getInstanceInfo().size());
        // Assume that Chrome is not in a desktop window.
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(false);

        // Action
        assertFalse(
                "Chrome instance should not be closed.",
                mMultiInstanceManager.closeChromeWindowIfEmpty(INSTANCE_ID_1));

        verify(mMultiInstanceManager, never()).closeInstance(anyInt(), anyInt());
    }

    @Test
    @Config(sdk = 31)
    @DisableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER)
    public void testCleanupIfLastInstance() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});

        mMultiInstanceManager.mTestBuildInstancesList = true;
        assertEquals(
                "Failed to alloc INSTANCE_ID_1.",
                INSTANCE_ID_1,
                allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask62, true));
        List<InstanceInfo> instanceInfo = mMultiInstanceManager.getInstanceInfo();
        assertEquals("Expected one instance.", 1, instanceInfo.size());
        assertEquals(
                "First instance should be INSTANCE_ID_1.",
                INSTANCE_ID_1,
                instanceInfo.get(0).instanceId);

        mMultiInstanceManager.cleanupSyncedTabGroupsIfLastInstance();
        verify(mTabGroupSyncService).getAllGroupIds();

        assertEquals(
                "Failed to alloc INSTANCE_ID_2.",
                INSTANCE_ID_2,
                allocInstanceIndex(INSTANCE_ID_2, mTabbedActivityTask63, true));
        assertEquals("Expected two instances.", 2, mMultiInstanceManager.getInstanceInfo().size());

        mMultiInstanceManager.cleanupSyncedTabGroupsIfLastInstance();
        // Verify this is not called a second time.
        verify(mTabGroupSyncService).getAllGroupIds();
    }

    @Test
    @SmallTest
    @Config(sdk = 30)
    public void testOpenInstance_TaskHasRunningActivity() {
        doTestOpenInstanceWithValidTask(/* isActivityAlive= */ true);
    }

    @Test
    @SmallTest
    @Config(sdk = 30)
    public void testOpenInstance_TaskHasNoRunningActivity() {
        doTestOpenInstanceWithValidTask(/* isActivityAlive= */ false);
    }

    private void doTestOpenInstanceWithValidTask(boolean isActivityAlive) {
        // Setup mocks to ensure that MultiWindowUtils#createNewWindowIntent() runs as expected.
        MultiWindowTestUtils.enableMultiInstance();
        when(mTabbedActivityTask62.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());

        // Create the MultiInstanceManager for current activity = |mTabbedActivityTask62| and setup
        // another instance for |mTabbedActivityTask63|.
        MultiInstanceManagerApi31 multiInstanceManager =
                Mockito.spy(
                        new TestMultiInstanceManagerApi31(
                                mTabbedActivityTask62,
                                mTabModelOrchestratorSupplier,
                                mMultiWindowModeStateDispatcher,
                                mActivityLifecycleDispatcher,
                                mModalDialogManagerSupplier,
                                mMenuOrKeyboardActionController,
                                mDesktopWindowStateProviderSupplier));

        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));

        // Setup AppTask's for both activities.
        int taskId62 = mTabbedActivityTask62.getTaskId();
        int taskId63 = mTabbedActivityTask63.getTaskId();
        var appTask62 = mock(AppTask.class);
        var appTaskInfo62 = mock(RecentTaskInfo.class);
        appTaskInfo62.taskId = taskId62;
        when(appTask62.getTaskInfo()).thenReturn(appTaskInfo62);
        var appTask63 = mock(AppTask.class);
        var appTaskInfo63 = mock(RecentTaskInfo.class);
        appTaskInfo63.taskId = taskId63;
        when(appTask63.getTaskInfo()).thenReturn(appTaskInfo63);
        List<AppTask> appTasks = List.of(appTask62, appTask63);
        when(mActivityManager.getAppTasks()).thenReturn(appTasks);

        if (!isActivityAlive) {
            // Force destruction of |mTabbedActivityTask63|.
            destroyActivity(mTabbedActivityTask63);
            ShadowApplicationStatus.removeRunningActivity(mTabbedActivityTask63);
        }

        // Try to restore the instance in task |taskId63|, from |mTabbedActivityTask62|.
        multiInstanceManager.openInstance(1, taskId63);

        if (isActivityAlive) {
            // If |mTabbedActivityTask63| is alive, verify that its instance was restored in the
            // existing task by bringing it to the foreground.
            verify(mActivityManager).moveTaskToFront(taskId63, 0);
            verify(mTabbedActivityTask62, never()).startActivity(any(), any());
            verify(appTask63, never()).finishAndRemoveTask();
        } else {
            // If |mTabbedActivityTask63| is not alive, verify that |mTabbedActivityTask62| starts a
            // new activity and finishes and removes the old task, and does not attempt to bring the
            // old task to the foreground.
            verify(mTabbedActivityTask62).startActivity(any(), any());
            verify(appTask63).finishAndRemoveTask();
            verify(mActivityManager, never()).moveTaskToFront(taskId63, 0);
        }
    }
}
