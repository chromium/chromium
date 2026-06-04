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
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.multiwindow.MultiWindowUtils.INVALID_TASK_ID;
import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseBooleanArray;

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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDialog;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.RecentlyClosedEntriesManagerTracker;
import org.chromium.chrome.browser.RecentlyClosedEntriesManagerTrackerFactory;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceDataProto.MultiInstanceData;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.AllocatedIdInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.CloseWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.InstanceAllocationType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
import org.chromium.chrome.browser.preferences.MultiInstancePreferenceKeys;
import org.chromium.chrome.browser.preferences.MultiInstanceSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;
import java.util.stream.Collectors;

/** Unit tests for {@link MultiInstanceManagerApi31}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH,
    ChromeFeatureList.INCOGNITO_AS_WINDOW_FULL_SCREEN
})
@DisableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
public class MultiInstanceManagerApi31UnitTest {
    private static final int INSTANCE_ID_1 = 1;
    private static final int INSTANCE_ID_2 = 2;
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

    private static final String TITLE1 = "title1";
    private static final String TITLE2 = "title2";
    private static final String TITLE3 = "title3";
    private static final GURL URL1 = JUnitTestGURLs.URL_1;
    private static final GURL URL2 = JUnitTestGURLs.URL_2;
    private static final GURL URL3 = JUnitTestGURLs.URL_3;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private TabModelOrchestrator mTabModelOrchestrator;
    @Mock private ActivityManager mActivityManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    @Mock private Supplier<DesktopWindowStateManager> mDesktopWindowStateManagerSupplier;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private AppHeaderState mAppHeaderState;
    @Mock private TabReparentingDelegate mTabReparentingDelegate;

    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private MismatchedIndicesHandler mMismatchedIndicesHandler;
    @Mock private TabModelSelectorBase mTabModelSelector;
    @Mock private TabModel mNormalTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab3;

    @Mock private Activity mActivityTask56;
    @Mock private Activity mActivityTask57;
    @Mock private Activity mActivityTask58;
    @Mock private Activity mActivityTask59;
    @Mock private Activity mActivityTask60;
    @Mock private Activity mActivityTask61;
    @Mock private ChromeTabbedActivity mTabbedActivityTask62;
    @Mock private ChromeTabbedActivity mTabbedActivityTask63;
    @Mock private ChromeTabbedActivity mTabbedActivityTask64;
    @Mock private ChromeTabbedActivity mTabbedActivityTask65;
    @Mock private ChromeTabbedActivity mTabbedActivityTask66;
    @Mock private RecentlyClosedEntriesManagerTracker mRecentlyClosedTracker;
    @Mock private MessageDispatcher mMessageDispatcher;

    @Captor private ArgumentCaptor<List<Integer>> mIntegerListCaptor;
    @Captor private ArgumentCaptor<List<InstanceInfo>> mInstanceInfoListCaptor;

    private final SettableMonotonicObservableSupplier<TabModelOrchestrator>
            mTabModelOrchestratorSupplier = ObservableSuppliers.createMonotonic();
    private SettableNonNullObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();

    Activity mCurrentActivity;
    Activity[] mActivityPool;
    Activity[] mTabbedActivityPool;
    private TestMultiInstanceManagerApi31 mMultiInstanceManager;
    private int mNormalTabCount;
    private int mIncognitoTabCount;

    private TestMultiInstanceManagerApi31 createTestMultiInstanceManager(Activity activity) {
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
                MonotonicObservableSupplier<TabModelOrchestrator> tabModelOrchestratorSupplier,
                MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
                ActivityLifecycleDispatcher activityLifecycleDispatcher,
                NonNullObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
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
            MultiWindowUtils.setAppTaskIdsForTesting(mAppTaskIds);
        }

        private void createInstance(int instanceId, Activity activity) {
            ChromeMultiInstancePersistentStore.writeActiveTabUrl(
                    instanceId, "https://id-" + instanceId + ".com");
            ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
            updateTasksWithoutDestroyingActivity(instanceId, activity);
            addInstanceInfo(instanceId, activity.getTaskId());
        }

        private void setAdjacentInstance(Activity activity) {
            mAdjacentInstance = activity;
        }

        private void addInstanceInfo(int instanceId, int taskId) {
            ChromeMultiInstancePersistentStore.writeLastAccessedTime(instanceId);
            ChromeMultiInstancePersistentStore.writeProfileType(
                    instanceId, SupportedProfileType.REGULAR);
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
                                ChromeMultiInstancePersistentStore.readActiveTabUrl(instanceId),
                                /* title= */ "",
                                /* customTitle= */ null,
                                /* tabCount= */ 0,
                                /* incognitoTabCount= */ 0,
                                /* isIncognitoSelected= */ false,
                                ChromeMultiInstancePersistentStore.readLastAccessedTime(instanceId),
                                ChromeMultiInstancePersistentStore.readClosureTime(instanceId)));
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
        public List<InstanceInfo> getInstanceInfo(
                @PersistedInstanceType int persistedInstanceType) {
            if (mTestBuildInstancesList) {
                return mTestInstanceInfos;
            }
            return super.getInstanceInfo(persistedInstanceType);
        }
    }

    @Before
    public void setUp() {
        mModalDialogManagerSupplier = ObservableSuppliers.createNonNull(mModalDialogManager);
        mTabModelOrchestratorSupplier.set(mTabModelOrchestrator);
        MultiInstanceOrchestratorImpl.setTabReparentingDelegateForTesting(mTabReparentingDelegate);
        MultiInstanceOrchestratorFactory.setInstance(MultiInstanceOrchestratorImpl.getInstance());

        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
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
        when(mActivityTask59.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        when(mActivityTask60.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        when(mActivityTask61.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        when(mTabbedActivityTask62.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);
        when(mTabbedActivityTask63.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn(mActivityManager);

        when(mActivityManager.getAppTasks()).thenReturn(new ArrayList<>());

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
        MultiWindowTestUtils.setupTabModelSelectorFactory(mProfile, mIncognitoProfile);
        mMultiInstanceManager = spy(createTestMultiInstanceManager(mCurrentActivity));

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
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        when(mNormalTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        doNothing()
                .when(mMultiInstanceManager)
                .showTargetSelectorDialog(MockitoHelper.anyCallback(), anyInt(), anyInt());

        setupActivityForCreateNewWindowIntent(mCurrentActivity);
        RecentlyClosedEntriesManagerTrackerFactory.setInstanceForTesting(mRecentlyClosedTracker);
        doReturn(mMessageDispatcher).when(mMultiInstanceManager).getMessageDispatcher();
    }

    @After
    public void tearDown() {
        MultiWindowTestUtils.resetInstanceInfo();
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        ApplicationStatus.destroyForJUnitTests();
        mMultiInstanceManager.mTestBuildInstancesList = false;
    }

    private void setupActivityForCreateNewWindowIntent(Activity activity) {
        // Setup mocks to ensure that MultiWindowUtils#createNewWindowIntent() runs successfully.
        MultiWindowTestUtils.enableMultiInstance();
        when(activity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());
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
        softCloseInstance(mActivityPool[1]);

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

        softCloseInstance(mActivityPool[1]);

        // New instance is assigned the instance ID 1 again when the associated task is
        // brought foreground and attempts to recreate the activity.
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[1]));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_STARTUP_WINDOW_POLICY)
    public void testAllocInstanceId_onStartupWindowPolicy_refrainsFromUsingExistingInstanceState() {
        DeviceInfo.setIsDesktopForTesting(true);
        // Allocate instance 0 and 1.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[0]));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[1]));

        // Simulate closing a window from Android Recents.
        removeTaskOnRecentsScreen(mActivityPool[1]);

        // Normally, without ON_STARTUP_WINDOW_POLICY, allocating a new window here would reuse
        // instance 1 because it has persistent state. With ON_STARTUP_WINDOW_POLICY enabled, it
        // refrains from reusing instance 1 and allocates a brand-new unused index (2).
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[1]));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ON_STARTUP_WINDOW_POLICY)
    public void testAllocInstanceId_onStartupWindowPolicy_relaunchBypassesPolicy() {
        DeviceInfo.setIsDesktopForTesting(true);
        // Allocate instance 0 and 1.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[0]));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityPool[1]));

        // Simulate closing a window from Android Recents.
        removeTaskOnRecentsScreen(mActivityPool[1]);

        // Mock the intent for the current activity to contain EXTRA_FROM_RELAUNCH.
        Intent relaunchIntent = new Intent();
        relaunchIntent.putExtra(IntentHandler.EXTRA_FROM_RELAUNCH, true);
        when(mCurrentActivity.getIntent()).thenReturn(relaunchIntent);

        // With ON_STARTUP_WINDOW_POLICY enabled, but with EXTRA_FROM_RELAUNCH set on the intent,
        // it should bypass the skip check and reuse instance 1.
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
                createTestMultiInstanceManager(mActivityTask59);
        MultiWindowUtils.setAppTaskIdsForTesting(
                new HashSet<>(Arrays.asList(TASK_ID_57, TASK_ID_58, TASK_ID_59)));
        AllocatedIdInfo instanceIdInfo =
                multiInstanceManager.allocInstanceId(
                        PASSED_ID_INVALID,
                        TASK_ID_59,
                        /* preferNew= */ true,
                        /* isIncognitoIntent= */ false);
        assertEquals(
                "Should not allocate valid instance id when at limit.",
                INVALID_WINDOW_ID,
                (int) instanceIdInfo.instanceId);
        assertEquals(
                "Should return PREFER_NEW_INVALID_INSTANCE.",
                InstanceAllocationType.PREFER_NEW_INVALID_INSTANCE,
                (int) instanceIdInfo.allocationType);
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
        AllocatedIdInfo instanceIdInfo =
                mMultiInstanceManager.allocInstanceId(
                        PASSED_ID_INVALID,
                        TASK_ID_59,
                        /* preferNew= */ true,
                        /* isIncognitoIntent= */ false);
        assertEquals("Should allocate a new instance id.", 3, (int) instanceIdInfo.instanceId);
        assertEquals(
                "Should return PREFER_NEW_INSTANCE_NEW_TASK.",
                InstanceAllocationType.PREFER_NEW_INSTANCE_NEW_TASK,
                (int) instanceIdInfo.allocationType);
    }

    @Test
    public void testAllocInstance_pickMruInstance() throws InterruptedException {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));

        removeTaskOnRecentsScreen(mActivityTask57);
        removeTaskOnRecentsScreen(mActivityTask58);

        // New instantiation picks up the most recently used one.
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(1);
        // These two writes can often use the same timestamp, and cause the result to be random.
        // Wait for the next millisecond to guarantee this doesn't happen.
        mFakeTimeTestRule.advanceMillis(1);
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(2); // Accessed most recently.

        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask59));
        removeTaskOnRecentsScreen(mActivityTask59);

        ChromeMultiInstancePersistentStore.writeLastAccessedTime(
                1); // instance ID 1 is now the MRU.
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask60));
    }

    @Test
    public void testGetInstanceInfo_closesInstancesOlderThanSixMonths() {
        // Current activity is mActivityTask56, managed by mMultiInstanceManager.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        assertEquals(3, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Advancing time by well over six months.
        mFakeTimeTestRule.advanceMillis(MultiInstanceManagerApi31.SIX_MONTHS_MS + 5000000);
        // Closing the two other instances that are not managing the current activity.
        assertEquals(1, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());
        verify(mMultiInstanceManager, times(1))
                .closeWindows(
                        mIntegerListCaptor.capture(),
                        eq(CloseWindowAppSource.RETENTION_PERIOD_EXPIRATION));
        List<List<Integer>> capturedLists = mIntegerListCaptor.getAllValues();
        assertEquals(1, capturedLists.size());
        assertEquals(2, capturedLists.get(0).size());
    }

    @Test
    public void testRemoveInvalidInstanceData_doesNotCloseCurrentInstanceEvenIfExpired() {
        // Setup current activity and instance.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mCurrentActivity));
        mMultiInstanceManager.initialize(0, TASK_ID_56, SupportedProfileType.MIXED);

        // Advance time by over six months.
        mFakeTimeTestRule.advanceMillis(MultiInstanceManagerApi31.SIX_MONTHS_MS + 1000);

        // Instance 0 is the current instance, it should NOT be closed.
        assertEquals(1, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());
        verify(mMultiInstanceManager, never())
                .closeWindows(any(), eq(CloseWindowAppSource.RETENTION_PERIOD_EXPIRATION));
    }

    @Test
    public void testRemoveInvalidInstanceData_closesExpiredInactiveInstance() {
        // Setup current activity and instance.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mCurrentActivity));
        mMultiInstanceManager.initialize(0, TASK_ID_56, SupportedProfileType.MIXED);

        // Setup another instance and make it inactive.
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        removeTaskOnRecentsScreen(mActivityTask57);

        // Advance time by over six months.
        mFakeTimeTestRule.advanceMillis(MultiInstanceManagerApi31.SIX_MONTHS_MS + 1000);

        // Instance 1 is expired and inactive, it should be closed.
        // Instance 0 is expired but current, it should not be closed.
        assertEquals(1, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());
        assertEquals(
                0,
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).get(0).instanceId);
        verify(mMultiInstanceManager, times(1))
                .closeWindows(any(), eq(CloseWindowAppSource.RETENTION_PERIOD_EXPIRATION));
    }

    @Test
    public void testAllocInstanceId_cleansUpExpiredInstanceBeforeAllocation() {
        // Setup an existing instance.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        mMultiInstanceManager.initialize(0, TASK_ID_56, SupportedProfileType.MIXED);

        // Simulate activity destruction and task removal (inactive instance).
        removeTaskOnRecentsScreen(mActivityTask56);

        // Advance time by over six months.
        mFakeTimeTestRule.advanceMillis(MultiInstanceManagerApi31.SIX_MONTHS_MS + 1000);

        // Now allocate a new instance for a new activity.
        // The old instance 0 should be cleaned up because it is expired and not the current
        // activity.
        // Then ID 0 should be re-allocated as a NEW instance.
        AllocatedIdInfo info =
                mMultiInstanceManager.allocInstanceId(
                        PASSED_ID_INVALID,
                        TASK_ID_57,
                        /* preferNew= */ false,
                        /* isIncognitoIntent= */ false);

        assertEquals(0, info.instanceId);
        assertEquals(InstanceAllocationType.NEW_INSTANCE_NEW_TASK, info.allocationType);
        verify(mMultiInstanceManager, times(1))
                .closeWindows(any(), eq(CloseWindowAppSource.RETENTION_PERIOD_EXPIRATION));
    }

    @Test
    public void testGetInstanceInfo_size_hardClosure() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        mMultiInstanceManager.setAdjacentInstance(mActivityTask57);

        assertEquals(3, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Removing a task from recent screen doesn't affect instance info list.
        removeTaskOnRecentsScreen(mActivityTask58);
        assertEquals(3, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Activity destroyed in the background due to memory constraint has no impact either.
        softCloseInstance(mActivityTask57);
        assertEquals(3, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Closing an instance removes the entry.
        mMultiInstanceManager.closeWindows(
                Collections.singletonList(1), CloseWindowAppSource.OTHER);
        assertEquals(2, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());
    }

    @Test
    public void testGetInstanceInfo_size_incognitoWindow_hardClosure() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        ChromeMultiInstancePersistentStore.writeTabCount(
                0, /* normalTabCount= */ 0, /* incognitoTabCount= */ 1);
        ChromeMultiInstancePersistentStore.writeTabCount(
                1, /* normalTabCount= */ 0, /* incognitoTabCount= */ 1);
        ChromeMultiInstancePersistentStore.writeTabCount(
                2, /* normalTabCount= */ 0, /* incognitoTabCount= */ 1);
        mMultiInstanceManager.setAdjacentInstance(mActivityTask57);

        assertEquals(3, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Activity destroyed in the background due to memory constraint has no impact.
        softCloseInstance(mActivityTask57);
        assertEquals(3, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Removing a task from recent screen cleans up the incognito window.
        removeTaskOnRecentsScreen(mActivityTask58);
        assertEquals(2, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Closing an instance from CloseWindowAppSource.OTHER cleans up the incognito window.
        mMultiInstanceManager.closeWindows(
                Collections.singletonList(1), CloseWindowAppSource.OTHER);
        assertEquals(1, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Closing an instance from CloseWindowAppSource.WINDOW_MANAGER cleans up the incognito
        // window.
        mMultiInstanceManager.closeWindows(
                Collections.singletonList(0), CloseWindowAppSource.WINDOW_MANAGER);
        assertEquals(0, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());
    }

    @Test
    public void testGetInstanceInfo_size_softClosure() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        mMultiInstanceManager.setAdjacentInstance(mActivityTask57);

        assertEquals(3, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Removing a task from recent screen doesn't affect instance info list.
        removeTaskOnRecentsScreen(mActivityTask58);
        assertEquals(3, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Trigger a soft closure on this window.
        softCloseInstance(mActivityTask57);
        assertEquals(3, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Soft closing an instance does not delete persisted state for the entry.
        mMultiInstanceManager.closeWindows(
                Collections.singletonList(1), CloseWindowAppSource.WINDOW_MANAGER);
        List<InstanceInfo> instanceInfoList =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY);
        assertEquals(2, instanceInfoList.size());
        assertTrue(ChromeMultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 1));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 0));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 2));
    }

    @Test
    public void testGetRecentlyClosedInstances() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));

        // Make instance1 inactive, but still usable.
        removeTaskOnRecentsScreen(mActivityTask57);

        // Close instance2 from the window manager, this should make it inactive and unusable (ie.
        // marked for deletion).
        mMultiInstanceManager.closeWindows(List.of(2), CloseWindowAppSource.WINDOW_MANAGER);
        destroyActivity(mActivityTask58);

        // Verify #getInstanceInfo() lists.
        List<InstanceInfo> activeInstances =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ACTIVE);
        List<InstanceInfo> inactiveUsableInstances =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE);
        assertEquals(
                "Total # of usable instances is incorrect.",
                2,
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());
        assertEquals("# of active instances is incorrect.", 1, activeInstances.size());
        assertEquals(
                "# of inactive, usable instances is incorrect.", 1, inactiveUsableInstances.size());
        assertEquals("Instance 0 should be active.", 0, activeInstances.get(0).instanceId);
        assertEquals(
                "Instance 1 should be inactive.", 1, inactiveUsableInstances.get(0).instanceId);

        // Verify #getRecentlyClosedInstances() list.
        List<InstanceInfo> closedInstances = mMultiInstanceManager.getRecentlyClosedInstances();
        assertEquals(2, closedInstances.size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testGetRecentlyClosedInstances_excludesIncognitoWindows() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(1, mActivityTask57));
        ChromeMultiInstancePersistentStore.writeProfileType(1, SupportedProfileType.OFF_THE_RECORD);

        // Make instance1 inactive, but still usable.
        removeTaskOnRecentsScreen(mActivityTask57);

        // Verify #getInstanceInfo() lists.
        List<InstanceInfo> activeInstances =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ACTIVE);
        List<InstanceInfo> inactiveUsableInstances =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE);
        assertEquals(
                "Total # of usable instances is incorrect.",
                2,
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());
        assertEquals("# of active instances is incorrect.", 1, activeInstances.size());
        assertEquals(
                "# of inactive, usable instances is incorrect.", 1, inactiveUsableInstances.size());
        assertEquals("Instance 0 should be active.", 0, activeInstances.get(0).instanceId);
        assertEquals(
                "Instance 1 should be inactive.", 1, inactiveUsableInstances.get(0).instanceId);

        // Verify #getRecentlyClosedInstances() list.
        List<InstanceInfo> closedInstances = mMultiInstanceManager.getRecentlyClosedInstances();
        assertEquals("# of recently closed instances is incorrect.", 0, closedInstances.size());
    }

    @Test
    public void testCloseWindows_OnInstancesClosedInvoked() {
        // Setup 3 instances.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));

        // Verify there are 3 active instances initially.
        assertEquals(3, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ACTIVE).size());

        // Trigger a soft closure for instance ID 1.
        when(mActivityTask57.isFinishing()).thenReturn(true);
        long initialTime = ChromeMultiInstancePersistentStore.readClosureTime(/* instanceId= */ 1);
        mFakeTimeTestRule.advanceMillis(100);
        mMultiInstanceManager.closeWindows(
                Collections.singletonList(1), CloseWindowAppSource.WINDOW_MANAGER);

        // Verify that the instance marked for deletion is not considered usable.
        assertEquals(2, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Verify that closure time is updated.
        assertTrue(
                ChromeMultiInstancePersistentStore.readClosureTime(/* instanceId= */ 1)
                        > initialTime);

        // Verify #onInstancesClosed is invoked.
        verify(mRecentlyClosedTracker)
                .onInstancesClosed(mInstanceInfoListCaptor.capture(), eq(false));

        // Verify the captured InstanceInfo.
        List<InstanceInfo> closedInstanceInfo = mInstanceInfoListCaptor.getValue();
        assertEquals("There should be exactly 1 InstanceInfo.", 1, closedInstanceInfo.size());
        assertEquals("Instance ID should be 1.", 1, closedInstanceInfo.get(0).instanceId);

        // Verify the instance is correctly marked for deletion.
        assertTrue(ChromeMultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 1));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 0));
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 2));

        // Subsequent restoration should update `markedForDeletion` instance state.
        mMultiInstanceManager.openWindow(1, NewWindowAppSource.WINDOW_MANAGER);
        List<InstanceInfo> instanceInfoList =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY);
        assertEquals(3, instanceInfoList.size());
        assertFalse(ChromeMultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 1));
    }

    @Test
    public void testCloseWindows_OnInstancesClosedInvoked_MixedIncognitoAndRegularWindows() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        ChromeMultiInstancePersistentStore.writeTabCount(
                2, /* normalTabCount= */ 0, /* incognitoTabCount= */ 1);

        assertEquals(3, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Close one regular and one incognito window.
        mMultiInstanceManager.closeWindows(
                Arrays.asList(1, 2), CloseWindowAppSource.WINDOW_MANAGER);

        // Verify that #onInstancesClosed is invoked only for regular window.
        verify(mRecentlyClosedTracker)
                .onInstancesClosed(
                        argThat(list -> list.size() == 1 && list.get(0).instanceId == 1),
                        eq(false));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), eq(true));
    }

    @Test
    public void testCloseWindows_OnInstancesClosedNotInvoked_WindowContainsOnlyOneNtp() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        ChromeMultiInstancePersistentStore.writeTabCount(
                1, /* normalTabCount= */ 1, /* incognitoTabCount= */ 0);
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(1, "chrome-native://newtab/");

        assertEquals(2, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Close the window that contains only 1 NTP.
        mMultiInstanceManager.closeWindows(
                Collections.singletonList(1), CloseWindowAppSource.WINDOW_MANAGER);

        // Verify that #onInstanceClosed is never invoked.
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), anyBoolean());

        // Verify the window that contains only 1 NTP is permanently closed.
        assertEquals(1, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());
    }

    @Test
    public void testCloseAllWindows_markedForDeletion() {
        MultiWindowUtils.setMaxInstancesForTesting(5);

        // Setup 3 instances.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mCurrentActivity));
        mMultiInstanceManager.initialize(
                /* instanceId= */ 0, /* taskId= */ TASK_ID_56, SupportedProfileType.MIXED);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));

        // Verify that there are 3 active instances initially.
        assertEquals(3, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ACTIVE).size());

        // Simulate closure of all windows from the window manager.
        mMultiInstanceManager.closeWindows(List.of(0, 1, 2), CloseWindowAppSource.WINDOW_MANAGER);
        destroyActivity(mCurrentActivity);
        destroyActivity(mActivityTask57);
        destroyActivity(mActivityTask58);

        // Verify that the current activity is finished last.
        InOrder inOrderVerifier = inOrder(mCurrentActivity, mActivityTask57, mActivityTask58);
        inOrderVerifier.verify(mActivityTask57).finishAndRemoveTask();
        inOrderVerifier.verify(mActivityTask58).finishAndRemoveTask();
        inOrderVerifier.verify(mCurrentActivity).finishAndRemoveTask();

        // Verify that we have persisted state for all 3 instances, that are now marked for
        // deletion and considered unusable.
        List<InstanceInfo> instances =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY);
        assertEquals(0, instances.size());
        assertTrue(ChromeMultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 0));
        assertTrue(ChromeMultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 1));
        assertTrue(ChromeMultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 2));

        // Verify that subsequent id allocation uses a new id, not a persisted one marked for
        // deletion.
        var multiInstanceManager = createTestMultiInstanceManager(mActivityTask59);
        var allocatedIdInfo =
                multiInstanceManager.allocInstanceId(
                        /* windowId= */ -1,
                        TASK_ID_59,
                        /* preferNew= */ false,
                        /* isIncognitoIntent= */ false);
        assertEquals(3, allocatedIdInfo.instanceId);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testGetInstanceInfo_filters() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);

        // Instance 0: Active, Regular
        assertEquals(0, allocInstanceIndex(0, mTabbedActivityPool[0]));
        ChromeMultiInstancePersistentStore.writeProfileType(0, SupportedProfileType.REGULAR);

        // Instance 1: Active, Incognito
        assertEquals(1, allocInstanceIndex(1, mTabbedActivityPool[1]));
        ChromeMultiInstancePersistentStore.writeProfileType(1, SupportedProfileType.OFF_THE_RECORD);

        // Instance 2: Inactive, Regular
        assertEquals(2, allocInstanceIndex(2, mTabbedActivityPool[2]));
        ChromeMultiInstancePersistentStore.writeProfileType(2, SupportedProfileType.REGULAR);
        removeTaskOnRecentsScreen(mTabbedActivityPool[2]);

        // Instance 3: Inactive, Incognito
        assertEquals(3, allocInstanceIndex(3, mTabbedActivityPool[3]));
        ChromeMultiInstancePersistentStore.writeProfileType(3, SupportedProfileType.OFF_THE_RECORD);
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

        // Test combined filter: INACTIVE and REGULAR
        List<InstanceInfo> inactiveRegularInstances =
                mMultiInstanceManager.getInstanceInfo(
                        PersistedInstanceType.INACTIVE | PersistedInstanceType.REGULAR);
        assertEquals(
                "INACTIVE | REGULAR should return 1 instance", 1, inactiveRegularInstances.size());
        assertEquals(2, inactiveRegularInstances.get(0).instanceId);
    }

    @Test
    public void testGetInstanceInfo_closesInactiveInstancesExceedingLimit() {
        // Setup 30 inactive instances with distinct lastAccessedTimes.
        // MAX_INACTIVE_INSTANCE_COUNT is 25.
        // Use instance IDs from 100 to 129 to avoid conflict with existing test setup IDs.
        for (int i = 0; i < 30; i++) {
            int instanceId = i + 100;
            MultiWindowTestUtils.createInstance(
                    instanceId, "https://url" + instanceId + ".com", 1, INVALID_TASK_ID);
            // Advance time to ensure each instance has a different lastAccessedTime.
            mFakeTimeTestRule.advanceMillis(1);
        }

        // Trigger cleanup by calling getInstanceInfo.
        mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY);

        // Verify closeWindows was called with the inactive instances exceeding the limit.
        verify(mMultiInstanceManager)
                .closeWindows(
                        mIntegerListCaptor.capture(),
                        eq(CloseWindowAppSource.RECENTLY_CLOSED_LIMIT_EXCEEDED));

        List<Integer> closedInstances = mIntegerListCaptor.getValue();
        // Since we have 30 inactive instances and the limit is 25, 5 should be closed.
        assertEquals("Should have closed 5 instances", 5, closedInstances.size());

        // The first 5 instances (100-104) are the oldest and should have been closed.
        for (int i = 0; i < 5; i++) {
            assertTrue(
                    "Instance " + (i + 100) + " should be among closed instances.",
                    closedInstances.contains(i + 100));
        }
    }

    @Test
    public void testCurrentInstanceId() {
        // Ensure the single instance at non-zero position is handled okay.
        int expected = 2;
        assertEquals(expected, allocInstanceIndex(expected, mActivityTask56));
        mMultiInstanceManager.initialize(expected, TASK_ID_56, SupportedProfileType.MIXED);
        int id = mMultiInstanceManager.getCurrentInstanceId();
        assertEquals("Current instanceId is not as expected", expected, id);
    }

    @Test
    public void testSelectedTabUpdatesInstanceInfo() {
        mTabModelOrchestratorSupplier.set(mTabModelOrchestrator);
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
        multiInstanceManager.initialize(INSTANCE_ID_1, TASK_ID_57, SupportedProfileType.MIXED);
        TabModelObserver tabModelObserver = multiInstanceManager.getTabModelObserverForTesting();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        final String customTitle = "My Custom Title";
        ChromeMultiInstancePersistentStore.writeCustomTitle(INSTANCE_ID_1, customTitle);

        triggerSelectTab(tabModelObserver, mTab1);
        assertFalse(
                "Normal tab should be selected",
                ChromeMultiInstancePersistentStore.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals(
                "Title should be from the active normal tab",
                TITLE1,
                ChromeMultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL1.getSpec(),
                ChromeMultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1));

        // Update url/title as a new normal tab is selected.
        triggerSelectTab(tabModelObserver, mTab2);
        assertFalse(
                "Normal tab should be selected",
                ChromeMultiInstancePersistentStore.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals(
                "Title should be from the active normal tab",
                TITLE2,
                ChromeMultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL2.getSpec(),
                ChromeMultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1));

        // Incognito tab doesn't affect url/title when selected.
        triggerSelectTab(tabModelObserver, mTab3);
        assertTrue(
                "Incognito tab should be selected",
                ChromeMultiInstancePersistentStore.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals(
                "Title should be from the active normal tab",
                TITLE2,
                ChromeMultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL2.getSpec(),
                ChromeMultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1));

        // Nulled-tab doesn't affect url/title either.
        triggerSelectTab(tabModelObserver, null);
        assertTrue(
                "Incognito tab should be selected",
                ChromeMultiInstancePersistentStore.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals(
                "Null tab should not affect the title",
                TITLE2,
                ChromeMultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1));
        assertEquals(
                "Null tab should not affect the URL",
                URL2.getSpec(),
                ChromeMultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1));
        assertEquals(
                "Custom title should not change when tab changes.",
                customTitle,
                ChromeMultiInstancePersistentStore.readCustomTitle(INSTANCE_ID_1));
    }

    @Test
    public void testRenameInstanceUpdatesCustomTitle() {
        final String newTitle = "My Renamed Window";
        final int instanceId = 2;
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(instanceId);
        mMultiInstanceManager.renameInstance(instanceId, newTitle);

        assertEquals(
                "Custom title should be updated in SharedPreferences.",
                newTitle,
                ChromeMultiInstancePersistentStore.readCustomTitle(instanceId));
    }

    @Test
    public void testTabEventsUpdatesTabCounts() {
        mTabModelOrchestratorSupplier.set(mTabModelOrchestrator);
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
        multiInstanceManager.initialize(INSTANCE_ID_1, TASK_ID_57, SupportedProfileType.MIXED);
        TabModelObserver tabModelObserver = multiInstanceManager.getTabModelObserverForTesting();

        when(mTab1.isIncognito()).thenReturn(false);
        when(mTab2.isIncognito()).thenReturn(false);
        when(mTab3.isIncognito()).thenReturn(true);

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        final String normalTabMessage = "Normal tab count does not match";
        final String incognitoTabMessage = "Normal tab count does not match";
        triggerAddTab(tabModelObserver, mTab1); // normal tab added
        assertEquals(
                normalTabMessage,
                1,
                ChromeMultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                0,
                ChromeMultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_1));

        triggerAddTab(tabModelObserver, mTab2); // normal tab added
        assertEquals(
                normalTabMessage,
                2,
                ChromeMultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                0,
                ChromeMultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_1));

        triggerAddTab(tabModelObserver, mTab3); // incognito tab added
        assertEquals(
                normalTabMessage,
                2,
                ChromeMultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                1,
                ChromeMultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_1));

        triggerOnFinishingTabClosure(tabModelObserver, mTab1);
        assertEquals(
                normalTabMessage,
                1,
                ChromeMultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                1,
                ChromeMultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_1));

        triggerTabRemoved(tabModelObserver, mTab3);
        assertEquals(
                normalTabMessage,
                1,
                ChromeMultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                0,
                ChromeMultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_1));

        triggerTabRemoved(tabModelObserver, mTab2);
        assertEquals(
                normalTabMessage,
                0,
                ChromeMultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                0,
                ChromeMultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_1));
    }

    @Test
    public void testZeroNormalTabClearsUrlTitle() {
        mTabModelOrchestratorSupplier.set(mTabModelOrchestrator);
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
        multiInstanceManager.initialize(INSTANCE_ID_1, TASK_ID_57, SupportedProfileType.MIXED);
        TabModelObserver tabModelObserver = multiInstanceManager.getTabModelObserverForTesting();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        triggerAddTab(tabModelObserver, mTab1);
        triggerSelectTab(tabModelObserver, mTab1);
        assertEquals(
                "Title should be from the active normal tab",
                TITLE1,
                ChromeMultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL1.getSpec(),
                ChromeMultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1));

        triggerAddTab(tabModelObserver, mTab2);
        triggerSelectTab(tabModelObserver, mTab2);
        assertEquals(
                "Title should be from the active normal tab",
                TITLE2,
                ChromeMultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL2.getSpec(),
                ChromeMultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1));

        triggerOnFinishingTabClosure(tabModelObserver, mTab1);
        triggerTabRemoved(tabModelObserver, mTab2);
        assertEquals(
                "Tab count should be zero",
                0,
                ChromeMultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertTrue(
                "Title was not cleared",
                TextUtils.isEmpty(
                        ChromeMultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1)));
        assertTrue(
                "URL was not cleared",
                TextUtils.isEmpty(
                        ChromeMultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1)));
    }

    @Test
    public void testRemoveInstanceInfo() {
        int index = 1;
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(index);
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(index, /* url= */ "url");
        ChromeMultiInstancePersistentStore.writeActiveTabTitle(index, /* title= */ "title");
        ChromeMultiInstancePersistentStore.writeCustomTitle(index, /* title= */ "title");
        ChromeMultiInstancePersistentStore.writeTabCount(
                index, /* normalTabCount= */ 1, /* incognitoTabCount= */ 1);
        ChromeMultiInstancePersistentStore.writeTabCountForRelaunchSync(index, /* tabCount= */ 2);
        ChromeMultiInstancePersistentStore.writeIncognitoSelected(
                index, /* incognitoSelected= */ true);
        ChromeMultiInstancePersistentStore.writeClosureTime(index);
        ChromeMultiInstancePersistentStore.writeProfileType(
                index, /* profileType= */ SupportedProfileType.MIXED);
        ChromeMultiInstancePersistentStore.writeMarkedForDeletion(
                index, /* markedForDeletion= */ true);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                MultiInstanceManager.CLOSE_WINDOW_APP_SOURCE_HISTOGRAM,
                                CloseWindowAppSource.OTHER)
                        .build();

        MultiInstanceManagerApi31.removeInstanceInfo(index, CloseWindowAppSource.OTHER);
        histogramWatcher.assertExpected();
        assertNull(
                "Persistent store should be updated.",
                ChromeMultiInstancePersistentStore.readActiveTabUrl(index));
        assertNull(
                "Persistent store should be updated.",
                ChromeMultiInstancePersistentStore.readActiveTabTitle(index));
        assertNull(
                "Persistent store should be updated.",
                ChromeMultiInstancePersistentStore.readCustomTitle(index));
        assertEquals(
                "Persistent store should be updated.",
                0,
                ChromeMultiInstancePersistentStore.readNormalTabCount(index));
        assertEquals(
                "Persistent store should be updated.",
                0,
                ChromeMultiInstancePersistentStore.readTabCountForRelaunch(index));
        assertEquals(
                "Persistent store should be updated.",
                0,
                ChromeMultiInstancePersistentStore.readIncognitoTabCount(index));
        assertFalse(
                "Persistent store should be updated.",
                ChromeMultiInstancePersistentStore.readIncognitoSelected(index));
        assertEquals(
                "Persistent store should be updated.",
                0,
                ChromeMultiInstancePersistentStore.readLastAccessedTime(index));
        assertEquals(
                "Persistent store should be updated.",
                0,
                ChromeMultiInstancePersistentStore.readClosureTime(index));
        assertEquals(
                "Persistent store should be updated.",
                SupportedProfileType.UNSET,
                ChromeMultiInstancePersistentStore.readProfileType(index));
        assertFalse(
                "Persistent store should be updated.",
                ChromeMultiInstancePersistentStore.readMarkedForDeletion(index));
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
        AllocatedIdInfo instanceIdInfo =
                mMultiInstanceManager.allocInstanceId(
                        passedId, activity.getTaskId(), preferNew, /* isIncognitoIntent= */ false);
        int index = instanceIdInfo.instanceId;

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
                                index,
                                SupportedProfileType.MIXED);
        if (pair == null) return INVALID_WINDOW_ID;

        int instanceId = pair.first;
        mMultiInstanceManager.createInstance(instanceId, activity);
        ChromeMultiInstancePersistentStore.writeTaskId(instanceId, activity.getTaskId());

        // Store minimal data to get the instance recognized.
        ChromeMultiInstancePersistentStore.writeActiveTabUrl(instanceId, "url" + instanceId);
        ChromeMultiInstancePersistentStore.writeTabCount(
                instanceId, /* normalTabCount= */ 1, /* incognitoTabCount= */ 0);
        return instanceId;
    }

    // Assert that the given task is new, and not in the task map.
    private void assertIsNewTask(int taskId) {
        for (int i = 0; i < mMultiInstanceManager.mMaxInstances; ++i) {
            assertNotEquals(taskId, ChromeMultiInstancePersistentStore.readTaskId(i));
        }
    }

    // Simulate a task is removed by swiping it away. Both the task and the associated activity
    // get destroyed. Task map gets updated. The persistent state file remains intact.
    private void removeTaskOnRecentsScreen(Activity activityForTask) {
        mMultiInstanceManager.updateTasksWithoutDestroyingActivity(
                INVALID_WINDOW_ID, activityForTask);
        destroyActivity(activityForTask);
    }

    // Simulate only an activity gets destroyed, leaving everything intact.
    private void softCloseInstance(Activity activity) {
        destroyActivity(activity);
    }

    private void destroyActivity(Activity activity) {
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.DESTROYED);
    }

    private void setupTwoInstances() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        // Allocate and create two instances.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mCurrentActivity, true));
        mMultiInstanceManager.initialize(0, TASK_ID_56, SupportedProfileType.MIXED);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63, true));
        var multiInstanceManager = createTestMultiInstanceManager(mTabbedActivityTask63);
        multiInstanceManager.initialize(1, TASK_ID_63, SupportedProfileType.MIXED);
        assertEquals(2, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testOpenWindow_opensAdjacently_WithRobustWindowManagementExperimental() {
        setupTwoInstances();
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                true);
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        mMultiInstanceManager.openWindow(INSTANCE_ID_2, NewWindowAppSource.WINDOW_MANAGER);

        verify(mCurrentActivity).startActivity(intentCaptor.capture());
        Intent intent = intentCaptor.getValue();
        assertNotEquals("Intent should not be null.", null, intent);
        assertEquals(
                "New window source extra is incorrect.",
                NewWindowAppSource.WINDOW_MANAGER,
                intent.getIntExtra(
                        IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, NewWindowAppSource.UNKNOWN));
        int flags = intent.getFlags();
        assertTrue(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT should be set.",
                (flags & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testOpenWindow_opensFullScreen_WithRobustWindowManagementExperimental() {
        setupTwoInstances();
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                false);
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        mMultiInstanceManager.openWindow(INSTANCE_ID_2, NewWindowAppSource.WINDOW_MANAGER);

        verify(mCurrentActivity).startActivity(intentCaptor.capture());
        Intent intent = intentCaptor.getValue();
        assertEquals(
                "New window source extra is incorrect.",
                NewWindowAppSource.WINDOW_MANAGER,
                intent.getIntExtra(
                        IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, NewWindowAppSource.UNKNOWN));
        assertNotEquals("Intent should not be null.", null, intent);
        int flags = intent.getFlags();
        assertFalse(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT should not be set.",
                (flags & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testOpenWindow_opensFullScreen_DifferentModel_RegularToIncognito() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        setupTwoInstances();
        ChromeMultiInstancePersistentStore.writeProfileType(0, SupportedProfileType.REGULAR);
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(INSTANCE_ID_2);
        ChromeMultiInstancePersistentStore.writeProfileType(
                INSTANCE_ID_2, SupportedProfileType.OFF_THE_RECORD);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        mMultiInstanceManager.openWindow(INSTANCE_ID_2, NewWindowAppSource.WINDOW_MANAGER);

        verify(mCurrentActivity).startActivity(intentCaptor.capture());
        Intent intent = intentCaptor.getValue();
        assertNotEquals("Intent should not be null.", null, intent);
        int flags = intent.getFlags();
        assertFalse(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT should not be set for different models.",
                (flags & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testOpenWindow_opensFullScreen_DifferentModel_IncognitoToRegular() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        setupActivityForCreateNewWindowIntent(mTabbedActivityTask62);
        when(mTabbedActivityTask62.isIncognitoWindow()).thenReturn(true);
        var manager = spy(createTestMultiInstanceManager(mTabbedActivityTask62));
        manager.mTestBuildInstancesList = true;

        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62, true));
        manager.initialize(0, TASK_ID_62, SupportedProfileType.OFF_THE_RECORD);

        ChromeMultiInstancePersistentStore.writeLastAccessedTime(INSTANCE_ID_2);
        ChromeMultiInstancePersistentStore.writeProfileType(
                INSTANCE_ID_2, SupportedProfileType.REGULAR);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        manager.openWindow(INSTANCE_ID_2, NewWindowAppSource.WINDOW_MANAGER);

        verify(mTabbedActivityTask62).startActivity(intentCaptor.capture());
        Intent intent = intentCaptor.getValue();
        assertNotEquals("Intent should not be null.", null, intent);
        int flags = intent.getFlags();
        assertFalse(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT should not be set for different models.",
                (flags & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testOpenWindow_opensAdjacently_SameModel_IncognitoToIncognito() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        setupActivityForCreateNewWindowIntent(mTabbedActivityTask62);
        when(mTabbedActivityTask62.isIncognitoWindow()).thenReturn(true);
        var manager = spy(createTestMultiInstanceManager(mTabbedActivityTask62));
        manager.mTestBuildInstancesList = true;

        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62, true));
        manager.initialize(0, TASK_ID_62, SupportedProfileType.OFF_THE_RECORD);

        ChromeMultiInstancePersistentStore.writeLastAccessedTime(INSTANCE_ID_2);
        ChromeMultiInstancePersistentStore.writeProfileType(
                INSTANCE_ID_2, SupportedProfileType.OFF_THE_RECORD);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        manager.openWindow(INSTANCE_ID_2, NewWindowAppSource.WINDOW_MANAGER);

        verify(mTabbedActivityTask62).startActivity(intentCaptor.capture());
        Intent intent = intentCaptor.getValue();
        assertNotEquals("Intent should not be null.", null, intent);
        int flags = intent.getFlags();
        assertTrue(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT should be set for same models.",
                (flags & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.INCOGNITO_AS_WINDOW_FULL_SCREEN)
    public void testOpenWindow_opensAdjacently_DifferentModel_WhenFeatureDisabled() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        setupTwoInstances();
        ChromeMultiInstancePersistentStore.writeProfileType(0, SupportedProfileType.REGULAR);
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(INSTANCE_ID_2);
        ChromeMultiInstancePersistentStore.writeProfileType(
                INSTANCE_ID_2, SupportedProfileType.OFF_THE_RECORD);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        mMultiInstanceManager.openWindow(INSTANCE_ID_2, NewWindowAppSource.WINDOW_MANAGER);

        verify(mCurrentActivity).startActivity(intentCaptor.capture());
        Intent intent = intentCaptor.getValue();
        assertNotEquals("Intent should not be null.", null, intent);
        int flags = intent.getFlags();
        assertTrue(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT should be set when feature is disabled.",
                (flags & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testCloseChromeWindowIfEmpty_closed() {
        DeviceInfo.setIsXrForTesting(true);
        mMultiInstanceManager.mTestBuildInstancesList = true;
        // Create an empty instance before asking it to close. The flag that provides permission to
        // close is enabled.
        assertEquals(INSTANCE_ID_1, allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask62, true));
        assertEquals(1, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Act.
        assertTrue(
                "Chrome instance should be closed.",
                mMultiInstanceManager.closeChromeWindowIfEmpty(INSTANCE_ID_1));

        verify(mMultiInstanceManager)
                .closeWindows(any(), eq(CloseWindowAppSource.NO_TABS_IN_WINDOW));
    }

    @Test
    public void testCloseChromeWindowIfEmpty() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        // Create an empty instance before asking it to close.
        assertEquals(INSTANCE_ID_1, allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask62, true));
        assertEquals(1, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());
        // Assume that Chrome is in a desktop window.
        when(mAppHeaderState.isInDesktopWindow()).thenReturn(true);

        // Act.
        assertTrue(
                "Chrome instance should be closed.",
                mMultiInstanceManager.closeChromeWindowIfEmpty(INSTANCE_ID_1));

        verify(mMultiInstanceManager)
                .closeWindows(any(), eq(CloseWindowAppSource.NO_TABS_IN_WINDOW));
    }

    @Test
    public void testCleanupIfLastInstance() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        assertEquals(
                "Failed to alloc INSTANCE_ID_1.",
                INSTANCE_ID_1,
                allocInstanceIndex(INSTANCE_ID_1, mTabbedActivityTask62, true));
        List<InstanceInfo> instanceInfo =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY);
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
        assertEquals(
                "Expected two instances.",
                2,
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        mMultiInstanceManager.cleanupSyncedTabGroupsIfLastInstance();
        // Verify this is not called a second time.
        verify(mTabGroupSyncService).getAllGroupIds();
    }

    @Test
    public void testCleanupSyncedTabGroupsIfOnlyInstance() {
        mMultiInstanceManager.mTestBuildInstancesList = true;

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
    public void testOpenWindow_TaskHasRunningActivity() {
        doTestOpenWindowWithValidTask(/* isActivityAlive= */ true);
    }

    @Test
    @Config(sdk = 30)
    public void testOpenWindow_TaskHasNoRunningActivity() {
        doTestOpenWindowWithValidTask(/* isActivityAlive= */ false);
    }

    @Test
    public void testWriteLastAccessedTime_InstanceCreation() {
        mMultiInstanceManager.mTestBuildInstancesList = true;

        // Simulate creation of activity |mTabbedActivityTask62| with index 0 and
        // |mTabbedActivityTask63| with index 1.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        // Advancing time by at least 1ms apart to record different instance access times.
        mFakeTimeTestRule.advanceMillis(1);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));

        long accessTime0 = ChromeMultiInstancePersistentStore.readLastAccessedTime(0);
        long accessTime1 = ChromeMultiInstancePersistentStore.readLastAccessedTime(1);

        List<InstanceInfo> instances =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY);

        // Verify the lastAccessedTime for both instances.
        assertEquals(
                "InstanceInfo.lastAccessedTime for instance0 is incorrect.",
                accessTime0,
                instances.get(0).lastAccessedTime);
        assertEquals(
                "InstanceInfo.lastAccessedTime for instance1 is incorrect.",
                accessTime1,
                instances.get(1).lastAccessedTime);
        assertTrue(
                "Access time for instance0 should be older than access time for instance1.",
                accessTime0 < accessTime1);
    }

    @Test
    public void testWriteLastAccessedTime_OnTopResumedActivityChanged() {
        mMultiInstanceManager.mTestBuildInstancesList = true;

        // Setup instance for |mTabbedActivityTask62| with index 0, make it the top resumed
        // activity.
        MultiInstanceManagerApi31 multiInstanceManager0 =
                createTestMultiInstanceManager(mTabbedActivityTask62);
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        multiInstanceManager0.initialize(0, TASK_ID_62, SupportedProfileType.MIXED);
        multiInstanceManager0.onTopResumedActivityChanged(true);
        long instance0CreationTime = ChromeMultiInstancePersistentStore.readLastAccessedTime(0);

        // Setup instance for |mTabbedActivityTask63| with index 1, make it the top resumed
        // activity.
        MultiInstanceManagerApi31 multiInstanceManager1 =
                createTestMultiInstanceManager(mTabbedActivityTask63);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));
        multiInstanceManager1.initialize(1, TASK_ID_63, SupportedProfileType.MIXED);
        multiInstanceManager0.onTopResumedActivityChanged(false);
        multiInstanceManager1.onTopResumedActivityChanged(true);
        long instance1CreationTime = ChromeMultiInstancePersistentStore.readLastAccessedTime(1);
        // Advance time by 1ms to record a different access time for the instances when the top
        // resumed activity changes.
        mFakeTimeTestRule.advanceMillis(1);
        // Resume instance0, so it becomes the top resumed activity.
        multiInstanceManager0.onTopResumedActivityChanged(true);
        multiInstanceManager1.onTopResumedActivityChanged(false);

        // Verify the lastAccessedTime for both instances.
        long accessTime0 = ChromeMultiInstancePersistentStore.readLastAccessedTime(0);
        long accessTime1 = ChromeMultiInstancePersistentStore.readLastAccessedTime(1);

        assertTrue(
                "Access time for instance0 is not updated.", accessTime0 > instance0CreationTime);
        assertFalse(
                "Access time for instance1 should not be updated.",
                accessTime1 > instance1CreationTime);
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
                ChromeMultiInstancePersistentStore.readTaskId(0));
        assertTrue(
                "SharedPref for tracking downgrade should be updated.",
                ChromeMultiInstancePersistentStore.readInstanceLimitDowngradeTriggered());

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
                ChromeMultiInstancePersistentStore.readTaskId(0));
        assertFalse(
                "SharedPref for tracking downgrade should not be updated.",
                MultiInstanceSharedPreferences.getInstance()
                        .readBoolean(
                                MultiInstancePreferenceKeys
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

    private void doTestOpenWindowWithValidTask(boolean isActivityAlive) {
        setupActivityForCreateNewWindowIntent(mTabbedActivityTask62);

        // Create the MultiInstanceManager for current activity = |mTabbedActivityTask62| and setup
        // another instance for |mTabbedActivityTask63|.
        MultiInstanceManagerApi31 multiInstanceManager62 =
                spy(createTestMultiInstanceManager(mTabbedActivityTask62));
        MultiInstanceManagerApi31 multiInstanceManager63 =
                spy(createTestMultiInstanceManager(mTabbedActivityTask63));

        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        multiInstanceManager62.initialize(0, TASK_ID_62, SupportedProfileType.MIXED);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));
        multiInstanceManager63.initialize(1, TASK_ID_63, SupportedProfileType.MIXED);

        // Setup AppTask's for both activities. Clear test AppTask ids that are set during the test
        // manager instantiation so that ids from the current mocked AppTasks are used.
        MultiWindowUtils.setAppTaskIdsForTesting(null);
        List<AppTask> appTasks =
                setupActivityManagerAppTasks(mTabbedActivityTask62, mTabbedActivityTask63);

        if (!isActivityAlive) {
            // Force destruction of |mTabbedActivityTask63|.
            destroyActivity(mTabbedActivityTask63);
        }

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.MultiWindowMode.InactiveInstanceRestore.AppSource2",
                                NewWindowAppSource.WINDOW_MANAGER)
                        .build();

        // Try to restore the instance in task from |mTabbedActivityTask62|.
        multiInstanceManager62.openWindow(1, NewWindowAppSource.WINDOW_MANAGER);

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
            verify(mRecentlyClosedTracker).onInstanceRestored(1);
            histogramWatcher.assertExpected();
        }
    }

    @Test
    public void testOpenNewWindow() {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        mMultiInstanceManager.openNewWindow(false);

        verify(mCurrentActivity).startActivity(intentCaptor.capture(), eq(null));
        Intent intent = intentCaptor.getValue();
        assertNotNull(intent.getComponent());
        assertEquals(
                "New window source extra is incorrect.",
                NewWindowAppSource.WINDOW_MANAGER,
                intent.getIntExtra(
                        IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE, NewWindowAppSource.UNKNOWN));
        assertEquals(
                "org.chromium.chrome.browser.ChromeTabbedActivity",
                intent.getComponent().getClassName());
    }

    @Test
    public void showInstanceCreationLimitMessage() {
        when(mCurrentActivity.getResources()).thenReturn(mock(Resources.class));

        mMultiInstanceManager.showInstanceCreationLimitMessage();

        ArgumentCaptor<PropertyModel> message = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher).enqueueWindowScopedMessage(message.capture(), eq(false));
        assertEquals(
                "Message identifier should match.",
                MessageIdentifier.MULTI_INSTANCE_CREATION_LIMIT,
                message.getValue().get(MessageBannerProperties.MESSAGE_IDENTIFIER));
    }

    @Test
    public void testShowInstanceCreationLimitMessage_SuppressesDuplicates() {
        when(mCurrentActivity.getResources()).thenReturn(mock(Resources.class));

        // First invocation enqueues.
        mMultiInstanceManager.showInstanceCreationLimitMessage();
        ArgumentCaptor<PropertyModel> message = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher).enqueueWindowScopedMessage(message.capture(), eq(false));
        reset(mMessageDispatcher);

        // Second invocation does not enqueue again because it is already enqueued.
        mMultiInstanceManager.showInstanceCreationLimitMessage();
        verify(mMessageDispatcher, never()).enqueueWindowScopedMessage(any(), anyBoolean());

        // Dismiss the message.
        message.getValue()
                .get(MessageBannerProperties.ON_DISMISSED)
                .onResult(DismissReason.GESTURE);

        // Third invocation now enqueues again because the first one was dismissed.
        mMultiInstanceManager.showInstanceCreationLimitMessage();
        verify(mMessageDispatcher).enqueueWindowScopedMessage(any(), eq(false));
    }

    @Test
    public void testShowNameWindowDialog_UsesCustomTitle() {
        Activity realActivity = Robolectric.setupActivity(Activity.class);
        var manager = createTestMultiInstanceManager(realActivity);
        manager.initialize(INSTANCE_ID_1, TASK_ID_56, SupportedProfileType.MIXED);

        final String customTitle = "Custom Title";
        final String defaultTitle = "Default Title";
        ChromeMultiInstancePersistentStore.writeCustomTitle(INSTANCE_ID_1, customTitle);
        ChromeMultiInstancePersistentStore.writeActiveTabTitle(INSTANCE_ID_1, defaultTitle);

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
        var manager = createTestMultiInstanceManager(realActivity);
        manager.initialize(INSTANCE_ID_1, TASK_ID_56, SupportedProfileType.MIXED);

        final String defaultTitle = "Default Title";
        ChromeMultiInstancePersistentStore.writeActiveTabTitle(INSTANCE_ID_1, defaultTitle);
        ChromeMultiInstancePersistentStore.writeCustomTitle(INSTANCE_ID_1, /* title= */ null);

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
        var manager = createTestMultiInstanceManager(realActivity);
        manager.initialize(INSTANCE_ID_1, TASK_ID_56, SupportedProfileType.MIXED);
        final String defaultTitle = "Default Title";
        ChromeMultiInstancePersistentStore.writeActiveTabTitle(INSTANCE_ID_1, defaultTitle);

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
                ChromeMultiInstancePersistentStore.readCustomTitle(INSTANCE_ID_1));
    }

    @Test
    public void testShowNameWindowDialog_DialogCallbackIgnoresDefaultTitle() {
        Activity realActivity = Robolectric.setupActivity(Activity.class);
        var manager = createTestMultiInstanceManager(realActivity);
        manager.initialize(INSTANCE_ID_1, TASK_ID_56, SupportedProfileType.MIXED);
        final String defaultTitle = "Default Title";
        ChromeMultiInstancePersistentStore.writeActiveTabTitle(INSTANCE_ID_1, defaultTitle);

        manager.showNameWindowDialog(NameWindowDialogSource.TAB_STRIP);

        Dialog dialog = ShadowDialog.getLatestDialog();
        assertTrue("Dialog should be showing.", dialog.isShowing());

        TextInputEditText editText = dialog.findViewById(R.id.title_input_text);
        editText.setText(defaultTitle);

        dialog.findViewById(R.id.positive_button).performClick();

        assertFalse("Dialog should be dismissed.", dialog.isShowing());
        assertNull(
                "Custom title should not be saved if identical to default title.",
                ChromeMultiInstancePersistentStore.readCustomTitle(INSTANCE_ID_1));
    }

    @Test
    public void testGetInstanceInfo_sortsByLastAccessedTime() {
        mMultiInstanceManager.mTestBuildInstancesList = true;

        // Current activity is mActivityTask56, managed by mMultiInstanceManager.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(0);
        mFakeTimeTestRule.advanceMillis(100);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(1);
        mFakeTimeTestRule.advanceMillis(100);
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(2);

        // Simulate simultaneous activity lifecycle changes for instances 0 and 1 that records
        // nearly same last accessed time.
        mFakeTimeTestRule.advanceMillis(100);
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(/* instanceId= */ 0);
        mFakeTimeTestRule.advanceMillis(1);
        ChromeMultiInstancePersistentStore.writeLastAccessedTime(/* instanceId= */ 1);

        // Get instance info from instance 0 as the current instance.
        List<InstanceInfo> instanceInfo =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY);

        // Verify that the current instance (0) is first, and the rest are sorted by last accessed
        // time.
        assertEquals("Current instance (0) should be first.", 0, instanceInfo.get(0).instanceId);
        assertEquals("Instance 1should be second.", 1, instanceInfo.get(1).instanceId);
        assertEquals("Instance 1 should be third.", 2, instanceInfo.get(2).instanceId);
    }

    @Test
    public void testOnStopWithNative_updatesClosureTime() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        mMultiInstanceManager.initialize(
                /* instanceId= */ 0, /* taskId= */ TASK_ID_56, SupportedProfileType.MIXED);
        long initialTime = ChromeMultiInstancePersistentStore.readClosureTime(/* instanceId= */ 0);

        mFakeTimeTestRule.advanceMillis(100);

        mMultiInstanceManager.onStopWithNative();
        long updatedTime = ChromeMultiInstancePersistentStore.readClosureTime(/* instanceId= */ 0);

        assertTrue("Closure time should be updated.", updatedTime > initialTime);
    }

    @Test
    public void testOnDestroy_notifiesInstancesClosed() {
        var manager1 = createTestMultiInstanceManager(mTabbedActivityTask62);
        var manager2 = createTestMultiInstanceManager(mTabbedActivityTask63);
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        manager1.initialize(
                /* instanceId= */ 0, /* taskId= */ TASK_ID_62, SupportedProfileType.MIXED);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));
        manager2.initialize(
                /* instanceId= */ 1, /* taskId= */ TASK_ID_63, SupportedProfileType.MIXED);
        long initialTime0 = ChromeMultiInstancePersistentStore.readClosureTime(/* instanceId= */ 0);

        mFakeTimeTestRule.advanceMillis(100);

        // Destroy an instance with non-zero tab count.
        when(mTabbedActivityTask62.isFinishing()).thenReturn(true);
        ChromeMultiInstancePersistentStore.writeTabCount(
                /* instanceId= */ 0, /* normalTabCount= */ 3, /* incognitoTabCount= */ 0);
        manager1.onDestroy();
        long closureTime0 = ChromeMultiInstancePersistentStore.readClosureTime(/* instanceId= */ 0);
        assertTrue("Closure time should be updated.", closureTime0 > initialTime0);

        // Destroy an instance with zero tabs.
        when(mTabbedActivityTask63.isFinishing()).thenReturn(true);
        ChromeMultiInstancePersistentStore.writeTabCount(
                /* instanceId= */ 1, /* normalTabCount= */ 0, /* incognitoTabCount= */ 0);
        manager2.onDestroy();
        long closureTime1 = ChromeMultiInstancePersistentStore.readClosureTime(/* instanceId= */ 1);
        assertEquals("Closure time should be updated.", 0, closureTime1);

        // Verify #onInstancesClosed is only invoked for the window that contains restorable regular
        // tabs.
        verify(mRecentlyClosedTracker).onInstancesClosed(any(), eq(false));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), eq(true));
    }

    @Test
    public void testOnDestroy_notifyInstancesClosedNotInvoked() {
        var manager1 = createTestMultiInstanceManager(mTabbedActivityTask62);
        var manager2 = createTestMultiInstanceManager(mTabbedActivityTask63);
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        manager1.initialize(
                /* instanceId= */ 0, /* taskId= */ TASK_ID_62, SupportedProfileType.MIXED);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));
        manager2.initialize(
                /* instanceId= */ 1, /* taskId= */ TASK_ID_63, SupportedProfileType.MIXED);

        // Simulate that the activity is being destroyed but task is still alive.
        when(mTabbedActivityTask62.isFinishing()).thenReturn(false);

        // Destroy an instance with non-zero tab count.
        ChromeMultiInstancePersistentStore.writeTabCount(
                /* instanceId= */ 0, /* normalTabCount= */ 3, /* incognitoTabCount= */ 0);
        manager1.onDestroy();

        // Simulate that the activity is being destroyed but task is still alive.
        when(mTabbedActivityTask63.isFinishing()).thenReturn(false);

        // Destroy an instance with zero tabs.
        ChromeMultiInstancePersistentStore.writeTabCount(
                /* instanceId= */ 1, /* normalTabCount= */ 0, /* incognitoTabCount= */ 0);
        manager2.onDestroy();

        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), anyBoolean());
    }

    @Test
    public void testOnDestroy_notifyInstancesClosedNotInvoked_incognitoWindow() {
        var manager1 = createTestMultiInstanceManager(mTabbedActivityTask62);
        var manager2 = createTestMultiInstanceManager(mTabbedActivityTask63);
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        manager1.initialize(
                /* instanceId= */ 0, /* taskId= */ TASK_ID_62, SupportedProfileType.MIXED);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));
        manager2.initialize(
                /* instanceId= */ 1, /* taskId= */ TASK_ID_63, SupportedProfileType.MIXED);

        // Destroy an instance with non-zero normal tab count.
        when(mTabbedActivityTask62.isFinishing()).thenReturn(true);
        ChromeMultiInstancePersistentStore.writeTabCount(
                /* instanceId= */ 0, /* normalTabCount= */ 3, /* incognitoTabCount= */ 3);
        manager1.onDestroy();

        when(mTabbedActivityTask63.isFinishing()).thenReturn(true);

        // Destroy an instance with zero normal tabs.
        when(mTabbedActivityTask62.isFinishing()).thenReturn(true);
        ChromeMultiInstancePersistentStore.writeTabCount(
                /* instanceId= */ 1, /* normalTabCount= */ 0, /* incognitoTabCount= */ 3);
        manager2.onDestroy();

        // Verify #onInstancesClosed is only invoked for the window that contains restorable regular
        // tabs.
        verify(mRecentlyClosedTracker).onInstancesClosed(any(), eq(false));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), eq(true));
    }

    @Test
    public void testOnDestroy_whenFinishing_makesInstanceNonRecoverable() {
        // Setup sData so that isRecoverable is supported.
        ChromeMultiInstancePersistentStore.sData = MultiInstanceData.getDefaultInstance();

        int instanceId = allocInstanceIndex(PASSED_ID_INVALID, mCurrentActivity);
        mMultiInstanceManager.initialize(instanceId, TASK_ID_56, SupportedProfileType.MIXED);

        assertTrue(
                "Instance should be recoverable initially.",
                ChromeMultiInstancePersistentStore.readCrashRecoveryData().stream()
                        .anyMatch(info -> info.windowId == instanceId));

        // Mock activity as finishing.
        when(mCurrentActivity.isFinishing()).thenReturn(true);

        // Call onDestroy.
        mMultiInstanceManager.onDestroy();

        // Verify task ID is NOT removed even if activity is finishing.
        assertEquals(
                "Task ID should NOT be removed in onDestroy.",
                TASK_ID_56,
                ChromeMultiInstancePersistentStore.readTaskId(instanceId));

        // Verify isRecoverable is cleared.
        assertFalse(
                "Instance should not be recoverable after onDestroy() when finishing.",
                ChromeMultiInstancePersistentStore.readCrashRecoveryData().stream()
                        .anyMatch(info -> info.windowId == instanceId));

        ChromeMultiInstancePersistentStore.sData = null;
    }

    @Test
    public void testOnDestroy_whenNotFinishing_keepsInstanceRecoverable() {
        // Setup sData so that isRecoverable is supported.
        ChromeMultiInstancePersistentStore.sData = MultiInstanceData.getDefaultInstance();

        int instanceId = allocInstanceIndex(PASSED_ID_INVALID, mCurrentActivity);
        mMultiInstanceManager.initialize(instanceId, TASK_ID_56, SupportedProfileType.MIXED);

        assertTrue(
                "Instance should be recoverable initially.",
                ChromeMultiInstancePersistentStore.readCrashRecoveryData().stream()
                        .anyMatch(info -> info.windowId == instanceId));

        // Mock activity as NOT finishing (e.g. system kill).
        when(mCurrentActivity.isFinishing()).thenReturn(false);

        // Call onDestroy.
        mMultiInstanceManager.onDestroy();

        // Verify task ID is NOT removed.
        assertEquals(
                "Task ID should NOT be removed.",
                TASK_ID_56,
                ChromeMultiInstancePersistentStore.readTaskId(instanceId));

        // Verify isRecoverable is NOT cleared.
        assertTrue(
                "Instance should still be recoverable after onDestroy() when not finishing.",
                ChromeMultiInstancePersistentStore.readCrashRecoveryData().stream()
                        .anyMatch(info -> info.windowId == instanceId));

        ChromeMultiInstancePersistentStore.sData = null;
    }

    @Test
    public void testOnStopWithNative_whenFinishing_makesInstanceNonRecoverable() {
        // Setup sData so that isRecoverable is supported.
        ChromeMultiInstancePersistentStore.sData = MultiInstanceData.getDefaultInstance();

        int instanceId = allocInstanceIndex(PASSED_ID_INVALID, mCurrentActivity);
        mMultiInstanceManager.initialize(instanceId, TASK_ID_56, SupportedProfileType.MIXED);

        assertTrue(
                "Instance should be recoverable initially.",
                ChromeMultiInstancePersistentStore.readCrashRecoveryData().stream()
                        .anyMatch(info -> info.windowId == instanceId));

        // Mock activity as finishing.
        when(mCurrentActivity.isFinishing()).thenReturn(true);

        // Call onStopWithNative.
        mMultiInstanceManager.onStopWithNative();

        // Verify isRecoverable is cleared.
        assertFalse(
                "Instance should not be recoverable after onStopWithNative() when finishing.",
                ChromeMultiInstancePersistentStore.readCrashRecoveryData().stream()
                        .anyMatch(info -> info.windowId == instanceId));

        ChromeMultiInstancePersistentStore.sData = null;
    }

    @Test
    public void testCloseWindow_makesInstanceNonRecoverable() {
        // Setup sData.
        ChromeMultiInstancePersistentStore.sData = MultiInstanceData.getDefaultInstance();

        int instanceId = allocInstanceIndex(PASSED_ID_INVALID, mCurrentActivity);
        mMultiInstanceManager.initialize(instanceId, TASK_ID_56, SupportedProfileType.MIXED);

        assertTrue(
                "Instance should be recoverable initially.",
                ChromeMultiInstancePersistentStore.readCrashRecoveryData().stream()
                        .anyMatch(info -> info.windowId == instanceId));

        // Call closeWindows with WINDOW_MANAGER source so it's not permanently deleted.
        mMultiInstanceManager.closeWindows(
                Collections.singletonList(instanceId), CloseWindowAppSource.WINDOW_MANAGER);

        // Verify marked for deletion.
        assertTrue(
                "Instance should be marked for deletion.",
                ChromeMultiInstancePersistentStore.readMarkedForDeletion(instanceId));

        // Verify isRecoverable is cleared.
        assertFalse(
                "Instance should not be recoverable after being marked for deletion.",
                ChromeMultiInstancePersistentStore.readCrashRecoveryData().stream()
                        .anyMatch(info -> info.windowId == instanceId));

        ChromeMultiInstancePersistentStore.sData = null;
    }
}
