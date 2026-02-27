// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
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
import org.chromium.base.Token;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
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
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.AllocatedIdInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.CloseWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.InstanceAllocationType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
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
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadataExtractor;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.LoadUrlParams;
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
@EnableFeatures({
    ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT,
    ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS
})
@DisableFeatures({
    ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
})
public class MultiInstanceManagerApi31UnitTest {
    private static final int INSTANCE_ID_1 = 1;
    private static final int INSTANCE_ID_2 = 2;
    private static final int NONEXISTENT_INSTANCE_ID = 4;
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
    private static final Token TAB_GROUP_ID1 = new Token(2L, 2L);
    private static final ArrayList<Map.Entry<Integer, String>> TAB_IDS_TO_URLS =
            new ArrayList<>(List.of(Map.entry(TAB_ID_1, "https://www.youtube.com/")));

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
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
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

    private final SettableMonotonicObservableSupplier<TabModelOrchestrator>
            mTabModelOrchestratorSupplier = ObservableSuppliers.createMonotonic();
    private final SettableMonotonicObservableSupplier<ModalDialogManager>
            mModalDialogManagerSupplier = ObservableSuppliers.createMonotonic();
    private final OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();
    private UnownedUserDataHost mUnownedUserDataHost;

    Activity mCurrentActivity;
    Activity[] mActivityPool;
    Activity[] mTabbedActivityPool;
    private TestMultiInstanceManagerApi31 mMultiInstanceManager;
    private int mNormalTabCount;
    private int mIncognitoTabCount;
    private ArrayList<Tab> mGroupedTabs;
    private TabGroupMetadata mTabGroupMetadata;

    private TestMultiInstanceManagerApi31 createTestMultiInstanceManager(Activity activity) {
        return new TestMultiInstanceManagerApi31(
                activity,
                mTabModelOrchestratorSupplier,
                mMultiWindowModeStateDispatcher,
                mActivityLifecycleDispatcher,
                mModalDialogManagerSupplier,
                mMenuOrKeyboardActionController,
                mDesktopWindowStateManagerSupplier,
                mTabReparentingDelegate);
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
                MonotonicObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
                MenuOrKeyboardActionController menuOrKeyboardActionController,
                Supplier<DesktopWindowStateManager> desktopWindowStateManagerSupplier,
                TabReparentingDelegate tabReparentingDelegate) {
            super(
                    activity,
                    tabModelOrchestratorSupplier,
                    multiWindowModeStateDispatcher,
                    activityLifecycleDispatcher,
                    modalDialogManagerSupplier,
                    menuOrKeyboardActionController,
                    desktopWindowStateManagerSupplier,
                    tabReparentingDelegate);
            setAppTaskIdsForTesting(mAppTaskIds);
        }

        private void createInstance(int instanceId, Activity activity) {
            MultiInstancePersistentStore.writeActiveTabUrl(
                    instanceId, "https://id-" + instanceId + ".com");
            ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
            updateTasksWithoutDestroyingActivity(instanceId, activity);
            addInstanceInfo(instanceId, activity.getTaskId());
        }

        private void setAdjacentInstance(Activity activity) {
            mAdjacentInstance = activity;
        }

