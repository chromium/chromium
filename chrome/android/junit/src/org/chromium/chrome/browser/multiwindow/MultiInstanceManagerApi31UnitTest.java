// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
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
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadataExtractor;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabwindow.TabModelSelectorFactory;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.util.XrUtils;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link MultiInstanceManagerApi31}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MultiInstanceManagerApi31UnitTest {
    private static final int INSTANCE_ID_1 = 1;
    private static final int INSTANCE_ID_2 = 2;
    private static final int NON_EXISTENT_INSTANCE_ID = 4;
    private static final int PASSED_ID_2 = 2;
    private static final int PASSED_ID_INVALID = INVALID_WINDOW_ID;
    private static final int TASK_ID_56 = 56;
    private static final int TASK_ID_57 = 57;
    private static final int TASK_ID_58 = 58;
    private static final int TASK_ID_59 = 59;
    private static final int TASK_ID_60 = 60;
    private static final int TASK_ID_61 = 61;
    private static final int TASK_ID_62 = 62;
    private static final int TASK_ID_63 = 63;

    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int TAB_ID_3 = 3;
    private static final GURL TAB_URL_1 = new GURL("http://amazon.com");
    private static final GURL TAB_URL_2 = new GURL("http://youtube.com");
    private static final GURL TAB_URL_3 = new GURL("http://facebook.com");

    private static final String TITLE1 = "title1";
    private static final String TITLE2 = "title2";
    private static final String TITLE3 = "title3";
    private static final GURL URL1 = JUnitTestGURLs.URL_1;
    private static final GURL URL2 = JUnitTestGURLs.URL_2;
    private static final GURL URL3 = JUnitTestGURLs.URL_3;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock ObservableSupplier<TabModelOrchestrator> mTabModelOrchestratorSupplier;
    @Mock TabModelOrchestrator mTabModelOrchestrator;
    @Mock TabPersistentStore mTabPersistentStore;
    @Mock ActivityManager mActivityManager;
    @Mock ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock ModalDialogManager mModalDialogManager;
    @Mock ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock Supplier<DesktopWindowStateManager> mDesktopWindowStateManagerSupplier;
    @Mock DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock AppHeaderState mAppHeaderState;

    @Mock TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock TabGroupSyncService mTabGroupSyncService;
    @Mock Profile mProfile;
    @Mock Profile mIncognitoProfile;
    @Mock ProfileProvider mProfileProvider;
    @Mock MismatchedIndicesHandler mMismatchedIndicesHandler;
    @Mock TabModelSelectorBase mTabModelSelector;
    @Mock TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock TabGroupModelFilter mTabGroupModelFilter;
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

    @Captor private ArgumentCaptor<Runnable> mOnSaveTabListRunnableCaptor;

    Activity mCurrentActivity;
    Activity[] mActivityPool;
    Activity[] mTabbedActivityPool;
    private TestMultiInstanceManagerApi31 mMultiInstanceManager;
    private int mNormalTabCount;
    private int mIncognitoTabCount;
    private ArrayList<Tab> mGroupedTabs;
    private TabGroupMetadata mTabGroupMetadata;

    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();

    private MultiInstanceManagerApi31 createMultiInstanceManager(Activity activity) {
        return new TestMultiInstanceManagerApi31(
                activity,
                mTabModelOrchestratorSupplier,
                mMultiWindowModeStateDispatcher,
                mActivityLifecycleDispatcher,
                mModalDialogManagerSupplier,
                mMenuOrKeyboardActionController,
                mDesktopWindowStateManagerSupplier);
    }

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
                Supplier<DesktopWindowStateManager> desktopWindowStateManagerSupplier) {
            super(
                    activity,
                    tabModelOrchestratorSupplier,
                    multiWindowModeStateDispatcher,
                    activityLifecycleDispatcher,
                    modalDialogManagerSupplier,
                    menuOrKeyboardActionController,
                    desktopWindowStateManagerSupplier);
        }

        private void createInstance(int instanceId, Activity activity) {
            MultiInstanceManagerApi31.writeUrl(instanceId, "https://id-" + instanceId + ".com");
            ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
            updateTasksWithoutDestroyingActivity(instanceId, activity);
            addInstanceInfo(instanceId, activity.getTaskId());
        }

        private void setAdjacentInstance(Activity activity) {
            mAdjacentInstance = activity;
        }

        private void addInstanceInfo(int instanceId, int taskId) {
            MultiInstanceManagerApi31.writeLastAccessedTime(instanceId);
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
                                false,
                                MultiInstanceManagerApi31.readLastAccessedTime(instanceId)));
            }
        }

        private void updateTasksWithoutDestroyingActivity(int instanceId, Activity activity) {
            if (instanceId == INVALID_WINDOW_ID) {
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
        void moveAndReparentTabToNewWindow(
                Tab tab,
                int instanceId,
                boolean preferNew,
                boolean openAdjacently,
                boolean addTrustedIntentExtras) {
            // Change the last parameter to false to bypass calling
            // IntentUtils.addTrustedIntentExtras() for testing.
            super.moveAndReparentTabToNewWindow(tab, instanceId, preferNew, openAdjacently, false);
        }

        @Override
        void moveAndReparentTabGroupToNewWindow(
                TabGroupMetadata tabGroupMetadata,
                int instanceId,
                boolean preferNew,
                boolean openAdjacently,
                boolean addTrustedIntentExtras) {
            // Change the last parameter to false to bypass calling
            // IntentUtils.addTrustedIntentExtras() for testing.
            super.moveAndReparentTabGroupToNewWindow(
                    tabGroupMetadata, instanceId, preferNew, openAdjacently, false);
        }

        @Override
        void setupIntentForTabReparenting(Tab tab, Intent intent, Runnable finalizeCallback) {}

        @Override
        void setupIntentForGroupReparenting(
                TabGroupMetadata tabGroupMetadata, Intent intent, Runnable finalizeCallback) {}

        @Override
        void beginReparentingTab(
                Tab tab, Intent intent, Bundle startActivityOptions, Runnable finalizeCallback) {}

        @Override
        void beginReparentingTabGroup(TabGroupMetadata tabGroupMetadata, Intent intent) {}
    }

    @Before
    public void setUp() {
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(any())).thenReturn(true);

        when(mTab1.getId()).thenReturn(TAB_ID_1);
        when(mTab2.getId()).thenReturn(TAB_ID_2);
        when(mTab3.getId()).thenReturn(TAB_ID_3);
        when(mTab1.getUrl()).thenReturn(TAB_URL_1);
        when(mTab2.getUrl()).thenReturn(TAB_URL_2);
        when(mTab3.getUrl()).thenReturn(TAB_URL_3);
        when(mTab1.getTabGroupId()).thenReturn(Token.createRandom());
        mGroupedTabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));
        mTabGroupMetadata =
                TabGroupMetadataExtractor.extractTabGroupMetadata(
                        mGroupedTabs, INSTANCE_ID_1, mTab1.getId(), /* isGroupShared= */ false);

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
        when(mTabModelOrchestrator.getTabPersistentStore()).thenReturn(mTabPersistentStore);

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
                    public TabModelSelector buildTabbedSelector(
                            Context context,
                            ModalDialogManager modalDialogManager,
                            OneshotSupplier<ProfileProvider> profileProviderSupplier,
                            TabCreatorManager tabCreatorManager,
                            NextTabPolicySupplier nextTabPolicySupplier) {
                        return new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null);
                    }

                    @Override
                    public Pair<TabModelSelector, Destroyable> buildHeadlessSelector(
                            @WindowId int windowId, Profile profile) {
                        return Pair.create(
                                new MockTabModelSelector(
                                        mProfile,
                                        mIncognitoProfile,
                                        /* tabCount= */ 0,
                                        /* incognitoTabCount= */ 0,
                                        /* delegate= */ null),
                                () -> {});
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
                                mDesktopWindowStateManagerSupplier));
        ApplicationStatus.setCachingEnabled(true);
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

        when(mDesktopWindowStateManagerSupplier.get()).thenReturn(mDesktopWindowStateManager);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(mAppHeaderState);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(anyBoolean()))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mNormalTabModel);
        when(mNormalTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
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
    public void testAllocInstanceId_reachesMaximum() {
        assertTrue(mMultiInstanceManager.mMaxInstances < mActivityPool.length);
        int index = 0;
        for (; index < mMultiInstanceManager.mMaxInstances; ++index) {
            assertEquals(index, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[index]));
        }
        assertEquals(
                INVALID_WINDOW_ID, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[index]));

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
    public void testAllocInstanceId_removeTaskOnRecentScreen() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));

        removeTaskOnRecentsScreen(mActivityTask57);

        // New instantiation picks up the smallest available ID.
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask59));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MULTI_INSTANCE_APPLICATION_STATUS_CLEANUP)
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
                                mModalDialogManager,
                                mProfileProviderSupplier,
                                null,
                                null,
                                mMismatchedIndicesHandler,
                                index);
        int instanceId = pair.first;
        assertEquals(0, instanceId);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.MULTI_INSTANCE_APPLICATION_STATUS_CLEANUP)
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
                                mModalDialogManager,
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
    public void testAllocInstanceId_assignPassedInstanceID() {
        // Take always the the passed ID if valid. This can be from switcher UI, explicitly
        // chosen by a user.
        assertEquals(PASSED_ID_2, allocInstanceIndex(PASSED_ID_2, mActivityTask58));
    }

    @Test
    public void testAllocInstanceId_ignoreWrongPassedInstanceID() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));

        // Go through the ordinary allocation logic if the passed ID is out of range.
        assertEquals(1, allocInstanceIndex(10, mActivityTask59));
    }

    @Test
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
    public void testAllocInstance_pickMruInstance() throws InterruptedException {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));

        removeTaskOnRecentsScreen(mActivityTask57);
        removeTaskOnRecentsScreen(mActivityTask58);

        // New instantiation picks up the most recently used one.
        MultiInstanceManagerApi31.writeLastAccessedTime(1);
        // These two writes can often use the same timestamp, and cause the result to be random.
        // Wait for the next millisecond to guarantee this doesn't happen.
        mFakeTimeTestRule.advanceMillis(1);
        MultiInstanceManagerApi31.writeLastAccessedTime(2); // Accessed most recently.

        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask59));
        removeTaskOnRecentsScreen(mActivityTask59);

        MultiInstanceManagerApi31.writeLastAccessedTime(1); // instance ID 1 is now the MRU.
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask60));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DISABLE_INSTANCE_LIMIT)
    public void testGetInstanceInfo_closesInstancesOlderThanSixMonths() {
        MultiWindowTestUtils.enableMultiInstance();
        // Setting up two additional Multi-instance managers; mMultiInstanceManager already exists.
        MultiInstanceManagerApi31 multiInstanceManager1 =
                createMultiInstanceManager(mActivityTask57);
        MultiInstanceManagerApi31 multiInstanceManager2 =
                createMultiInstanceManager(mActivityTask58);

        // Current activity is mActivityTask56, managed by mMultiInstanceManager.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        assertEquals(3, mMultiInstanceManager.getInstanceInfo().size());

        // Advancing time by well over six months.
        mFakeTimeTestRule.advanceMillis(MultiInstanceManagerApi31.SIX_MONTHS_MS + 5000000);
        // Closing the two other instances that are not managing the current activity.
        assertEquals(1, mMultiInstanceManager.getInstanceInfo().size());
        verify(mMultiInstanceManager, times(2)).closeInstance(anyInt(), anyInt());
    }

    @Test
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
    public void testCurrentInstanceId() {
        // Ensure the single instance at non-zero position is handled okay.
        int expected = 2;
        assertEquals(expected, allocInstanceIndex(expected, mActivityTask56));
        int id = mMultiInstanceManager.getCurrentInstanceId();
        assertEquals("Current instanceId is not as expected", expected, id);
    }

    @Test
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

        ApplicationStatus.onStateChangeForTesting(mCurrentActivity, ActivityState.CREATED);
        MultiInstanceManagerApi31 multiInstanceManager =
                new MultiInstanceManagerApi31(
                        mCurrentActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mModalDialogManagerSupplier,
                        mMenuOrKeyboardActionController,
                        mDesktopWindowStateManagerSupplier);
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

        ApplicationStatus.onStateChangeForTesting(mCurrentActivity, ActivityState.CREATED);
        MultiInstanceManagerApi31 multiInstanceManager =
                new MultiInstanceManagerApi31(
                        mCurrentActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mModalDialogManagerSupplier,
                        mMenuOrKeyboardActionController,
                        mDesktopWindowStateManagerSupplier);
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

        ApplicationStatus.onStateChangeForTesting(mCurrentActivity, ActivityState.CREATED);
        MultiInstanceManagerApi31 multiInstanceManager =
                new MultiInstanceManagerApi31(
                        mCurrentActivity,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mModalDialogManagerSupplier,
                        mMenuOrKeyboardActionController,
                        mDesktopWindowStateManagerSupplier);
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

    @Test
    public void testRemoveInstanceInfo() {
        int index = 1;
        String urlKey = MultiInstanceManagerApi31.urlKey(index);
        ChromeSharedPreferences.getInstance().writeString(urlKey, "");
        String titleKey = MultiInstanceManagerApi31.titleKey(index);
        ChromeSharedPreferences.getInstance().writeString(titleKey, "");
        String tabCountKey = MultiInstanceManagerApi31.tabCountKey(index);
        ChromeSharedPreferences.getInstance().writeInt(tabCountKey, 1);
        String tabCountForRelaunch = MultiInstanceManagerApi31.tabCountForRelaunchKey(index);
        ChromeSharedPreferences.getInstance().writeInt(tabCountForRelaunch, 1);
        String incognitoTabCountKey = MultiInstanceManagerApi31.incognitoTabCountKey(index);
        ChromeSharedPreferences.getInstance().writeInt(incognitoTabCountKey, 1);
        String incognitoSelectedKey = MultiInstanceManagerApi31.incognitoSelectedKey(index);
        ChromeSharedPreferences.getInstance().writeBoolean(incognitoSelectedKey, false);
        String lastAccessedTimeKey = MultiInstanceManagerApi31.lastAccessedTimeKey(index);
        ChromeSharedPreferences.getInstance().writeLong(lastAccessedTimeKey, 1);

        MultiInstanceManagerApi31.removeInstanceInfo(index);
        assertFalse(
                "Shared preference key should be removed.",
                ChromeSharedPreferences.getInstance().contains(urlKey));
        assertFalse(
                "Shared preference key should be removed.",
                ChromeSharedPreferences.getInstance().contains(titleKey));
        assertFalse(
                "Shared preference key should be removed.",
                ChromeSharedPreferences.getInstance().contains(tabCountKey));
        assertFalse(
                "Shared preference key should be removed.",
                ChromeSharedPreferences.getInstance().contains(tabCountForRelaunch));
        assertFalse(
                "Shared preference key should be removed.",
                ChromeSharedPreferences.getInstance().contains(incognitoTabCountKey));
        assertFalse(
                "Shared preference key should be removed.",
                ChromeSharedPreferences.getInstance().contains(incognitoSelectedKey));
        assertFalse(
                "Shared preference key should be removed.",
                ChromeSharedPreferences.getInstance().contains(lastAccessedTimeKey));
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
                                mModalDialogManager,
                                mProfileProviderSupplier,
                                null,
                                null,
                                mMismatchedIndicesHandler,
                                index);
        if (pair == null) return INVALID_WINDOW_ID;

        int instanceId = pair.first;
        mMultiInstanceManager.createInstance(instanceId, activity);
        MultiInstanceManagerApi31.updateTaskMap(instanceId, activity.getTaskId());

        // Store minimal data to get the instance recognized.
        MultiInstanceManagerApi31.writeUrl(instanceId, "url" + instanceId);
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
        mMultiInstanceManager.updateTasksWithoutDestroyingActivity(
                INVALID_WINDOW_ID, activityForTask);
        destroyActivity(activityForTask);
    }

    private void removeTaskWithoutDestroyingActivity(Activity activityForTask) {
        mMultiInstanceManager.updateTasksWithoutDestroyingActivity(
                INVALID_WINDOW_ID, activityForTask);
    }

    // Simulate only an activity gets destroyed, leaving everything intact.
    private void closeInstanceOnly(Activity activity, int ignored) {
        destroyActivity(activity);
    }

    private void destroyActivity(Activity activity) {
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.DESTROYED);
    }

    private void setupTwoInstances() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        MultiWindowTestUtils.enableMultiInstance();
        // Allocate and create two instances.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62, true));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63, true));
        assertEquals(2, mMultiInstanceManager.getInstanceInfo().size());
    }

    private void setupMaxInstances() {
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
    }

    @Test
    public void testMoveTabToNewWindow_calledWithDesiredParameters() {
        setupTwoInstances();

        // Action
        mMultiInstanceManager.moveTabToNewWindow(mTab1);

        // Verify the call is made with desired parameters. The moveAndReparentTabToNewWindow method
        // is validated in integration test here
        // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/javatests/src/org/chromium/chrome/browser/multiwindow/MultiWindowIntegrationTest.java
        verify(mMultiInstanceManager, times(1))
                .moveAndReparentTabToNewWindow(
                        any(), eq(INVALID_WINDOW_ID), eq(true), eq(false), eq(true));
    }

    @Test
    public void testMoveTabGroupToNewWindow_calledWithDesiredParameters() {
        setupTwoInstances();

        // Action
        mMultiInstanceManager.moveTabGroupToNewWindow(mTabGroupMetadata);

        // Verify
        verify(mMultiInstanceManager, times(1))
                .moveAndReparentTabGroupToNewWindow(
                        eq(mTabGroupMetadata),
                        eq(INVALID_WINDOW_ID),
                        eq(true),
                        eq(false),
                        eq(true));
    }

    @Test
    public void testMoveTabToNewWindow_BeyondMaxWindows_CallsOnly_OpenNewWindow() {
        setupMaxInstances();

        // Action
        mMultiInstanceManager.moveTabToNewWindow(mTab1);

        // Verify only openNewWindow is called and moveAndReparentTabToNewWindow is not called.
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabToNewWindow(
                        any(), eq(INVALID_WINDOW_ID), eq(true), eq(false), eq(true));
        verify(mMultiInstanceManager, times(1)).openNewWindow(any());
    }

    @Test
    public void testMoveTabGroupToNewWindow_BeyondMaxWindows_CallsOnly_OpenNewWindow() {
        setupMaxInstances();

        // Action
        mMultiInstanceManager.moveTabGroupToNewWindow(mTabGroupMetadata);

        // Verify only openNewWindow is called and moveAndReparentTabToNewWindow is not called.
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabGroupToNewWindow(
                        any(), eq(INVALID_WINDOW_ID), eq(true), eq(false), eq(true));
        verify(mMultiInstanceManager, times(1)).openNewWindow(any());
    }

    @Test
    public void testMoveTabToCurrentWindow_calledWithDesiredParameters() {
        setupTwoInstances();

        // Action
        int tabAtIndex = 0;
        mMultiInstanceManager.moveTabToWindow(mTabbedActivityTask63, mTab1, tabAtIndex);

        // Verify moveTabAction and getCurrentInstanceInfo are each called once.
        verify(mMultiInstanceManager, times(1)).moveTabAction(any(), eq(mTab1), eq(tabAtIndex));
        verify(mMultiInstanceManager, times(1)).getInstanceInfoFor(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_GROUP_DRAG_DROP_ANDROID)
    public void testMoveTabGroupToCurrentWindow_calledWithDesiredParameters() {
        setupTwoInstances();

        // Action
        int tabAtIndex = 0;
        mMultiInstanceManager.moveTabGroupToWindow(
                mTabbedActivityTask63, mTabGroupMetadata, tabAtIndex);

        // Verify moveTabGroupAction and getCurrentInstanceInfo are each called once.
        verify(mMultiInstanceManager, times(1))
                .moveTabGroupAction(any(), eq(mTabGroupMetadata), eq(tabAtIndex));
        verify(mMultiInstanceManager, times(1)).getInstanceInfoFor(any());
    }

    @Test
    public void testMoveTabAction_WithTabIndex_success() {
        setupTwoInstances();

        // Action
        InstanceInfo info = mMultiInstanceManager.getInstanceInfoFor(mTabbedActivityTask63);
        mMultiInstanceManager.moveTabAction(info, mTab1, /* tabAtIndex= */ 0);

        // Verify reparentTabToRunningActivity is called once.
        verify(mMultiInstanceManager, times(1))
                .reparentTabToRunningActivity(any(), eq(mTab1), eq(0));
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabToNewWindow(
                        eq(mTab1), eq(INVALID_WINDOW_ID), eq(false), eq(true), eq(true));
    }

    @Test
    public void testMoveTabGroupAction_WithTabIndex_success() {
        setupTwoInstances();

        // Action
        InstanceInfo info = mMultiInstanceManager.getInstanceInfoFor(mTabbedActivityTask63);
        mMultiInstanceManager.moveTabGroupAction(info, mTabGroupMetadata, /* startIndex= */ 0);

        // Verify reparentTabToRunningActivity is called once.
        verify(mMultiInstanceManager, times(1))
                .reparentTabGroupToRunningActivity(any(), eq(mTabGroupMetadata), eq(0));
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabGroupToNewWindow(
                        eq(mTabGroupMetadata),
                        eq(INVALID_WINDOW_ID),
                        eq(false),
                        eq(true),
                        eq(true));
    }

    @Test
    public void testMoveTabAction_WithNonExistentInstance_success() {
        setupTwoInstances();

        // Action
        InstanceInfo info =
                new InstanceInfo(
                        NON_EXISTENT_INSTANCE_ID,
                        NON_EXISTENT_INSTANCE_ID,
                        InstanceInfo.Type.ADJACENT,
                        "https://id-4.com",
                        "",
                        0,
                        0,
                        false,
                        0);
        mMultiInstanceManager.moveTabAction(info, mTab1, /* tabAtIndex= */ 0);

        // Verify moveAndReparentTabToNewWindow is called made with desired parameters once. The
        // method is validated in integration test here
        // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/javatests/src/org/chromium/chrome/browser/multiwindow/MultiWindowIntegrationTest.java.
        // Also reparentTabToRunningActivity is not called.
        verify(mMultiInstanceManager, times(1))
                .moveAndReparentTabToNewWindow(
                        eq(mTab1), eq(NON_EXISTENT_INSTANCE_ID), eq(false), eq(true), eq(true));
        verify(mMultiInstanceManager, times(0))
                .reparentTabToRunningActivity(any(), eq(mTab1), eq(0));
    }

    @Test
    public void testMoveTabGroupAction_WithNonExistentInstance_success() {
        setupTwoInstances();

        // Action
        InstanceInfo info =
                new InstanceInfo(
                        NON_EXISTENT_INSTANCE_ID,
                        NON_EXISTENT_INSTANCE_ID,
                        InstanceInfo.Type.ADJACENT,
                        "https://id-4.com",
                        "",
                        0,
                        0,
                        false,
                        0);
        mMultiInstanceManager.moveTabGroupAction(info, mTabGroupMetadata, /* startIndex= */ 0);

        // Verify moveAndReparentTabToNewWindow is called made with desired parameters once. The
        // method is validated in integration test here
        // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/javatests/src/org/chromium/chrome/browser/multiwindow/MultiWindowIntegrationTest.java.
        // Also reparentTabToRunningActivity is not called.
        verify(mMultiInstanceManager, times(1))
                .moveAndReparentTabGroupToNewWindow(
                        eq(mTabGroupMetadata),
                        eq(NON_EXISTENT_INSTANCE_ID),
                        eq(false),
                        eq(true),
                        eq(true));
        verify(mMultiInstanceManager, times(0))
                .reparentTabGroupToRunningActivity(any(), eq(mTabGroupMetadata), eq(0));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_GROUP_DRAG_DROP_ANDROID)
    public void testReparentGroupToRunningActivity() {
        doTestReparentGroupToRunningActivity(/* isGroupShared= */ false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_GROUP_DRAG_DROP_ANDROID)
    public void testReparentGroupToRunningActivity_sharedTabGroup() {
        doTestReparentGroupToRunningActivity(/* isGroupShared= */ true);
    }

    @Test
    public void testCloseChromeWindowIfEmpty_closed() {
        XrUtils.setXrDeviceForTesting(true);
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
    public void testCleanupSyncedTabGroupsIfOnlyInstance() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});

        allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask62, true);
        mMultiInstanceManager.cleanupSyncedTabGroupsIfOnlyInstance(mTabModelSelector);
        verify(mTabGroupSyncService).getAllGroupIds();
        reset(mTabGroupSyncService);

        allocInstanceIndex(INSTANCE_ID_2, mTabbedActivityTask63, true);
        mMultiInstanceManager.cleanupSyncedTabGroupsIfOnlyInstance(mTabModelSelector);
        verifyNoInteractions(mTabGroupSyncService);
    }

    @Test
    @Config(sdk = 30)
    public void testOpenInstance_TaskHasRunningActivity() {
        doTestOpenInstanceWithValidTask(/* isActivityAlive= */ true);
    }

    @Test
    @Config(sdk = 30)
    public void testOpenInstance_TaskHasNoRunningActivity() {
        doTestOpenInstanceWithValidTask(/* isActivityAlive= */ false);
    }

    @Test
    public void testWriteLastAccessedTime_InstanceCreation() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        MultiWindowTestUtils.enableMultiInstance();

        // Simulate creation of activity |mTabbedActivityTask62| with index 0 and
        // |mTabbedActivityTask63| with index 1.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        // Advancing time by at least 1ms apart to record different instance access times.
        mFakeTimeTestRule.advanceMillis(1);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));

        long accessTime0 = MultiInstanceManagerApi31.readLastAccessedTime(0);
        long accessTime1 = MultiInstanceManagerApi31.readLastAccessedTime(1);

        InstanceInfo info0 = mMultiInstanceManager.getInstanceInfoFor(mTabbedActivityTask62);
        InstanceInfo info1 = mMultiInstanceManager.getInstanceInfoFor(mTabbedActivityTask63);

        // Verify the lastAccessedTime for both instances.
        assertEquals(
                "InstanceInfo.lastAccessedTime for instance0 is incorrect.",
                accessTime0,
                info0.lastAccessedTime);
        assertEquals(
                "InstanceInfo.lastAccessedTime for instance1 is incorrect.",
                accessTime1,
                info1.lastAccessedTime);
        assertTrue(
                "Access time for instance0 should be older than access time for instance1.",
                accessTime0 < accessTime1);
    }

    @Test
    public void testWriteLastAccessedTime_OnTopResumedActivityChanged() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        MultiWindowTestUtils.enableMultiInstance();

        // Setup instance for |mTabbedActivityTask62| with index 0, make it the top resumed
        // activity.
        MultiInstanceManagerApi31 multiInstanceManager0 =
                new TestMultiInstanceManagerApi31(
                        mTabbedActivityTask62,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mModalDialogManagerSupplier,
                        mMenuOrKeyboardActionController,
                        mDesktopWindowStateManagerSupplier);
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        multiInstanceManager0.initialize(0, mTabbedActivityTask62.getTaskId());
        multiInstanceManager0.onTopResumedActivityChanged(true);
        long instance0CreationTime = MultiInstanceManagerApi31.readLastAccessedTime(0);

        // Setup instance for |mTabbedActivityTask63| with index 1, make it the top resumed
        // activity.
        MultiInstanceManagerApi31 multiInstanceManager1 =
                new TestMultiInstanceManagerApi31(
                        mTabbedActivityTask63,
                        mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher,
                        mActivityLifecycleDispatcher,
                        mModalDialogManagerSupplier,
                        mMenuOrKeyboardActionController,
                        mDesktopWindowStateManagerSupplier);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));
        multiInstanceManager1.initialize(1, mTabbedActivityTask63.getTaskId());
        multiInstanceManager0.onTopResumedActivityChanged(false);
        multiInstanceManager1.onTopResumedActivityChanged(true);
        long instance1CreationTime = MultiInstanceManagerApi31.readLastAccessedTime(1);
        // Advance time by 1ms to record a different access time for the instances when the top
        // resumed activity changes.
        mFakeTimeTestRule.advanceMillis(1);
        // Resume instance0, so it becomes the top resumed activity.
        multiInstanceManager0.onTopResumedActivityChanged(true);
        multiInstanceManager1.onTopResumedActivityChanged(false);

        // Verify the lastAccessedTime for both instances.
        long accessTime0 = MultiInstanceManagerApi31.readLastAccessedTime(0);
        long accessTime1 = MultiInstanceManagerApi31.readLastAccessedTime(1);

        assertTrue(
                "Access time for instance0 is not updated.", accessTime0 > instance0CreationTime);
        assertTrue(
                "Access time for instance1 is not updated.", accessTime1 > instance1CreationTime);
    }

    @Test
    public void launchIntentInMaybeClosedWindow_NewWindow() {
        Intent intent = new Intent();
        MultiInstanceManagerApi31.launchIntentInUnknown(
                mTabbedActivityTask62, intent, INSTANCE_ID_2);
        verify(mTabbedActivityTask62).startActivity(intent, null);
        assertEquals(
                INSTANCE_ID_2,
                intent.getIntExtra(IntentHandler.EXTRA_WINDOW_ID, INVALID_WINDOW_ID));
    }

    @Test
    public void launchIntentInMaybeClosedWindow_ExistingWindow() {
        assertEquals(INSTANCE_ID_1, allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask63, true));
        Intent intent = new Intent();
        MultiInstanceManagerApi31.launchIntentInUnknown(
                mTabbedActivityTask62, intent, INSTANCE_ID_1);
        verify(mTabbedActivityTask63).onNewIntent(intent);
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
                                mDesktopWindowStateManagerSupplier));

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
        }

        // Try to restore the instance in task |taskId63|, from |mTabbedActivityTask62|.
        multiInstanceManager.openInstance(1, taskId63);

        if (isActivityAlive) {
            // If |mTabbedActivityTask63| is alive, verify that its instance was restored in the
            // existing task by bringing it to the foreground.
            verify(mActivityManager).moveTaskToFront(taskId63, 0);
            verify(mTabbedActivityTask62, never()).startActivity(any());
            verify(appTask63, never()).finishAndRemoveTask();
        } else {
            // If |mTabbedActivityTask63| is not alive, verify that |mTabbedActivityTask62| starts a
            // new activity and finishes and removes the old task, and does not attempt to bring the
            // old task to the foreground.
            verify(mTabbedActivityTask62).startActivity(any());
            verify(appTask63).finishAndRemoveTask();
            verify(mActivityManager, never()).moveTaskToFront(taskId63, 0);
        }
    }

    private void doTestReparentGroupToRunningActivity(boolean isGroupShared) {
        // Setup.
        mMultiInstanceManager.mTestBuildInstancesList = true;
        ArrayList<Map.Entry<Integer, String>> tabIdsToUrls =
                new ArrayList<>(
                        List.of(
                                Map.entry(1, "https://www.amazon.com"),
                                Map.entry(2, "https://www.youtube.com"),
                                Map.entry(3, "https://www.facebook.com")));
        TabGroupMetadata tabGroupMetadata =
                new TabGroupMetadata(
                        /* rootId= */ -1,
                        /* selectedTabId= */ -1,
                        INSTANCE_ID_1,
                        /* tabGroupId= */ null,
                        tabIdsToUrls,
                        /* tabGroupColor= */ 0,
                        /* tabGroupTitle= */ null,
                        /* mhtmlTabTitle= */ null,
                        /* tabGroupCollapsed= */ false,
                        isGroupShared,
                        /* isIncognito= */ false);
        allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask62, true);

        // Trigger a group reparent.
        mMultiInstanceManager.reparentTabGroupToRunningActivity(
                mTabbedActivityTask62, tabGroupMetadata, /* tabAtIndex= */ 0);

        // Verify we pause the TabGroupSyncService to stop observing local changes.
        verify(mTabGroupSyncService).setLocalObservationMode(/* observeLocalChanges */ false);

        // Verify we pause the TabPersistentStore.
        verify(mTabPersistentStore).pauseSaveTabList();
        verify(mTabPersistentStore).resumeSaveTabList(mOnSaveTabListRunnableCaptor.capture());

        // Verify we only send the reparent intent after the Runnable runs.
        verify(mTabbedActivityTask62, never()).onNewIntent(any());
        mOnSaveTabListRunnableCaptor.getValue().run();
        verify(mTabbedActivityTask62).onNewIntent(any());

        // Verify we resume the TabGroupSyncService to begin observing local changes.
        verify(mTabGroupSyncService).setLocalObservationMode(/* observeLocalChanges */ true);
    }
}
