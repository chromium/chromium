// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.INVALID_TASK_ID;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseBooleanArray;
import android.util.SparseIntArray;

import androidx.test.core.app.ApplicationProvider;

import com.google.android.material.textfield.TextInputEditText;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDialog;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.CloseWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.SupportedProfileType;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
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
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
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
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Supplier;
import java.util.stream.Collectors;

/** Unit tests for {@link MultiInstanceManagerApi31}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
@DisableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
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
        private Intent mReparentingTabsIntent;

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
            setAppTaskIdsForTesting(mAppTaskIds);
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
            ChromeSharedPreferences.getInstance()
                    .writeInt(
                            ChromePreferenceKeys.MULTI_INSTANCE_PROFILE_TYPE.createKey(
                                    String.valueOf(instanceId)),
                            SupportedProfileType.REGULAR);
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
                                /* title= */ "",
                                /* customTitle= */ null,
                                /* tabCount= */ 0,
                                /* incognitoTabCount= */ 0,
                                /* isIncognitoSelected= */ false,
                                MultiInstanceManagerApi31.readLastAccessedTime(instanceId),
                                /* closedByUser= */ false));
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
        protected void installTabModelObserver() {}

        @Override
        public List<InstanceInfo> getInstanceInfo() {
            if (mTestBuildInstancesList) {
                return mTestInstanceInfos;
            }
            return super.getInstanceInfo();
        }

        @Override
        void moveAndReparentTabsToNewWindow(
                List<Tab> tabs,
                int instanceId,
                boolean preferNew,
                boolean openAdjacently,
                boolean addTrustedIntentExtras,
                @NewWindowAppSource int source) {
            // Change the last parameter to false to bypass calling
            // IntentUtils.addTrustedIntentExtras() for testing.
            super.moveAndReparentTabsToNewWindow(
                    tabs, instanceId, preferNew, openAdjacently, false, source);
        }

        @Override
        void moveAndReparentTabGroupToNewWindow(
                TabGroupMetadata tabGroupMetadata,
                int instanceId,
                boolean preferNew,
                boolean openAdjacently,
                boolean addTrustedIntentExtras,
                @NewWindowAppSource int source) {
            // Change the last parameter to false to bypass calling
            // IntentUtils.addTrustedIntentExtras() for testing.
            super.moveAndReparentTabGroupToNewWindow(
                    tabGroupMetadata, instanceId, preferNew, openAdjacently, false, source);
        }

        @Override
        void setupIntentForTabsReparenting(
                List<Tab> tabs, Intent intent, Runnable finalizeCallback) {}

        @Override
        void setupIntentForGroupReparenting(
                TabGroupMetadata tabGroupMetadata, Intent intent, Runnable finalizeCallback) {}

        @Override
        void beginReparentingTabs(
                List<Tab> tabs,
                Intent intent,
                @Nullable Bundle startActivityOptions,
                @Nullable Runnable finalizeCallback) {
            mReparentingTabsIntent = intent;
        }

        @Override
        void beginReparentingTabGroup(TabGroupMetadata tabGroupMetadata, Intent intent) {}

        Intent getReparentingTabsIntent() {
            return mReparentingTabsIntent;
        }
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
                        mTabGroupModelFilter,
                        mGroupedTabs,
                        INSTANCE_ID_1,
                        TAB_ID_1,
                        /* isGroupShared= */ false);

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
                            NextTabPolicySupplier nextTabPolicySupplier,
                            MultiInstanceManager multiInstanceManager) {
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
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.MULTI_INSTANCE_MAX_INSTANCE_LIMIT);
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        ApplicationStatus.destroyForJUnitTests();
        mMultiInstanceManager.mTestBuildInstancesList = false;
        ApplicationStatus.setCachingEnabled(false);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testAllocInstanceId_reachesMaximum() {
        assertTrue(mMultiInstanceManager.mMaxInstances < mActivityPool.length);
        int index = 0;
        for (; index < mMultiInstanceManager.mMaxInstances; ++index) {
            assertEquals(index, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[index]));
        }
        assertEquals(
                INVALID_WINDOW_ID, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[index]));

        // Activity ID 1 gets removed from memory.
        softCloseInstance(mActivityPool[1], 1);

        // We allocated max number of instances already. Activity Id 1 is was removed but
        // remains mapped to a task still alive. No more new allocation is possible.
        assertIsNewTask(TASK_ID_60);
        assertEquals(-1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask60));

        // New allocation becomes possible only after a task is gone.
        removeTaskOnRecentsScreen(mActivityPool[2]);
        assertIsNewTask(TASK_ID_61);
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask61));
    }

    @Test
    public void testAllocInstanceId_destroyedInstanceMappedBackToItsTask() {
        int index = 0;
        for (; index < mMultiInstanceManager.mMaxInstances; ++index) {
            assertEquals(index, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[index]));
        }

        softCloseInstance(mActivityPool[1], 1);

        // New instance is assigned the instance ID 1 again when the associated task is
        // brought foreground and attempts to recreate the activity.
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[1]));
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
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
                        PASSED_ID_INVALID, TASK_ID_57, false, SupportedProfileType.REGULAR);
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
                        PASSED_ID_INVALID, TASK_ID_57, false, SupportedProfileType.REGULAR);
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
    public void testAllocInstanceId_allowPassedInstanceIdValueGreaterThanMaxLimitValue() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));

        // Honor a passed ID with value greater than the max instance limit value. This is possible
        // after an instance limit downgrade.
        assertEquals(10, allocInstanceIndex(10, mActivityTask59));
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
    public void testAllocInstanceId_preferNew_atDowngradedInstanceLimit() {
        // Set initial instance limit and allocate ids for max instances.
        MultiWindowUtils.setMaxInstancesForTesting(3);
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));

        // Decrease instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(2);

        // Simulate deletion of instance0, so id=0 becomes available.
        MultiInstanceManagerApi31.removeInstanceInfo(0, CloseWindowAppSource.OTHER);

        // Trying to allocate a new instance with preferNew should fail.
        when(mActivityTask59.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        MultiInstanceManagerApi31 multiInstanceManager =
                createMultiInstanceManager(mActivityTask59);
        MultiInstanceManagerApi31.setAppTaskIdsForTesting(
                new HashSet<>(Arrays.asList(TASK_ID_57, TASK_ID_58, TASK_ID_59)));
        Pair<Integer, Integer> instanceIdInfo =
                multiInstanceManager.allocInstanceId(
                        PASSED_ID_INVALID,
                        TASK_ID_59,
                        /* preferNew= */ true,
                        SupportedProfileType.REGULAR);
        assertEquals(
                "Should not allocate valid instance id when at limit.",
                INVALID_WINDOW_ID,
                (int) instanceIdInfo.first);
        assertEquals(
                "Should return PREFER_NEW_INVALID_INSTANCE.",
                MultiWindowUtils.InstanceAllocationType.PREFER_NEW_INVALID_INSTANCE,
                (int) instanceIdInfo.second);
    }

    @Test
    public void testAllocInstanceId_preferNew_withInactiveInstance_allocatesNewId() {
        // Set initial instance limit and allocate ids for max instances.
        MultiWindowUtils.setMaxInstancesForTesting(3);
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));

        // Make one instance inactive.
        removeTaskOnRecentsScreen(mActivityTask57);

        // Now we have 2 active instances (0, 2) and 1 inactive instance (1).
        // Total instances = 3, max instances = 3.

        // Try to allocate a new instance with preferNew. This should succeed and allocate a new
        // instance ID outside of the mMaxInstances range as we only consider active instances.
        when(mActivityTask59.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        Pair<Integer, Integer> instanceIdInfo =
                mMultiInstanceManager.allocInstanceId(
                        PASSED_ID_INVALID,
                        TASK_ID_59,
                        /* preferNew= */ true,
                        SupportedProfileType.REGULAR);
        assertEquals("Should allocate a new instance id.", 3, (int) instanceIdInfo.first);
        assertEquals(
                "Should return PREFER_NEW_INSTANCE_NEW_TASK.",
                MultiWindowUtils.InstanceAllocationType.PREFER_NEW_INSTANCE_NEW_TASK,
                (int) instanceIdInfo.second);
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

        // Current activity is mActivityTask56, managed by mMultiInstanceManager.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        assertEquals(3, mMultiInstanceManager.getInstanceInfo().size());

        // Advancing time by well over six months.
        mFakeTimeTestRule.advanceMillis(MultiInstanceManagerApi31.SIX_MONTHS_MS + 5000000);
        // Closing the two other instances that are not managing the current activity.
        assertEquals(1, mMultiInstanceManager.getInstanceInfo().size());
        verify(mMultiInstanceManager, times(2)).closeWindow(anyInt(), anyInt());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS)
    public void testGetInstanceInfo_size_hardClosure() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        mMultiInstanceManager.setAdjacentInstance(mActivityTask57);

        assertEquals(3, mMultiInstanceManager.getInstanceInfo().size());

        // Removing a task from recent screen doesn't affect instance info list.
        removeTaskOnRecentsScreen(mActivityTask58);
        assertEquals(3, mMultiInstanceManager.getInstanceInfo().size());

        // Activity destroyed in the background due to memory constraint has no impact either.
        softCloseInstance(mActivityTask57, TASK_ID_57);
        assertEquals(3, mMultiInstanceManager.getInstanceInfo().size());

        // Closing an instance removes the entry.
        mMultiInstanceManager.closeWindow(1, CloseWindowAppSource.OTHER);
        assertEquals(2, mMultiInstanceManager.getInstanceInfo().size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS)
    public void testGetInstanceInfo_size_softClosure() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        mMultiInstanceManager.setAdjacentInstance(mActivityTask57);

        assertEquals(3, mMultiInstanceManager.getInstanceInfo().size());

        // Removing a task from recent screen doesn't affect instance info list.
        removeTaskOnRecentsScreen(mActivityTask58);
        assertEquals(3, mMultiInstanceManager.getInstanceInfo().size());

        // Trigger a soft closure on this window.
        softCloseInstance(mActivityTask57, TASK_ID_57);
        assertEquals(3, mMultiInstanceManager.getInstanceInfo().size());

        // Soft closing an instance does not remove the entry.
        mMultiInstanceManager.closeWindow(1, CloseWindowAppSource.WINDOW_MANAGER);
        assertEquals(3, mMultiInstanceManager.getInstanceInfo().size());
        for (InstanceInfo instanceInfo : mMultiInstanceManager.getInstanceInfo()) {
            if (instanceInfo.instanceId == 1) {
                assertTrue(instanceInfo.closedByUser);
                break;
            }
        }
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
    public void testGetInstanceInfo_filters() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        MultiWindowTestUtils.enableMultiInstance();

        // Instance 0: Active, Regular
        assertEquals(0, allocInstanceIndex(0, mTabbedActivityPool[0]));
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        MultiInstanceManagerApi31.profileTypeKey(0), SupportedProfileType.REGULAR);

        // Instance 1: Active, Incognito
        assertEquals(1, allocInstanceIndex(1, mTabbedActivityPool[1]));
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        MultiInstanceManagerApi31.profileTypeKey(1),
                        SupportedProfileType.OFF_THE_RECORD);

        // Instance 2: Inactive, Regular
        assertEquals(2, allocInstanceIndex(2, mTabbedActivityPool[2]));
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        MultiInstanceManagerApi31.profileTypeKey(2), SupportedProfileType.REGULAR);
        removeTaskOnRecentsScreen(mTabbedActivityPool[2]);

        // Instance 3: Inactive, Incognito
        assertEquals(3, allocInstanceIndex(3, mTabbedActivityPool[3]));
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        MultiInstanceManagerApi31.profileTypeKey(3),
                        SupportedProfileType.OFF_THE_RECORD);
        removeTaskOnRecentsScreen(mTabbedActivityPool[3]);

        // Test PersistedInstanceType.ANY
        List<InstanceInfo> allInstances =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY);
        assertEquals("ANY should return 4 instances", 4, allInstances.size());
        Set<Integer> allIds =
                allInstances.stream().map(i -> i.instanceId).collect(Collectors.toSet());
        assertTrue(allIds.containsAll(Arrays.asList(0, 1, 2, 3)));

        // Test PersistedInstanceType.ACTIVE
        List<InstanceInfo> activeInstances =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ACTIVE);
        assertEquals("ACTIVE should return 2 instances", 2, activeInstances.size());
        Set<Integer> activeIds =
                activeInstances.stream().map(i -> i.instanceId).collect(Collectors.toSet());
        assertTrue(activeIds.containsAll(Arrays.asList(0, 1)));

        // Test PersistedInstanceType.INACTIVE
        List<InstanceInfo> inactiveInstances =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE);
        assertEquals("INACTIVE should return 2 instances", 2, inactiveInstances.size());
        Set<Integer> inactiveIds =
                inactiveInstances.stream().map(i -> i.instanceId).collect(Collectors.toSet());
        assertTrue(inactiveIds.containsAll(Arrays.asList(2, 3)));

        // Test PersistedInstanceType.OFF_THE_RECORD
        List<InstanceInfo> otrInstances =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.OFF_THE_RECORD);
        assertEquals("OFF_THE_RECORD should return 2 instances", 2, otrInstances.size());
        Set<Integer> otrIds =
                otrInstances.stream().map(i -> i.instanceId).collect(Collectors.toSet());
        assertTrue(otrIds.containsAll(Arrays.asList(1, 3)));

        // Test PersistedInstanceType.REGULAR
        List<InstanceInfo> regularInstances =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.REGULAR);
        assertEquals("REGULAR should return 2 instances", 2, regularInstances.size());
        Set<Integer> regularIds =
                regularInstances.stream().map(i -> i.instanceId).collect(Collectors.toSet());
        assertTrue(regularIds.containsAll(Arrays.asList(0, 2)));

        // Test combined filter: ACTIVE and OFF_THE_RECORD
        List<InstanceInfo> activeOtrInstances =
                mMultiInstanceManager.getInstanceInfo(
                        PersistedInstanceType.ACTIVE | PersistedInstanceType.OFF_THE_RECORD);
        assertEquals(
                "ACTIVE | OFF_THE_RECORD should return 1 instance", 1, activeOtrInstances.size());
        assertEquals(1, activeOtrInstances.get(0).instanceId);

        // Test combined filter: ACTIVE and REGULAR
        List<InstanceInfo> activeRegularInstances =
                mMultiInstanceManager.getInstanceInfo(
                        PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR);
        assertEquals("ACTIVE | REGULAR should return 1 instance", 1, activeRegularInstances.size());
        assertEquals(0, activeRegularInstances.get(0).instanceId);
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

        final String customTitle = "My Custom Title";
        ChromeSharedPreferences.getInstance()
                .writeString(MultiInstanceManagerApi31.customTitleKey(INSTANCE_ID_1), customTitle);

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
        assertEquals(
                "Custom title should not change when tab changes.",
                customTitle,
                MultiInstanceManagerApi31.readCustomTitle(INSTANCE_ID_1));
    }

    @Test
    public void testRenameCallbackUpdatesCustomTitle() {
        Callback<Pair<Integer, String>> renameCallback =
                mMultiInstanceManager.getRenameCallbackForTesting();

        final String newTitle = "My Renamed Window";
        final int instanceId = 2;
        renameCallback.onResult(new Pair<>(instanceId, newTitle));

        assertEquals(
                "Custom title should be updated in SharedPreferences.",
                newTitle,
                MultiInstanceManagerApi31.readCustomTitle(instanceId));
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
        String customTitleKey = MultiInstanceManagerApi31.customTitleKey(index);
        ChromeSharedPreferences.getInstance().writeString(customTitleKey, "");
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
        String profileTypeKey = MultiInstanceManagerApi31.profileTypeKey(index);
        ChromeSharedPreferences.getInstance().writeInt(profileTypeKey, 1);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MultiInstanceManager.CLOSE_WINDOW_APP_SOURCE_HISTOGRAM,
                                CloseWindowAppSource.OTHER)
                        .build();

        MultiInstanceManagerApi31.removeInstanceInfo(index, CloseWindowAppSource.OTHER);
        histogramWatcher.assertExpected();
        assertFalse(
                "Shared preference key should be removed.",
                ChromeSharedPreferences.getInstance().contains(urlKey));
        assertFalse(
                "Shared preference key should be removed.",
                ChromeSharedPreferences.getInstance().contains(titleKey));
        assertFalse(
                "Shared preference key should be removed.",
                ChromeSharedPreferences.getInstance().contains(customTitleKey));
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
        assertFalse(
                "Shared preference key should be removed.",
                ChromeSharedPreferences.getInstance().contains(profileTypeKey));
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
        tabModelObserver.onFinishingTabClosure(tab, TabClosingSource.UNKNOWN);
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
                mMultiInstanceManager.allocInstanceId(
                        passedId, activity.getTaskId(), preferNew, SupportedProfileType.REGULAR);
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
    private void softCloseInstance(Activity activity, int ignored) {
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
                .openNewWindow(eq("Android.WindowManager.NewWindow"), eq(false), anyInt());
    }

    @Test
    public void testMoveSingleTabToNewWindow_calledWithDesiredParameters() {
        setupTwoInstances();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MultiInstanceManager.NEW_WINDOW_APP_SOURCE_HISTOGRAM,
                                NewWindowAppSource.MENU)
                        .build();
        // Action
        mMultiInstanceManager.moveTabsToNewWindow(
                Collections.singletonList(mTab1), NewWindowAppSource.MENU);

        // Verify the call is made with desired parameters. The moveAndReparentTabToNewWindow method
        // is validated in integration test here
        // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/javatests/src/org/chromium/chrome/browser/multiwindow/MultiWindowIntegrationTest.java
        verify(mMultiInstanceManager, times(1))
                .moveAndReparentTabsToNewWindow(
                        eq(Collections.singletonList(mTab1)),
                        eq(INVALID_WINDOW_ID),
                        eq(true),
                        eq(true),
                        eq(true),
                        anyInt());
        histogramWatcher.assertExpected();
    }

    @Test
    public void testMoveTabsToNewWindow_calledWithDesiredParameters() {
        setupTwoInstances();
        List<Tab> tabs = List.of(mTab1, mTab2);

        mMultiInstanceManager.moveTabsToNewWindow(tabs, NewWindowAppSource.KEYBOARD_SHORTCUT);

        verify(mMultiInstanceManager, times(1))
                .moveAndReparentTabsToNewWindow(
                        eq(tabs), eq(INVALID_WINDOW_ID), eq(true), eq(true), eq(true), anyInt());
    }

    @Test
    public void testMoveTabGroupToNewWindow_calledWithDesiredParameters() {
        setupTwoInstances();

        // Action
        mMultiInstanceManager.moveTabGroupToNewWindow(
                mTabGroupMetadata, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify
        verify(mMultiInstanceManager, times(1))
                .moveAndReparentTabGroupToNewWindow(
                        eq(mTabGroupMetadata),
                        eq(INVALID_WINDOW_ID),
                        eq(true),
                        eq(true),
                        eq(true),
                        anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testMoveTabsToNewWindow_opensAdjacently_WithRobustWindowManagementExperimental() {
        setupTwoInstances();
        List<Tab> tabs = List.of(mTab1, mTab2);

        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                true);

        when(mCurrentActivity.isInMultiWindowMode()).thenReturn(false);

        mMultiInstanceManager.moveTabsToNewWindow(tabs, NewWindowAppSource.KEYBOARD_SHORTCUT);

        Intent intent = mMultiInstanceManager.getReparentingTabsIntent();
        assertNotEquals("Intent should not be null.", null, intent);
        int flags = intent.getFlags();
        assertTrue(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT should be set.",
                (flags & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testMoveTabsToNewWindow_opensFullScreen_WithRobustWindowManagementExperimental() {
        setupTwoInstances();
        List<Tab> tabs = List.of(mTab1, mTab2);

        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                false);

        mMultiInstanceManager.moveTabsToNewWindow(tabs, NewWindowAppSource.KEYBOARD_SHORTCUT);

        Intent intent = mMultiInstanceManager.getReparentingTabsIntent();
        assertNotEquals("Intent should not be null.", null, intent);
        int flags = intent.getFlags();
        assertFalse(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT should not be set.",
                (flags & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testOpenInstance_opensAdjacently_WithRobustWindowManagementExperimental() {
        setupTwoInstances();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MultiInstanceManager.NEW_WINDOW_APP_SOURCE_HISTOGRAM,
                                NewWindowAppSource.WINDOW_MANAGER)
                        .build();
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                true);
        when(mCurrentActivity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        mMultiInstanceManager.openInstance(
                INSTANCE_ID_2, INVALID_TASK_ID, NewWindowAppSource.WINDOW_MANAGER);

        verify(mCurrentActivity).startActivity(intentCaptor.capture());
        histogramWatcher.assertExpected();
        Intent intent = intentCaptor.getValue();
        assertNotEquals("Intent should not be null.", null, intent);
        int flags = intent.getFlags();
        assertTrue(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT should be set.",
                (flags & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testOpenInstance_opensFullScreen_WithRobustWindowManagementExperimental() {
        setupTwoInstances();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MultiInstanceManager.NEW_WINDOW_APP_SOURCE_HISTOGRAM,
                                NewWindowAppSource.WINDOW_MANAGER)
                        .build();
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                false);
        when(mCurrentActivity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        mMultiInstanceManager.openInstance(
                INSTANCE_ID_2, INVALID_TASK_ID, NewWindowAppSource.WINDOW_MANAGER);

        verify(mCurrentActivity).startActivity(intentCaptor.capture());
        histogramWatcher.assertExpected();
        Intent intent = intentCaptor.getValue();
        assertNotEquals("Intent should not be null.", null, intent);
        int flags = intent.getFlags();
        assertFalse(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT should not be set.",
                (flags & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testMoveSingleTabToNewWindow_BeyondMaxWindows_CallsOnly_OpenNewWindow() {
        setupMaxInstances();

        // Action
        mMultiInstanceManager.moveTabsToNewWindow(
                Collections.singletonList(mTab1), NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify only openNewWindow is called and moveAndReparentTabToNewWindow is not called.
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabsToNewWindow(
                        eq(Collections.singletonList(mTab1)),
                        eq(INVALID_WINDOW_ID),
                        eq(true),
                        eq(false),
                        eq(true),
                        anyInt());
        verify(mMultiInstanceManager, times(1)).openNewWindow(any(), anyBoolean(), anyInt());
    }

    @Test
    public void testMoveTabsToNewWindow_BeyondMaxWindows_CallsOnly_OpenNewWindow() {
        setupMaxInstances();
        List<Tab> tabs = List.of(mTab1, mTab2);

        // Action
        mMultiInstanceManager.moveTabsToNewWindow(tabs, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify only openNewWindow is called and moveAndReparentTabsToNewWindow is not called.
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabsToNewWindow(
                        any(), anyInt(), anyBoolean(), anyBoolean(), anyBoolean(), anyInt());
        verify(mMultiInstanceManager, times(1)).openNewWindow(any(), anyBoolean(), anyInt());
    }

    @Test
    public void testMoveTabGroupToNewWindow_BeyondMaxWindows_CallsOnly_OpenNewWindow() {
        setupMaxInstances();

        // Action
        mMultiInstanceManager.moveTabGroupToNewWindow(
                mTabGroupMetadata, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify only openNewWindow is called and moveAndReparentTabToNewWindow is not called.
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabGroupToNewWindow(
                        any(), eq(INVALID_WINDOW_ID), eq(true), eq(false), eq(true), anyInt());
        verify(mMultiInstanceManager, times(1)).openNewWindow(any(), anyBoolean(), anyInt());
    }

    @Test
    public void testMoveSingleTabToCurrentWindow_calledWithDesiredParameters() {
        setupTwoInstances();
        InstanceInfo instanceInfo = mMultiInstanceManager.getInstanceInfoFor(mTabbedActivityTask63);
        clearInvocations(mMultiInstanceManager); // Clear getInstanceInfoFor call above.

        // Action
        int tabAtIndex = 0;
        mMultiInstanceManager.moveTabsToWindow(
                mTabbedActivityTask63, Collections.singletonList(mTab1), tabAtIndex);

        // Verify moveTabToWindow and getCurrentInstanceInfo are each called once.
        InOrder inOrderVerifier = inOrder(mMultiInstanceManager);
        inOrderVerifier
                .verify(mMultiInstanceManager, times(1))
                .moveTabsToWindow(
                        mTabbedActivityTask63, Collections.singletonList(mTab1), tabAtIndex);
        inOrderVerifier
                .verify(mMultiInstanceManager, times(1))
                .getInstanceInfoFor(mTabbedActivityTask63);
        inOrderVerifier
                .verify(mMultiInstanceManager, times(1))
                .moveTabsToWindow(
                        instanceInfo,
                        Collections.singletonList(mTab1),
                        tabAtIndex,
                        NewWindowAppSource.OTHER);
    }

    @Test
    public void testMoveTabsToCurrentWindow_calledWithDesiredParameters() {
        setupTwoInstances();
        InstanceInfo instanceInfo = mMultiInstanceManager.getInstanceInfoFor(mTabbedActivityTask63);
        clearInvocations(mMultiInstanceManager); // Clear getInstanceInfoFor call above.

        // Action
        List<Tab> tabs = List.of(mTab1, mTab2);
        int tabAtIndex = 0;
        mMultiInstanceManager.moveTabsToWindow(mTabbedActivityTask63, tabs, tabAtIndex);

        // Verify moveTabsToWindow and getCurrentInstanceInfo are each called once.
        InOrder inOrderVerifier = inOrder(mMultiInstanceManager);
        inOrderVerifier
                .verify(mMultiInstanceManager, times(1))
                .moveTabsToWindow(mTabbedActivityTask63, tabs, tabAtIndex);
        inOrderVerifier
                .verify(mMultiInstanceManager, times(1))
                .getInstanceInfoFor(mTabbedActivityTask63);
        inOrderVerifier
                .verify(mMultiInstanceManager, times(1))
                .moveTabsToWindow(instanceInfo, tabs, tabAtIndex, NewWindowAppSource.OTHER);
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
                .moveTabGroupToWindow(any(Activity.class), eq(mTabGroupMetadata), eq(tabAtIndex));
        verify(mMultiInstanceManager, times(1)).getInstanceInfoFor(any());
    }

    @Test
    public void testMoveSingleTabToWindow_WithTabIndex_success() {
        setupTwoInstances();

        // Action
        InstanceInfo info = mMultiInstanceManager.getInstanceInfoFor(mTabbedActivityTask63);
        mMultiInstanceManager.moveTabsToWindow(
                info,
                Collections.singletonList(mTab1),
                /* tabAtIndex= */ 0,
                NewWindowAppSource.OTHER);

        // Verify reparentTabToRunningActivity is called once.
        verify(mMultiInstanceManager, times(1))
                .reparentTabsToRunningActivity(any(), eq(Collections.singletonList(mTab1)), eq(0));
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabsToNewWindow(
                        eq(Collections.singletonList(mTab1)),
                        eq(INVALID_WINDOW_ID),
                        eq(false),
                        eq(true),
                        eq(true),
                        anyInt());
    }

    @Test
    public void testMoveTabsToWindow_WithTabIndex_success() {
        setupTwoInstances();
        List<Tab> tabs = List.of(mTab1, mTab2);
        // Action
        InstanceInfo info = mMultiInstanceManager.getInstanceInfoFor(mTabbedActivityTask63);
        mMultiInstanceManager.moveTabsToWindow(
                info, tabs, /* tabAtIndex= */ 0, NewWindowAppSource.OTHER);

        // Verify reparentTabToRunningActivity is called once.
        verify(mMultiInstanceManager, times(1))
                .reparentTabsToRunningActivity(any(), eq(tabs), eq(0));
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabsToNewWindow(
                        eq(tabs), eq(INVALID_WINDOW_ID), eq(false), eq(true), eq(true), anyInt());
    }

    @Test
    public void testMoveTabGroupAction_WithTabIndex_success() {
        setupTwoInstances();

        // Action
        InstanceInfo info = mMultiInstanceManager.getInstanceInfoFor(mTabbedActivityTask63);
        mMultiInstanceManager.moveTabGroupToWindow(
                info, mTabGroupMetadata, /* startIndex= */ 0, NewWindowAppSource.OTHER);

        // Verify reparentTabToRunningActivity is called once.
        verify(mMultiInstanceManager, times(1))
                .reparentTabGroupToRunningActivity(any(), eq(mTabGroupMetadata), eq(0));
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabGroupToNewWindow(
                        eq(mTabGroupMetadata),
                        eq(INVALID_WINDOW_ID),
                        eq(false),
                        eq(true),
                        eq(true),
                        anyInt());
    }

    @Test
    public void testMoveSingleTabToWindow_WithNonExistentInstance_success() {
        setupTwoInstances();

        // Action
        InstanceInfo info =
                new InstanceInfo(
                        /* instanceId= */ NON_EXISTENT_INSTANCE_ID,
                        /* taskId= */ NON_EXISTENT_INSTANCE_ID,
                        InstanceInfo.Type.ADJACENT,
                        "https://id-4.com",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 0,
                        /* closedByUser= */ false);
        mMultiInstanceManager.moveTabsToWindow(
                info,
                Collections.singletonList(mTab1),
                /* tabAtIndex= */ 0,
                NewWindowAppSource.OTHER);

        // Verify moveAndReparentTabToNewWindow is called made with desired parameters once. The
        // method is validated in integration test here
        // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/javatests/src/org/chromium/chrome/browser/multiwindow/MultiWindowIntegrationTest.java.
        // Also reparentTabsToRunningActivity is not called.
        verify(mMultiInstanceManager, times(1))
                .moveAndReparentTabsToNewWindow(
                        eq(Collections.singletonList(mTab1)),
                        eq(NON_EXISTENT_INSTANCE_ID),
                        eq(false),
                        eq(false),
                        eq(true),
                        anyInt());
        verify(mMultiInstanceManager, times(0))
                .reparentTabsToRunningActivity(any(), eq(Collections.singletonList(mTab1)), eq(0));
    }

    @Test
    public void testMoveTabsToWindow_WithNonExistentInstance_success() {
        setupTwoInstances();
        List<Tab> tabs = List.of(mTab1, mTab2);

        // Action
        InstanceInfo info =
                new InstanceInfo(
                        /* instanceId= */ NON_EXISTENT_INSTANCE_ID,
                        /* taskId= */ NON_EXISTENT_INSTANCE_ID,
                        InstanceInfo.Type.ADJACENT,
                        "https://id-4.com",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 0,
                        /* closedByUser= */ false);
        mMultiInstanceManager.moveTabsToWindow(
                info, tabs, /* tabAtIndex= */ 0, NewWindowAppSource.OTHER);

        // Verify
        verify(mMultiInstanceManager, times(1))
                .moveAndReparentTabsToNewWindow(
                        eq(tabs),
                        eq(NON_EXISTENT_INSTANCE_ID),
                        eq(false),
                        eq(false),
                        eq(true),
                        anyInt());
        verify(mMultiInstanceManager, times(0))
                .reparentTabsToRunningActivity(any(), eq(tabs), eq(0));
    }

    @Test
    public void testMoveTabGroupAction_WithNonExistentInstance_success() {
        setupTwoInstances();

        // Action
        InstanceInfo info =
                new InstanceInfo(
                        /* instanceId= */ NON_EXISTENT_INSTANCE_ID,
                        /* taskId= */ NON_EXISTENT_INSTANCE_ID,
                        InstanceInfo.Type.ADJACENT,
                        "https://id-4.com",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 0,
                        /* closedByUser= */ false);
        mMultiInstanceManager.moveTabGroupToWindow(
                info, mTabGroupMetadata, /* startIndex= */ 0, NewWindowAppSource.OTHER);

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
                        eq(true),
                        anyInt());
        verify(mMultiInstanceManager, times(0))
                .reparentTabGroupToRunningActivity(any(), eq(mTabGroupMetadata), eq(0));
    }

    @Test
    public void testMoveTabsToWindowAndMergeToDest_success() {
        setupTwoInstances();
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTab1.getTabGroupId()).thenReturn(null);
        when(mTab2.getTabGroupId()).thenReturn(null);
        // Action
        InstanceInfo info = mMultiInstanceManager.getInstanceInfoFor(mTabbedActivityTask63);
        mMultiInstanceManager.moveTabsToWindowAndMergeToDest(info, tabs, /* destTabId= */ 3);

        // Verify reparentTabsToRunningActivityAndMergeToDest is called once.
        verify(mMultiInstanceManager, times(1))
                .reparentTabsToRunningActivityAndMergeToDest(any(), eq(tabs), eq(3));
        verify(mMultiInstanceManager, times(0))
                .moveAndReparentTabsToNewWindow(
                        eq(tabs), eq(INVALID_WINDOW_ID), eq(false), eq(true), eq(true), anyInt());
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
        DeviceInfo.setIsXrForTesting(true);
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
                .closeWindow(anyInt(), eq(CloseWindowAppSource.NO_TABS_IN_WINDOW));
    }

    @Test
    public void testCloseChromeWindowIfEmpty() {
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
                .closeWindow(anyInt(), eq(CloseWindowAppSource.NO_TABS_IN_WINDOW));
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
        multiInstanceManager0.initialize(0, TASK_ID_62);
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
        multiInstanceManager1.initialize(1, TASK_ID_63);
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

    @Test
    @DisableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
    public void showInstanceRestorationMessage() {
        MultiWindowUtils.setInstanceCountForTesting(3);
        MultiWindowUtils.setMaxInstancesForTesting(2);
        var messageDispatcher = mock(MessageDispatcher.class);
        when(mCurrentActivity.getResources()).thenReturn(mock(Resources.class));

        mMultiInstanceManager.showInstanceRestorationMessage(messageDispatcher);
        verify(messageDispatcher).enqueueWindowScopedMessage(any(), eq(false));
        assertTrue(
                "SharedPref for tracking restoration message should be updated.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.MULTI_INSTANCE_RESTORATION_MESSAGE_SHOWN,
                                false));
    }

    @Test
    public void triggerInstanceLimitDowngrade() {
        // Set initial instance limit and allocate ids for max instances.
        MultiWindowUtils.setMaxInstancesForTesting(3);
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        mFakeTimeTestRule.advanceMillis(1);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        mFakeTimeTestRule.advanceMillis(1);
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));

        // Decrease instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(2);
        // Simulate recreation of mActivityTask58 after instance limit downgrade. New allocation
        // should result in finishing least recently used activity .
        removeTaskOnRecentsScreen(mActivityTask58);
        List<AppTask> appTasks = setupActivityManagerAppTasks(mActivityTask56, mActivityTask57);
        allocInstanceIndex(2, mActivityTask58);

        verify(appTasks.get(0)).finishAndRemoveTask();
        assertEquals(
                "Task map for LRU activity should be updated.",
                -1,
                MultiInstanceManagerApi31.getTaskFromMap(0));
        assertTrue(
                "SharedPref for tracking downgrade should be updated.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys
                                        .MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED,
                                false));

        // Subsequent reallocation of an instance should not trigger downgrade path to finish the
        // LRU activity task.
        destroyActivity(mActivityTask56);
        // AppTasks should now contain tasks for instances with id=1 and id=2.
        appTasks = setupActivityManagerAppTasks(mActivityTask57, mActivityTask58);
        allocInstanceIndex(0, mActivityTask56);
        verify(appTasks.get(0), never()).finishAndRemoveTask();
        verify(appTasks.get(1), never()).finishAndRemoveTask();
    }

    @Test
    public void triggerInstanceLimitDowngrade_MaxActiveOneInactive_NoTasksFinished() {
        // Set initial instance limit and allocate ids for max instances.
        MultiWindowUtils.setMaxInstancesForTesting(3);
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        mFakeTimeTestRule.advanceMillis(1);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        mFakeTimeTestRule.advanceMillis(1);
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));

        // Make mActivityTask57 inactive.
        removeTaskOnRecentsScreen(mActivityTask57);

        // Decrease instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(2);
        // Simulate recreation of mActivityTask58 after instance limit downgrade. New allocation
        // should result in not finishing any additional tasks since we don't have excess running
        // tasks.
        removeTaskOnRecentsScreen(mActivityTask58);
        List<AppTask> appTasks = setupActivityManagerAppTasks(mActivityTask56);
        allocInstanceIndex(2, mActivityTask58);

        verify(appTasks.get(0), never()).finishAndRemoveTask();
        assertNotEquals(
                "Task map for LRU activity should not be updated.",
                -1,
                MultiInstanceManagerApi31.getTaskFromMap(0));
        assertFalse(
                "SharedPref for tracking downgrade should not be updated.",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys
                                        .MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED,
                                false));
    }

    private List<AppTask> setupActivityManagerAppTasks(Activity... activities) {
        List<AppTask> appTasks = new ArrayList<>();
        for (Activity activity : activities) {
            int taskId = activity.getTaskId();
            var appTask = mock(AppTask.class);
            var appTaskInfo = mock(RecentTaskInfo.class);
            appTaskInfo.taskId = taskId;
            when(appTask.getTaskInfo()).thenReturn(appTaskInfo);
            appTasks.add(appTask);
        }
        Context spyContext = spy(ApplicationProvider.getApplicationContext());
        when(spyContext.getSystemService(Context.ACTIVITY_SERVICE)).thenReturn(mActivityManager);
        ContextUtils.initApplicationContextForTests(spyContext);
        when(mActivityManager.getAppTasks()).thenReturn(appTasks);
        return appTasks;
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
        List<AppTask> appTasks =
                setupActivityManagerAppTasks(mTabbedActivityTask62, mTabbedActivityTask63);

        if (!isActivityAlive) {
            // Force destruction of |mTabbedActivityTask63|.
            destroyActivity(mTabbedActivityTask63);
        }

        // Try to restore the instance in task TASK_ID_63, from |mTabbedActivityTask62|.
        multiInstanceManager.openInstance(1, TASK_ID_63, NewWindowAppSource.WINDOW_MANAGER);

        if (isActivityAlive) {
            // If |mTabbedActivityTask63| is alive, verify that its instance was restored in the
            // existing task by bringing it to the foreground.
            verify(mActivityManager).moveTaskToFront(TASK_ID_63, 0);
            verify(mTabbedActivityTask62, never()).startActivity(any());
            verify(appTasks.get(1), never()).finishAndRemoveTask();
        } else {
            // If |mTabbedActivityTask63| is not alive, verify that |mTabbedActivityTask62| starts a
            // new activity and finishes and removes the old task, and does not attempt to bring the
            // old task to the foreground.
            verify(mTabbedActivityTask62).startActivity(any());
            verify(appTasks.get(1)).finishAndRemoveTask();
            verify(mActivityManager, never()).moveTaskToFront(TASK_ID_63, 0);
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
        verify(mTabGroupSyncService).setLocalObservationMode(/* observeLocalChanges= */ false);

        // Verify we pause the TabPersistentStore.
        verify(mTabPersistentStore).pauseSaveTabList();
        verify(mTabPersistentStore).resumeSaveTabList(mOnSaveTabListRunnableCaptor.capture());

        // Verify we only send the reparent intent after the Runnable runs.
        verify(mTabbedActivityTask62, never()).onNewIntent(any());
        mOnSaveTabListRunnableCaptor.getValue().run();
        verify(mTabbedActivityTask62).onNewIntent(any());

        // Verify we resume the TabGroupSyncService to begin observing local changes.
        verify(mTabGroupSyncService).setLocalObservationMode(/* observeLocalChanges= */ true);
    }

    @Test
    public void testCreateNewWindowIntent_Incognito_OpenNewIncognitoWindowExtraIsTrue() {
        when(mCurrentActivity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());

        Intent intent = mMultiInstanceManager.createNewWindowIntent(/* isIncognito= */ true);

        assertNotNull(intent);
        assertTrue(
                intent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, /* defaultValue= */ false));
    }

    @Test
    public void testCreateNewWindowIntent_NotIncognito_OpenNewIncognitoWindowExtraIsFalse() {
        when(mCurrentActivity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());

        Intent intent = mMultiInstanceManager.createNewWindowIntent(/* isIncognito= */ false);
        assertNotNull(intent);
        assertFalse(
                intent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, /* defaultValue= */ true));
    }

    @Test
    public void
            testCreateNewWindowIntent_NonMultiWindowMode_ShouldNotOpenInAdjacentWindow_NoLaunchAdjacentFlag() {
        when(mCurrentActivity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());

        // Non-multi-window mode
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(false);
        when(mCurrentActivity.isInMultiWindowMode()).thenReturn(false);

        // The new window shouldn't be opened as an adjacent window.
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                false);

        Intent intent = mMultiInstanceManager.createNewWindowIntent(/* isIncognito= */ false);

        assertNotNull(intent);
        assertEquals(0, (intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT));
    }

    @Test
    public void
            testCreateNewWindowIntent_NonMultiWindowMode_ShouldOpenInAdjacentWindow_AddLaunchAdjacentFlag() {
        when(mCurrentActivity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());

        // Non-multi-window mode
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(false);
        when(mCurrentActivity.isInMultiWindowMode()).thenReturn(false);

        // The new window should be opened as an adjacent window.
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                true);

        Intent intent = mMultiInstanceManager.createNewWindowIntent(/* isIncognito= */ false);

        assertNotNull(intent);
        assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testCreateNewWindowIntent_MultiWindowMode_AddLaunchAdjacentFlag() {
        when(mCurrentActivity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());

        // multi-window mode
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(true);
        when(mCurrentActivity.isInMultiWindowMode()).thenReturn(true);

        Intent intent = mMultiInstanceManager.createNewWindowIntent(/* isIncognito= */ false);

        assertNotNull(intent);
        assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testOpenNewWindow_launchesIntentForChromeTabbedActivity() {
        when(mCurrentActivity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MultiInstanceManager.NEW_WINDOW_APP_SOURCE_HISTOGRAM,
                                NewWindowAppSource.OTHER)
                        .build();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        mMultiInstanceManager.openNewWindow("", false, NewWindowAppSource.OTHER);

        verify(mCurrentActivity).startActivity(intentCaptor.capture());
        Intent intent = intentCaptor.getValue();
        assertNotNull(intent.getComponent());
        histogramWatcher.assertExpected();
        assertEquals(
                "org.chromium.chrome.browser.ChromeTabbedActivity",
                intent.getComponent().getClassName());
    }

    @Test
    public void showInstanceCreationLimitMessage() {
        var messageDispatcher = mock(MessageDispatcher.class);
        when(mCurrentActivity.getResources()).thenReturn(mock(Resources.class));

        mMultiInstanceManager.showInstanceCreationLimitMessage(messageDispatcher);

        ArgumentCaptor<PropertyModel> message = ArgumentCaptor.forClass(PropertyModel.class);
        verify(messageDispatcher).enqueueWindowScopedMessage(message.capture(), eq(false));
        assertEquals(
                "Message identifier should match.",
                MessageIdentifier.MULTI_INSTANCE_CREATION_LIMIT,
                message.getValue().get(MessageBannerProperties.MESSAGE_IDENTIFIER));
    }

    @Test
    public void testShowNameWindowDialog_UsesCustomTitle() {
        Activity realActivity = Robolectric.setupActivity(Activity.class);
        var manager = createMultiInstanceManager(realActivity);
        manager.initialize(INSTANCE_ID_1, TASK_ID_56);

        final String customTitle = "Custom Title";
        final String defaultTitle = "Default Title";
        MultiInstanceManagerApi31.writeCustomTitle(INSTANCE_ID_1, customTitle);
        MultiInstanceManagerApi31.writeTitle(INSTANCE_ID_1, defaultTitle);

        manager.showNameWindowDialog(NameWindowDialogSource.TAB_STRIP);

        Dialog dialog = ShadowDialog.getLatestDialog();
        assertTrue("Dialog should be showing.", dialog.isShowing());

        TextInputEditText editText = dialog.findViewById(R.id.title_input_text);
        assertEquals(
                "Dialog should be pre-filled with the custom title.",
                customTitle,
                editText.getText().toString());
    }

    @Test
    public void testShowNameWindowDialog_UsesRegularTitleAsFallback() {
        Activity realActivity = Robolectric.setupActivity(Activity.class);
        var manager = createMultiInstanceManager(realActivity);
        manager.initialize(INSTANCE_ID_1, TASK_ID_56);

        final String defaultTitle = "Default Title";
        MultiInstanceManagerApi31.writeTitle(INSTANCE_ID_1, defaultTitle);
        ChromeSharedPreferences.getInstance()
                .removeKey(MultiInstanceManagerApi31.customTitleKey(INSTANCE_ID_1));

        manager.showNameWindowDialog(NameWindowDialogSource.TAB_STRIP);

        Dialog dialog = ShadowDialog.getLatestDialog();
        assertTrue("Dialog should be showing.", dialog.isShowing());

        TextInputEditText editText = dialog.findViewById(R.id.title_input_text);
        assertEquals(
                "Dialog should be pre-filled with the regular title.",
                defaultTitle,
                editText.getText().toString());
    }

    @Test
    public void testShowNameWindowDialog_DialogCallbackUpdatesTitle() {
        Activity realActivity = Robolectric.setupActivity(Activity.class);
        var manager = createMultiInstanceManager(realActivity);
        manager.initialize(INSTANCE_ID_1, TASK_ID_56);
        final String defaultTitle = "Default Title";
        MultiInstanceManagerApi31.writeTitle(INSTANCE_ID_1, defaultTitle);

        manager.showNameWindowDialog(NameWindowDialogSource.TAB_STRIP);

        Dialog dialog = ShadowDialog.getLatestDialog();
        assertTrue("Dialog should be showing.", dialog.isShowing());

        final String newTitle = "Custom Title";
        TextInputEditText editText = dialog.findViewById(R.id.title_input_text);
        editText.setText(newTitle);

        dialog.findViewById(R.id.positive_button).performClick();

        assertFalse("Dialog should be dismissed.", dialog.isShowing());
        assertEquals(
                "New custom title should be saved.",
                newTitle,
                MultiInstanceManagerApi31.readCustomTitle(INSTANCE_ID_1));
    }

    @Test
    public void testShowNameWindowDialog_DialogCallbackIgnoresDefaultTitle() {
        Activity realActivity = Robolectric.setupActivity(Activity.class);
        var manager = createMultiInstanceManager(realActivity);
        manager.initialize(INSTANCE_ID_1, TASK_ID_56);
        final String defaultTitle = "Default Title";
        MultiInstanceManagerApi31.writeTitle(INSTANCE_ID_1, defaultTitle);

        manager.showNameWindowDialog(NameWindowDialogSource.TAB_STRIP);

        Dialog dialog = ShadowDialog.getLatestDialog();
        assertTrue("Dialog should be showing.", dialog.isShowing());

        TextInputEditText editText = dialog.findViewById(R.id.title_input_text);
        editText.setText(defaultTitle);

        dialog.findViewById(R.id.positive_button).performClick();

        assertFalse("Dialog should be dismissed.", dialog.isShowing());
        assertNull(
                "Custom title should not be saved if identical to default title.",
                MultiInstanceManagerApi31.readCustomTitle(INSTANCE_ID_1));
    }
}