        private void addInstanceInfo(int instanceId, int taskId) {
            MultiInstancePersistentStore.writeLastAccessedTime(instanceId);
            MultiInstancePersistentStore.writeProfileType(instanceId, SupportedProfileType.REGULAR);
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
                                MultiInstancePersistentStore.readActiveTabUrl(instanceId),
                                /* title= */ "",
                                /* customTitle= */ null,
                                /* tabCount= */ 0,
                                /* incognitoTabCount= */ 0,
                                /* isIncognitoSelected= */ false,
                                MultiInstancePersistentStore.readLastAccessedTime(instanceId),
                                MultiInstancePersistentStore.readClosureTime(instanceId)));
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
        mTabModelOrchestratorSupplier.set(mTabModelOrchestrator);
        mModalDialogManagerSupplier.set(mModalDialogManager);
        mUnownedUserDataHost = new UnownedUserDataHost();

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

        when(mActivityManager.getAppTasks()).thenReturn(new ArrayList());

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
        doNothing().when(mMultiInstanceManager).showTargetSelectorDialog(any(), anyInt(), anyInt());

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
        MultiInstanceManagerApi31.setAppTaskIdsForTesting(
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
        MultiInstancePersistentStore.writeLastAccessedTime(1);
        // These two writes can often use the same timestamp, and cause the result to be random.
        // Wait for the next millisecond to guarantee this doesn't happen.
        mFakeTimeTestRule.advanceMillis(1);
        MultiInstancePersistentStore.writeLastAccessedTime(2); // Accessed most recently.

        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask59));
        removeTaskOnRecentsScreen(mActivityTask59);

        MultiInstancePersistentStore.writeLastAccessedTime(1); // instance ID 1 is now the MRU.
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
        verify(mMultiInstanceManager, times(2))
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
        MultiInstancePersistentStore.writeTabCount(
                0, /* normalTabCount= */ 0, /* incognitoTabCount= */ 1);
        MultiInstancePersistentStore.writeTabCount(
                1, /* normalTabCount= */ 0, /* incognitoTabCount= */ 1);
        MultiInstancePersistentStore.writeTabCount(
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
        assertTrue(MultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 1));
        assertFalse(MultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 0));
        assertFalse(MultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 2));
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
        MultiInstancePersistentStore.writeProfileType(1, SupportedProfileType.OFF_THE_RECORD);

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
        long initialTime = MultiInstancePersistentStore.readClosureTime(/* instanceId= */ 1);
        mFakeTimeTestRule.advanceMillis(100);
        mMultiInstanceManager.closeWindows(
                Collections.singletonList(1), CloseWindowAppSource.WINDOW_MANAGER);

        // Verify that the instance marked for deletion is not considered usable.
        assertEquals(2, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());

        // Verify that closure time is updated.
        assertTrue(MultiInstancePersistentStore.readClosureTime(/* instanceId= */ 1) > initialTime);

        // Verify #onInstancesClosed is invoked.
        ArgumentCaptor<List<InstanceInfo>> captor = ArgumentCaptor.forClass(List.class);
        verify(mRecentlyClosedTracker).onInstancesClosed(captor.capture(), eq(false));

        // Verify the captured InstanceInfo.
        List<InstanceInfo> closedInstanceInfo = captor.getValue();
        assertEquals("There should be exactly 1 InstanceInfo.", 1, closedInstanceInfo.size());
        assertEquals("Instance ID should be 1.", 1, closedInstanceInfo.get(0).instanceId);

        // Verify the instance is correctly marked for deletion.
        assertTrue(MultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 1));
        assertFalse(MultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 0));
        assertFalse(MultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 2));

        // Subsequent restoration should update `markedForDeletion` instance state.
        mMultiInstanceManager.openWindow(1, NewWindowAppSource.WINDOW_MANAGER);
        List<InstanceInfo> instanceInfoList =
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY);
        assertEquals(3, instanceInfoList.size());
        assertFalse(MultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 1));
    }

    @Test
    public void testCloseWindows_OnInstancesClosedInvoked_MixedIncognitoAndRegularWindows() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        MultiInstancePersistentStore.writeTabCount(
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
        MultiInstancePersistentStore.writeTabCount(
                1, /* normalTabCount= */ 1, /* incognitoTabCount= */ 0);
        MultiInstancePersistentStore.writeActiveTabUrl(1, "chrome-native://newtab/");

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
                /* instanceId= */ 0,
                /* taskId= */ TASK_ID_56,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);
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
        assertTrue(MultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 0));
        assertTrue(MultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 1));
        assertTrue(MultiInstancePersistentStore.readMarkedForDeletion(/* instanceId= */ 2));

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
        MultiInstancePersistentStore.writeProfileType(0, SupportedProfileType.REGULAR);

        // Instance 1: Active, Incognito
        assertEquals(1, allocInstanceIndex(1, mTabbedActivityPool[1]));
        MultiInstancePersistentStore.writeProfileType(1, SupportedProfileType.OFF_THE_RECORD);

        // Instance 2: Inactive, Regular
        assertEquals(2, allocInstanceIndex(2, mTabbedActivityPool[2]));
        MultiInstancePersistentStore.writeProfileType(2, SupportedProfileType.REGULAR);
        removeTaskOnRecentsScreen(mTabbedActivityPool[2]);

        // Instance 3: Inactive, Incognito
        assertEquals(3, allocInstanceIndex(3, mTabbedActivityPool[3]));
        MultiInstancePersistentStore.writeProfileType(3, SupportedProfileType.OFF_THE_RECORD);
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
    public void testCurrentInstanceId() {
        // Ensure the single instance at non-zero position is handled okay.
        int expected = 2;
        assertEquals(expected, allocInstanceIndex(expected, mActivityTask56));
        mMultiInstanceManager.initialize(
                expected, TASK_ID_56, SupportedProfileType.MIXED, /* host= */ mUnownedUserDataHost);
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
                        mDesktopWindowStateManagerSupplier,
                        mTabReparentingDelegate);
        multiInstanceManager.initialize(
                INSTANCE_ID_1,
                TASK_ID_57,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);
        TabModelObserver tabModelObserver = multiInstanceManager.getTabModelObserverForTesting();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        final String customTitle = "My Custom Title";
        MultiInstancePersistentStore.writeCustomTitle(INSTANCE_ID_1, customTitle);

        triggerSelectTab(tabModelObserver, mTab1);
        assertFalse(
                "Normal tab should be selected",
                MultiInstancePersistentStore.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals(
                "Title should be from the active normal tab",
                TITLE1,
                MultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL1.getSpec(),
                MultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1));

        // Update url/title as a new normal tab is selected.
        triggerSelectTab(tabModelObserver, mTab2);
        assertFalse(
                "Normal tab should be selected",
                MultiInstancePersistentStore.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals(
                "Title should be from the active normal tab",
                TITLE2,
                MultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL2.getSpec(),
                MultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1));

        // Incognito tab doesn't affect url/title when selected.
        triggerSelectTab(tabModelObserver, mTab3);
        assertTrue(
                "Incognito tab should be selected",
                MultiInstancePersistentStore.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals(
                "Title should be from the active normal tab",
                TITLE2,
                MultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL2.getSpec(),
                MultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1));

        // Nulled-tab doesn't affect url/title either.
        triggerSelectTab(tabModelObserver, null);
        assertTrue(
                "Incognito tab should be selected",
                MultiInstancePersistentStore.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals(
                "Null tab should not affect the title",
                TITLE2,
                MultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1));
        assertEquals(
                "Null tab should not affect the URL",
                URL2.getSpec(),
                MultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1));
        assertEquals(
                "Custom title should not change when tab changes.",
                customTitle,
                MultiInstancePersistentStore.readCustomTitle(INSTANCE_ID_1));
    }

    @Test
    public void testRenameInstanceUpdatesCustomTitle() {
        final String newTitle = "My Renamed Window";
        final int instanceId = 2;
        mMultiInstanceManager.renameInstance(instanceId, newTitle);

        assertEquals(
                "Custom title should be updated in SharedPreferences.",
                newTitle,
                MultiInstancePersistentStore.readCustomTitle(instanceId));
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
                        mDesktopWindowStateManagerSupplier,
                        mTabReparentingDelegate);
        multiInstanceManager.initialize(
                INSTANCE_ID_1,
                TASK_ID_57,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);
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
                MultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                0,
                MultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_1));

        triggerAddTab(tabModelObserver, mTab2); // normal tab added
        assertEquals(
                normalTabMessage,
                2,
                MultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                0,
                MultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_1));

        triggerAddTab(tabModelObserver, mTab3); // incognito tab added
        assertEquals(
                normalTabMessage,
                2,
                MultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                1,
                MultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_1));

        triggerOnFinishingTabClosure(tabModelObserver, mTab1);
        assertEquals(
                normalTabMessage,
                1,
                MultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                1,
                MultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_1));

        triggerTabRemoved(tabModelObserver, mTab3);
        assertEquals(
                normalTabMessage,
                1,
                MultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                0,
                MultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_1));

        triggerTabRemoved(tabModelObserver, mTab2);
        assertEquals(
                normalTabMessage,
                0,
                MultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertEquals(
                incognitoTabMessage,
                0,
                MultiInstancePersistentStore.readIncognitoTabCount(INSTANCE_ID_1));
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
                        mDesktopWindowStateManagerSupplier,
                        mTabReparentingDelegate);
        multiInstanceManager.initialize(
                INSTANCE_ID_1,
                TASK_ID_57,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);
        TabModelObserver tabModelObserver = multiInstanceManager.getTabModelObserverForTesting();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);

        triggerAddTab(tabModelObserver, mTab1);
        triggerSelectTab(tabModelObserver, mTab1);
        assertEquals(
                "Title should be from the active normal tab",
                TITLE1,
                MultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL1.getSpec(),
                MultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1));

        triggerAddTab(tabModelObserver, mTab2);
        triggerSelectTab(tabModelObserver, mTab2);
        assertEquals(
                "Title should be from the active normal tab",
                TITLE2,
                MultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1));
        assertEquals(
                "URL should be from the active normal tab",
                URL2.getSpec(),
                MultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1));

        triggerOnFinishingTabClosure(tabModelObserver, mTab1);
        triggerTabRemoved(tabModelObserver, mTab2);
        assertEquals(
                "Tab count should be zero",
                0,
                MultiInstancePersistentStore.readNormalTabCount(INSTANCE_ID_1));
        assertTrue(
                "Title was not cleared",
                TextUtils.isEmpty(MultiInstancePersistentStore.readActiveTabTitle(INSTANCE_ID_1)));
        assertTrue(
                "URL was not cleared",
                TextUtils.isEmpty(MultiInstancePersistentStore.readActiveTabUrl(INSTANCE_ID_1)));
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
        MultiInstancePersistentStore.writeActiveTabUrl(index, /* url= */ "url");
        MultiInstancePersistentStore.writeActiveTabTitle(index, /* title= */ "title");
        MultiInstancePersistentStore.writeCustomTitle(index, /* title= */ "title");
        MultiInstancePersistentStore.writeTabCount(
                index, /* normalTabCount= */ 1, /* incognitoTabCount= */ 1);
        MultiInstancePersistentStore.writeTabCountForRelaunchSync(index, /* tabCount= */ 2);
        MultiInstancePersistentStore.writeIncognitoSelected(index, /* incognitoSelected= */ true);
        MultiInstancePersistentStore.writeLastAccessedTime(index);
        MultiInstancePersistentStore.writeClosureTime(index);
        MultiInstancePersistentStore.writeProfileType(
                index, /* profileType= */ SupportedProfileType.MIXED);
        MultiInstancePersistentStore.writeMarkedForDeletion(index, /* markedForDeletion= */ true);

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
                MultiInstancePersistentStore.readActiveTabUrl(index));
        assertNull(
                "Persistent store should be updated.",
                MultiInstancePersistentStore.readActiveTabTitle(index));
        assertNull(
                "Persistent store should be updated.",
                MultiInstancePersistentStore.readCustomTitle(index));
        assertEquals(
                "Persistent store should be updated.",
                0,
                MultiInstancePersistentStore.readNormalTabCount(index));
        assertEquals(
                "Persistent store should be updated.",
                0,
                MultiInstancePersistentStore.readTabCountForRelaunch(index));
        assertEquals(
                "Persistent store should be updated.",
                0,
                MultiInstancePersistentStore.readIncognitoTabCount(index));
        assertFalse(
                "Persistent store should be updated.",
                MultiInstancePersistentStore.readIncognitoSelected(index));
        assertEquals(
                "Persistent store should be updated.",
                0,
                MultiInstancePersistentStore.readLastAccessedTime(index));
        assertEquals(
                "Persistent store should be updated.",
                0,
                MultiInstancePersistentStore.readClosureTime(index));
        assertEquals(
                "Persistent store should be updated.",
                SupportedProfileType.UNSET,
                MultiInstancePersistentStore.readProfileType(index));
        assertFalse(
                "Persistent store should be updated.",
                MultiInstancePersistentStore.readMarkedForDeletion(index));
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
                                null,
                                mMismatchedIndicesHandler,
                                index);
        if (pair == null) return INVALID_WINDOW_ID;

        int instanceId = pair.first;
        mMultiInstanceManager.createInstance(instanceId, activity);
        MultiInstancePersistentStore.writeTaskId(instanceId, activity.getTaskId());

        // Store minimal data to get the instance recognized.
        MultiInstancePersistentStore.writeActiveTabUrl(instanceId, "url" + instanceId);
        MultiInstancePersistentStore.writeTabCount(
                instanceId, /* normalTabCount= */ 1, /* incognitoTabCount= */ 0);
        return instanceId;
    }

    // Assert that the given task is new, and not in the task map.
    private void assertIsNewTask(int taskId) {
        for (int i = 0; i < mMultiInstanceManager.mMaxInstances; ++i) {
            assertNotEquals(taskId, MultiInstancePersistentStore.readTaskId(i));
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
        mMultiInstanceManager.initialize(
                0, TASK_ID_56, SupportedProfileType.MIXED, /* host= */ mUnownedUserDataHost);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63, true));
        var multiInstanceManager = createTestMultiInstanceManager(mTabbedActivityTask63);
        multiInstanceManager.initialize(
                1, TASK_ID_63, SupportedProfileType.MIXED, /* host= */ mUnownedUserDataHost);
        assertEquals(2, mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());
    }

    private void setupMaxInstances() {
        mMultiInstanceManager.mTestBuildInstancesList = true;
        // Create max instances first before asking to move a tab from one to another.
        for (int index = 0; index < mMultiInstanceManager.mMaxInstances; ++index) {
            assertEquals(
                    index, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityPool[index], true));
        }
        assertEquals(
                mMultiInstanceManager.mMaxInstances,
                mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.ANY).size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testMoveTabsToNewWindow_validInput_opensAdjacently() {
        setupTwoInstances();
        List<Tab> tabs = List.of(mTab1, mTab2);
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                true);

        mMultiInstanceManager.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        tabs,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ true,
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testMoveTabsToNewWindow_validInput_opensFullScreen() {
        setupTwoInstances();
        List<Tab> tabs = List.of(mTab1, mTab2);

        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                false);

        mMultiInstanceManager.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        tabs,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ false,
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL)
    public void testMoveTabsToNewWindow_inMultiWindowMode_opensAdjacently() {
        setupTwoInstances();
        List<Tab> tabs = List.of(mTab1, mTab2);

        // Fieldtrial param to not open adjacently should be ignored.
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                false);

        mMultiInstanceManager.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        tabs,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ false,
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    public void testMoveTabGroupToNewWindow_validInput() {
        setupTwoInstances();

        mMultiInstanceManager.moveTabGroupToNewWindow(
                mTabGroupMetadata, NewWindowAppSource.KEYBOARD_SHORTCUT);

        verify(mTabReparentingDelegate)
                .reparentTabGroupToNewWindow(
                        mTabGroupMetadata,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ true,
                        NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    public void testMoveTabsToWindowByIdChecked_InvalidParams() {
        List<Tab> tabs = List.of(mTab1, mTab2);

        // destWindowId should have persisted instance state.
        assertThrows(
                AssertionError.class,
                () ->
                        mMultiInstanceManager.moveTabsToWindowByIdChecked(
                                /* destWindowId= */ NONEXISTENT_INSTANCE_ID,
                                tabs,
                                /* destTabIndex= */ 2,
                                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX));

        // destTabIndex and destGroupTabId should not both be specified when moving tabs to an
        // existing window.
        assertThrows(
                AssertionError.class,
                () ->
                        mMultiInstanceManager.moveTabsToWindowByIdChecked(
                                /* destWindowId= */ 1,
                                tabs,
                                /* destTabIndex= */ 1,
                                /* destGroupTabId= */ 2));
    }

    @Test
    public void testMoveTabsToWindowByIdChecked_toDestTabIndex() {
        // Setup.
        setupTwoInstances();

        // Act.
        List<Tab> tabs = List.of(mTab1, mTab2);
        int destTabIndex = 0;
        mMultiInstanceManager.moveTabsToWindowByIdChecked(
                /* destWindowId= */ 1,
                tabs,
                destTabIndex,
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToExistingWindow(
                        eq(mTabbedActivityTask63), eq(tabs), eq(destTabIndex), eq(-1));
    }

    @Test
    public void testMoveTabsToWindowByIdChecked_toDestTabGroup() {
        // Setup.
        setupTwoInstances();
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTab1.getTabGroupId()).thenReturn(null);
        when(mTab2.getTabGroupId()).thenReturn(null);

        // Act.
        mMultiInstanceManager.moveTabsToWindowByIdChecked(
                /* destWindowId= */ 1,
                tabs,
                /* destTabIndex= */ TabList.INVALID_TAB_INDEX,
                /* destGroupTabId= */ 3);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabsToExistingWindow(eq(mTabbedActivityTask63), eq(tabs), eq(-1), eq(3));
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
    public void testMoveTabsToNewWindow_atInstanceLimit() {
        setupMaxInstances();
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mCurrentActivity.getResources()).thenReturn(mock(Resources.class));

        // Act.
        mMultiInstanceManager.moveTabsToNewWindow(
                tabs, /* finalizeCallback= */ null, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify that tab reparenting is not initiated, and a message is shown.
        verify(mTabReparentingDelegate, never())
                .reparentTabsToNewWindow(any(), anyInt(), anyBoolean(), any(), anyInt());
        verify(mMultiInstanceManager).showInstanceCreationLimitMessage();
    }

    @Test
    public void testMoveTabGroupToNewWindow_atInstanceLimit() {
        setupMaxInstances();
        when(mCurrentActivity.getResources()).thenReturn(mock(Resources.class));

        // Act.
        mMultiInstanceManager.moveTabGroupToNewWindow(
                mTabGroupMetadata, NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify that tab group reparenting is not initiated, and a message is shown.
        verify(mTabReparentingDelegate, never())
                .reparentTabGroupToNewWindow(any(), anyInt(), anyBoolean(), anyInt());
        verify(mMultiInstanceManager).showInstanceCreationLimitMessage();
    }

    @Test
    public void testMoveTabGroupToWindowByIdChecked_toActiveDestWindow() {
        // Setup.
        setupTwoInstances();

        // Act.
        int destTabIndex = 0;
        mMultiInstanceManager.moveTabGroupToWindowByIdChecked(
                /* destWindowId= */ 1, mTabGroupMetadata, destTabIndex);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabGroupToExistingWindow(
                        eq(mTabbedActivityTask63), eq(mTabGroupMetadata), eq(destTabIndex));
    }

    @Test
    public void testMoveTabGroupToWindowByIdChecked_toInactiveDestWindow() {
        // Setup.
        setupTwoInstances();

        // Act.
        mMultiInstanceManager.moveTabGroupToWindowByIdChecked(
                NONEXISTENT_INSTANCE_ID, mTabGroupMetadata, /* destTabIndex= */ 0);

        // Verify.
        verify(mTabReparentingDelegate)
                .reparentTabGroupToNewWindow(
                        mTabGroupMetadata,
                        NONEXISTENT_INSTANCE_ID,
                        /* openAdjacently= */ true,
                        NewWindowAppSource.TAB_REPARENTING_TO_INSTANCE_WITH_NO_ACTIVITY);
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

        long accessTime0 = MultiInstancePersistentStore.readLastAccessedTime(0);
        long accessTime1 = MultiInstancePersistentStore.readLastAccessedTime(1);

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
        multiInstanceManager0.initialize(
                0, TASK_ID_62, SupportedProfileType.MIXED, /* host= */ mUnownedUserDataHost);
        multiInstanceManager0.onTopResumedActivityChanged(true);
        long instance0CreationTime = MultiInstancePersistentStore.readLastAccessedTime(0);

        // Setup instance for |mTabbedActivityTask63| with index 1, make it the top resumed
        // activity.
        MultiInstanceManagerApi31 multiInstanceManager1 =
                createTestMultiInstanceManager(mTabbedActivityTask63);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));
        multiInstanceManager1.initialize(
                1, TASK_ID_63, SupportedProfileType.MIXED, /* host= */ mUnownedUserDataHost);
        multiInstanceManager0.onTopResumedActivityChanged(false);
        multiInstanceManager1.onTopResumedActivityChanged(true);
        long instance1CreationTime = MultiInstancePersistentStore.readLastAccessedTime(1);
        // Advance time by 1ms to record a different access time for the instances when the top
        // resumed activity changes.
        mFakeTimeTestRule.advanceMillis(1);
        // Resume instance0, so it becomes the top resumed activity.
        multiInstanceManager0.onTopResumedActivityChanged(true);
        multiInstanceManager1.onTopResumedActivityChanged(false);

        // Verify the lastAccessedTime for both instances.
        long accessTime0 = MultiInstancePersistentStore.readLastAccessedTime(0);
        long accessTime1 = MultiInstancePersistentStore.readLastAccessedTime(1);

        assertTrue(
                "Access time for instance0 is not updated.", accessTime0 > instance0CreationTime);
        assertFalse(
                "Access time for instance1 should not be updated.",
                accessTime1 > instance1CreationTime);
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
        when(mCurrentActivity.getResources()).thenReturn(mock(Resources.class));

        mMultiInstanceManager.showInstanceRestorationMessage();
        verify(mMessageDispatcher).enqueueWindowScopedMessage(any(), eq(false));
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
                MultiInstancePersistentStore.readTaskId(0));
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
                MultiInstancePersistentStore.readTaskId(0));
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

    private void doTestOpenWindowWithValidTask(boolean isActivityAlive) {
        setupActivityForCreateNewWindowIntent(mTabbedActivityTask62);

        // Create the MultiInstanceManager for current activity = |mTabbedActivityTask62| and setup
        // another instance for |mTabbedActivityTask63|.
        MultiInstanceManagerApi31 multiInstanceManager62 =
                spy(createTestMultiInstanceManager(mTabbedActivityTask62));
        MultiInstanceManagerApi31 multiInstanceManager63 =
                spy(createTestMultiInstanceManager(mTabbedActivityTask63));

        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        multiInstanceManager62.initialize(
                0, TASK_ID_62, SupportedProfileType.MIXED, /* host= */ mUnownedUserDataHost);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));
        multiInstanceManager63.initialize(
                1, TASK_ID_63, SupportedProfileType.MIXED, /* host= */ mUnownedUserDataHost);

        // Setup AppTask's for both activities. Clear test AppTask ids that are set during the test
        // manager instantiation so that ids from the current mocked AppTasks are used.
        MultiInstanceManagerApi31.setAppTaskIdsForTesting(null);
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
    public void testCreateNewWindowIntent_Incognito_OpenNewIncognitoWindowExtraIsTrue() {
        Intent intent =
                mMultiInstanceManager.createNewWindowIntent(
                        /* isIncognito= */ true, NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        assertNotNull(intent);
        assertTrue(
                intent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, /* defaultValue= */ false));
    }

    @Test
    public void testCreateNewWindowIntent_NotIncognito_OpenNewIncognitoWindowExtraIsFalse() {
        Intent intent =
                mMultiInstanceManager.createNewWindowIntent(
                        /* isIncognito= */ false, NewWindowAppSource.BROWSER_WINDOW_CREATOR);
        assertNotNull(intent);
        assertFalse(
                intent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, /* defaultValue= */ true));
    }

    @Test
    public void
            testCreateNewWindowIntent_NonMultiWindowMode_ShouldNotOpenInAdjacentWindow_NoLaunchAdjacentFlag() {
        // Non-multi-window mode
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(false);
        when(mCurrentActivity.isInMultiWindowMode()).thenReturn(false);

        // The new window shouldn't be opened as an adjacent window.
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                false);

        Intent intent =
                mMultiInstanceManager.createNewWindowIntent(
                        /* isIncognito= */ false, NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        assertNotNull(intent);
        assertEquals(0, (intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT));
    }

    @Test
    public void
            testCreateNewWindowIntent_NonMultiWindowMode_ShouldOpenInAdjacentWindow_AddLaunchAdjacentFlag() {
        // Non-multi-window mode
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(false);
        when(mCurrentActivity.isInMultiWindowMode()).thenReturn(false);

        // The new window should be opened as an adjacent window.
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT_EXPERIMENTAL,
                MultiWindowUtils.OPEN_ADJACENTLY_PARAM,
                true);

        Intent intent =
                mMultiInstanceManager.createNewWindowIntent(
                        /* isIncognito= */ false, NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        assertNotNull(intent);
        assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testCreateNewWindowIntent_MultiWindowMode_AddLaunchAdjacentFlag() {
        when(mMultiWindowModeStateDispatcher.canEnterMultiWindowMode()).thenReturn(true);
        when(mMultiWindowModeStateDispatcher.isInMultiWindowMode()).thenReturn(true);
        when(mCurrentActivity.isInMultiWindowMode()).thenReturn(true);

        Intent intent =
                mMultiInstanceManager.createNewWindowIntent(
                        /* isIncognito= */ false, NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        assertNotNull(intent);
        assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
    }

    @Test
    public void testOpenNewWindow() {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        mMultiInstanceManager.openNewWindow(false);

        verify(mCurrentActivity).startActivity(intentCaptor.capture());
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
    public void testShowNameWindowDialog_UsesCustomTitle() {
        Activity realActivity = Robolectric.setupActivity(Activity.class);
        var manager = createTestMultiInstanceManager(realActivity);
        manager.initialize(
                INSTANCE_ID_1,
                TASK_ID_56,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);

        final String customTitle = "Custom Title";
        final String defaultTitle = "Default Title";
        MultiInstancePersistentStore.writeCustomTitle(INSTANCE_ID_1, customTitle);
        MultiInstancePersistentStore.writeActiveTabTitle(INSTANCE_ID_1, defaultTitle);

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
        manager.initialize(
                INSTANCE_ID_1,
                TASK_ID_56,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);

        final String defaultTitle = "Default Title";
        MultiInstancePersistentStore.writeActiveTabTitle(INSTANCE_ID_1, defaultTitle);
        MultiInstancePersistentStore.writeCustomTitle(INSTANCE_ID_1, /* title= */ null);

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
        manager.initialize(
                INSTANCE_ID_1,
                TASK_ID_56,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);
        final String defaultTitle = "Default Title";
        MultiInstancePersistentStore.writeActiveTabTitle(INSTANCE_ID_1, defaultTitle);

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
                MultiInstancePersistentStore.readCustomTitle(INSTANCE_ID_1));
    }

    @Test
    public void testShowNameWindowDialog_DialogCallbackIgnoresDefaultTitle() {
        Activity realActivity = Robolectric.setupActivity(Activity.class);
        var manager = createTestMultiInstanceManager(realActivity);
        manager.initialize(
                INSTANCE_ID_1,
                TASK_ID_56,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);
        final String defaultTitle = "Default Title";
        MultiInstancePersistentStore.writeActiveTabTitle(INSTANCE_ID_1, defaultTitle);

        manager.showNameWindowDialog(NameWindowDialogSource.TAB_STRIP);

        Dialog dialog = ShadowDialog.getLatestDialog();
        assertTrue("Dialog should be showing.", dialog.isShowing());

        TextInputEditText editText = dialog.findViewById(R.id.title_input_text);
        editText.setText(defaultTitle);

        dialog.findViewById(R.id.positive_button).performClick();

        assertFalse("Dialog should be dismissed.", dialog.isShowing());
        assertNull(
                "Custom title should not be saved if identical to default title.",
                MultiInstancePersistentStore.readCustomTitle(INSTANCE_ID_1));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testMoveTabsToOtherWindow_dialogShown() {
        MultiWindowUtils.setInstanceCountForTesting(2);
        List<Tab> tabs = List.of(mTab1, mTab2);

        mMultiInstanceManager.moveTabsToOtherWindow(tabs, NewWindowAppSource.MENU);

        verify(mMultiInstanceManager)
                .showTargetSelectorDialog(
                        any(),
                        eq(PersistedInstanceType.ANY),
                        eq(R.string.menu_move_tab_to_other_window));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testMoveTabsToOtherWindow_incognitoTabs_dialogShown() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setInstanceCountForTesting(1);
        MultiWindowUtils.setIncognitoInstanceCountForTesting(2);
        List<Tab> tabs = List.of(mTab1);
        when(mTab1.isIncognitoBranded()).thenReturn(true);

        mMultiInstanceManager.moveTabsToOtherWindow(tabs, NewWindowAppSource.MENU);

        verify(mMultiInstanceManager)
                .showTargetSelectorDialog(
                        any(),
                        eq(PersistedInstanceType.ACTIVE | PersistedInstanceType.OFF_THE_RECORD),
                        eq(R.string.menu_move_tab_to_other_window));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testMoveTabsToOtherWindow_incognitoTabs_dialogHidden() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setIncognitoInstanceCountForTesting(1);
        List<Tab> tabs = List.of(mTab1);
        when(mTab1.isIncognitoBranded()).thenReturn(true);

        mMultiInstanceManager.moveTabsToOtherWindow(tabs, NewWindowAppSource.MENU);

        verify(mMultiInstanceManager, never())
                .showTargetSelectorDialog(
                        any(), anyInt(), eq(R.string.menu_move_tab_to_other_window));
        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        tabs,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ true,
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.MENU);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testMoveTabsToOtherWindow_regularTabs_dialogShown() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setInstanceCountForTesting(2);
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTab1.isIncognitoBranded()).thenReturn(false);

        mMultiInstanceManager.moveTabsToOtherWindow(tabs, NewWindowAppSource.MENU);

        verify(mMultiInstanceManager)
                .showTargetSelectorDialog(
                        any(),
                        eq(PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR),
                        eq(R.string.menu_move_tab_to_other_window));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testMoveTabsToOtherWindow_regularTabs_dialogHidden() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setInstanceCountForTesting(1);
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTab1.isIncognitoBranded()).thenReturn(false);

        mMultiInstanceManager.moveTabsToOtherWindow(tabs, NewWindowAppSource.MENU);

        verify(mMultiInstanceManager, never())
                .showTargetSelectorDialog(
                        any(), anyInt(), eq(R.string.menu_move_tab_to_other_window));
        verify(mTabReparentingDelegate)
                .reparentTabsToNewWindow(
                        tabs,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ true,
                        /* finalizeCallback= */ null,
                        NewWindowAppSource.MENU);
    }

    @Test
    public void testOpenUrlInOtherWindow_fromRegularWindow_dialogShown() {
        MultiWindowUtils.setInstanceCountForTesting(2);
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));

        mMultiInstanceManager.openUrlInOtherWindow(
                urlParams,
                /* parentTabId= */ 1,
                /* preferNew= */ false,
                PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR);

        verify(mMultiInstanceManager)
                .showTargetSelectorDialog(
                        any(),
                        eq(PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR),
                        eq(R.string.contextmenu_open_in_other_window));
    }

    @Test
    public void testOpenUrlInOtherWindow_fromIncognitoWindow_dialogShown() {
        MultiWindowUtils.setIncognitoInstanceCountForTesting(2);
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));

        mMultiInstanceManager.openUrlInOtherWindow(
                urlParams,
                /* parentTabId= */ 1,
                /* preferNew= */ false,
                PersistedInstanceType.ACTIVE | PersistedInstanceType.OFF_THE_RECORD);

        verify(mMultiInstanceManager)
                .showTargetSelectorDialog(
                        any(),
                        eq(PersistedInstanceType.ACTIVE | PersistedInstanceType.OFF_THE_RECORD),
                        eq(R.string.contextmenu_open_in_other_window));
    }

    @Test
    public void testOpenUrlInOtherWindow_fromRegularWindow_dialogHidden() {
        MultiWindowUtils.setInstanceCountForTesting(1);
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));

        mMultiInstanceManager.openUrlInOtherWindow(
                urlParams,
                /* parentTabId= */ 1,
                /* preferNew= */ false,
                PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR);

        verify(mMultiInstanceManager, never())
                .showTargetSelectorDialog(
                        any(), anyInt(), eq(R.string.contextmenu_open_in_other_window));
        verify(mMultiInstanceManager, never()).showInstanceCreationLimitMessage();
        var intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mCurrentActivity).startActivity(intentCaptor.capture());
        assertEquals(
                "New window source extra is incorrect.",
                NewWindowAppSource.URL_LAUNCH,
                intentCaptor
                        .getValue()
                        .getIntExtra(
                                IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE,
                                NewWindowAppSource.UNKNOWN));
    }

    @Test
    public void testOpenUrlInOtherWindow_fromIncognitoWindow_dialogHidden() {
        MultiWindowUtils.setIncognitoInstanceCountForTesting(1);
        // Regular instance count should be irrelevant.
        MultiWindowUtils.setInstanceCountForTesting(3);
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));

        mMultiInstanceManager.openUrlInOtherWindow(
                urlParams,
                /* parentTabId= */ 1,
                /* preferNew= */ false,
                PersistedInstanceType.ACTIVE | PersistedInstanceType.OFF_THE_RECORD);

        verify(mMultiInstanceManager, never())
                .showTargetSelectorDialog(
                        any(), anyInt(), eq(R.string.contextmenu_open_in_other_window));
        verify(mMultiInstanceManager, never()).showInstanceCreationLimitMessage();
        var intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mCurrentActivity).startActivity(intentCaptor.capture());
        assertEquals(
                "New window source extra is incorrect.",
                NewWindowAppSource.URL_LAUNCH,
                intentCaptor
                        .getValue()
                        .getIntExtra(
                                IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE,
                                NewWindowAppSource.UNKNOWN));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testMoveTabGroupToOtherWindow_dialogShown() {
        MultiWindowUtils.setInstanceCountForTesting(2);

        mMultiInstanceManager.moveTabGroupToOtherWindow(
                getTabGroupMetadata(/* isIncognito= */ false), NewWindowAppSource.MENU);

        verify(mMultiInstanceManager)
                .showTargetSelectorDialog(
                        any(),
                        eq(PersistedInstanceType.ANY),
                        eq(R.string.menu_move_group_to_other_window));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testMoveTabGroupToOtherWindow_incognitoTabs_dialogShown() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setInstanceCountForTesting(1);
        MultiWindowUtils.setIncognitoInstanceCountForTesting(2);

        mMultiInstanceManager.moveTabGroupToOtherWindow(
                getTabGroupMetadata(/* isIncognito= */ true), NewWindowAppSource.MENU);

        verify(mMultiInstanceManager)
                .showTargetSelectorDialog(
                        any(),
                        eq(PersistedInstanceType.ACTIVE | PersistedInstanceType.OFF_THE_RECORD),
                        eq(R.string.menu_move_group_to_other_window));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testMoveTabGroupToOtherWindow_incognitoTabs_dialogHidden() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setIncognitoInstanceCountForTesting(1);
        TabGroupMetadata tabGroupMetadata = getTabGroupMetadata(/* isIncognito= */ true);
        when(mTab1.isIncognitoBranded()).thenReturn(true);

        mMultiInstanceManager.moveTabGroupToOtherWindow(tabGroupMetadata, NewWindowAppSource.MENU);

        verify(mMultiInstanceManager, never())
                .showTargetSelectorDialog(
                        any(), anyInt(), eq(R.string.menu_move_group_to_other_window));
        verify(mTabReparentingDelegate)
                .reparentTabGroupToNewWindow(
                        tabGroupMetadata,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ true,
                        NewWindowAppSource.MENU);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testMoveTabGroupToOtherWindow_regularTabs_dialogShown() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setInstanceCountForTesting(2);

        mMultiInstanceManager.moveTabGroupToOtherWindow(
                getTabGroupMetadata(/* isIncognito= */ false), NewWindowAppSource.MENU);

        verify(mMultiInstanceManager)
                .showTargetSelectorDialog(
                        any(),
                        eq(PersistedInstanceType.ACTIVE | PersistedInstanceType.REGULAR),
                        eq(R.string.menu_move_group_to_other_window));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testMoveTabGroupToOtherWindow_regularTabs_dialogHidden() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        MultiWindowUtils.setInstanceCountForTesting(1);
        TabGroupMetadata tabGroupMetadata = getTabGroupMetadata(/* isIncognito= */ false);

        mMultiInstanceManager.moveTabGroupToOtherWindow(tabGroupMetadata, NewWindowAppSource.MENU);

        verify(mMultiInstanceManager, never())
                .showTargetSelectorDialog(
                        any(), anyInt(), eq(R.string.menu_move_group_to_other_window));
        verify(mTabReparentingDelegate)
                .reparentTabGroupToNewWindow(
                        tabGroupMetadata,
                        INVALID_WINDOW_ID,
                        /* openAdjacently= */ true,
                        NewWindowAppSource.MENU);
    }

    @Test
    public void testGetInstanceInfo_sortsByLastAccessedTime() {
        mMultiInstanceManager.mTestBuildInstancesList = true;

        // Current activity is mActivityTask56, managed by mMultiInstanceManager.
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));
        MultiInstancePersistentStore.writeLastAccessedTime(0);
        mFakeTimeTestRule.advanceMillis(100);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask57));
        MultiInstancePersistentStore.writeLastAccessedTime(1);
        mFakeTimeTestRule.advanceMillis(100);
        assertEquals(2, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask58));
        MultiInstancePersistentStore.writeLastAccessedTime(2);

        // Simulate simultaneous activity lifecycle changes for instances 0 and 1 that records
        // nearly same last accessed time.
        mFakeTimeTestRule.advanceMillis(100);
        MultiInstancePersistentStore.writeLastAccessedTime(/* instanceId= */ 0);
        mFakeTimeTestRule.advanceMillis(1);
        MultiInstancePersistentStore.writeLastAccessedTime(/* instanceId= */ 1);

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
                /* instanceId= */ 0,
                /* taskId= */ TASK_ID_56,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);
        long initialTime = MultiInstancePersistentStore.readClosureTime(/* instanceId= */ 0);

        mFakeTimeTestRule.advanceMillis(100);

        mMultiInstanceManager.onStopWithNative();
        long updatedTime = MultiInstancePersistentStore.readClosureTime(/* instanceId= */ 0);

        assertTrue("Closure time should be updated.", updatedTime > initialTime);
    }

    @Test
    public void testOnDestroy_notifiesInstancesClosed() {
        var manager1 = createTestMultiInstanceManager(mTabbedActivityTask62);
        var manager2 = createTestMultiInstanceManager(mTabbedActivityTask63);
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask62));
        manager1.initialize(
                /* instanceId= */ 0,
                /* taskId= */ TASK_ID_62,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));
        manager2.initialize(
                /* instanceId= */ 1,
                /* taskId= */ TASK_ID_63,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);
        long initialTime0 = MultiInstancePersistentStore.readClosureTime(/* instanceId= */ 0);

        mFakeTimeTestRule.advanceMillis(100);

        // Destroy an instance with non-zero tab count.
        when(mTabbedActivityTask62.isFinishing()).thenReturn(true);
        MultiInstancePersistentStore.writeTabCount(
                /* instanceId= */ 0, /* normalTabCount= */ 3, /* incognitoTabCount= */ 0);
        manager1.onDestroy();
        long closureTime0 = MultiInstancePersistentStore.readClosureTime(/* instanceId= */ 0);
        assertTrue("Closure time should be updated.", closureTime0 > initialTime0);

        // Destroy an instance with zero tabs.
        when(mTabbedActivityTask63.isFinishing()).thenReturn(true);
        MultiInstancePersistentStore.writeTabCount(
                /* instanceId= */ 1, /* normalTabCount= */ 0, /* incognitoTabCount= */ 0);
        manager2.onDestroy();
        long closureTime1 = MultiInstancePersistentStore.readClosureTime(/* instanceId= */ 1);
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
                /* instanceId= */ 0,
                /* taskId= */ TASK_ID_62,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));
        manager2.initialize(
                /* instanceId= */ 1,
                /* taskId= */ TASK_ID_63,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);

        // Simulate that the activity is being destroyed but task is still alive.
        when(mTabbedActivityTask62.isFinishing()).thenReturn(false);

        // Destroy an instance with non-zero tab count.
        MultiInstancePersistentStore.writeTabCount(
                /* instanceId= */ 0, /* normalTabCount= */ 3, /* incognitoTabCount= */ 0);
        manager1.onDestroy();

        // Simulate that the activity is being destroyed but task is still alive.
        when(mTabbedActivityTask63.isFinishing()).thenReturn(false);

        // Destroy an instance with zero tabs.
        MultiInstancePersistentStore.writeTabCount(
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
                /* instanceId= */ 0,
                /* taskId= */ TASK_ID_62,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);
        assertEquals(1, allocInstanceIndex(PASSED_ID_INVALID, mTabbedActivityTask63));
        manager2.initialize(
                /* instanceId= */ 1,
                /* taskId= */ TASK_ID_63,
                SupportedProfileType.MIXED,
                /* host= */ mUnownedUserDataHost);

        // Destroy an instance with non-zero normal tab count.
        when(mTabbedActivityTask62.isFinishing()).thenReturn(true);
        MultiInstancePersistentStore.writeTabCount(
                /* instanceId= */ 0, /* normalTabCount= */ 3, /* incognitoTabCount= */ 3);
        manager1.onDestroy();

        when(mTabbedActivityTask63.isFinishing()).thenReturn(true);

        // Destroy an instance with zero normal tabs.
        when(mTabbedActivityTask62.isFinishing()).thenReturn(true);
        MultiInstancePersistentStore.writeTabCount(
                /* instanceId= */ 1, /* normalTabCount= */ 0, /* incognitoTabCount= */ 3);
        manager2.onDestroy();

        // Verify #onInstancesClosed is only invoked for the window that contains restorable regular
        // tabs.
        verify(mRecentlyClosedTracker).onInstancesClosed(any(), eq(false));
        verify(mRecentlyClosedTracker, never()).onInstancesClosed(any(), eq(true));
    }

    private TabGroupMetadata getTabGroupMetadata(boolean isIncognito) {
        return new TabGroupMetadata(
                TAB_ID_1,
                /* sourceWindowId= */ 1,
                TAB_GROUP_ID1,
                TAB_IDS_TO_URLS,
                /* tabGroupColor= */ 0,
                TITLE1,
                /* mhtmlTabTitle= */ null,
                /* tabGroupCollapsed= */ true,
                /* isGroupShared= */ false,
                isIncognito);
    }
}
