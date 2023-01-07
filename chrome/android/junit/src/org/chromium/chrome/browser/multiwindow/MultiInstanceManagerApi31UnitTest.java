// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.text.TextUtils;
import android.util.Pair;
import android.util.SparseArray;
import android.util.SparseBooleanArray;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorFactory;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Unit tests for {@link MultiInstanceManagerApi31}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class MultiInstanceManagerApi31UnitTest {
    private static final int INVALID_INSTANCE_ID = MultiInstanceManagerApi31.INVALID_INSTANCE_ID;
    private static final int INSTANCE_ID_1 = 1;
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

    private static final String TITLE1 = "title1";
    private static final String TITLE2 = "title2";
    private static final String TITLE3 = "title3";
    private static final GURL URL1 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
    private static final GURL URL2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);
    private static final GURL URL3 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_3);

    private TestMultiInstanceManagerApi31 mMultiInstanceManager;
    @Mock
    MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock
    ObservableSupplier<TabModelOrchestrator> mTabModelOrchestratorSupplier;
    @Mock
    TabModelOrchestrator mTabModelOrchestrator;
    @Mock
    ObservableSupplier<ModalDialogManager> mModalDialogManagerSupplier;
    @Mock
    ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    MenuOrKeyboardActionController mMenuOrKeyboardActionController;

    @Mock
    TabModelSelectorBase mTabModelSelector;
    @Mock
    TabModel mNormalTabModel;
    @Mock
    TabModel mIncognitoTabModel;
    @Mock
    Tab mTab1;
    @Mock
    Tab mTab2;
    @Mock
    Tab mTab3;

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
    @Mock
    Activity mActivityTask62;

    Activity mCurrentActivity;

    Activity[] mActivityPool;

    private int mNormalTabCount;
    private int mIncognitoTabCount;

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
                ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
                MenuOrKeyboardActionController menuOrKeyboardActionController) {
            super(activity, tabModelOrchestratorSupplier, multiWindowModeStateDispatcher,
                    activityLifecycleDispatcher, modalDialogManagerSupplier,
                    menuOrKeyboardActionController);
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
        protected boolean isRunningInAdjacentWindow(
                SparseBooleanArray visibleTasks, Activity activity) {
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
        mCurrentActivity = mActivityTask56;
        TabWindowManagerSingleton.setTabModelSelectorFactoryForTesting(
                sMockTabModelSelectorFactory);
        mMultiInstanceManager =
                new TestMultiInstanceManagerApi31(mCurrentActivity, mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher, mActivityLifecycleDispatcher,
                        mModalDialogManagerSupplier, mMenuOrKeyboardActionController);
        ApplicationStatus.onStateChangeForTesting(mCurrentActivity, ActivityState.CREATED);
        SharedPreferencesManager.getInstance().removeKeysWithPrefix(
                ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
    }

    @After
    public void tearDown() {
        SharedPreferencesManager.getInstance().removeKeysWithPrefix(
                ChromePreferenceKeys.MULTI_INSTANCE_TASK_MAP);
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        ApplicationStatus.destroyForJUnitTests();
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
    public void testAllocInstanceId_ignoreWrongPassedInstanceID() {
        assertEquals(0, allocInstanceIndex(PASSED_ID_INVALID, mActivityTask56));

        // Go through the ordinary allocation logic if the passed ID is out of range.
        assertEquals(1, allocInstanceIndex(10, mActivityTask59));
    }

    @Test
    @SmallTest
    @UiThreadTest
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
        assertEquals(finalIndex,
                allocInstanceIndex(PASSED_ID_INVALID, mActivityTask61, /*preferNew=*/true));
    }

    @Test
    @SmallTest
    @UiThreadTest
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

    @Test
    @SmallTest
    @UiThreadTest
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
    @UiThreadTest
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
                new MultiInstanceManagerApi31(mCurrentActivity, mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher, mActivityLifecycleDispatcher,
                        mModalDialogManagerSupplier, mMenuOrKeyboardActionController);
        multiInstanceManager.initialize(INSTANCE_ID_1, TASK_ID_57);
        TabModelObserver tabModelObserver = multiInstanceManager.getTabModelObserverForTesting();

        triggerSelectTab(tabModelObserver, mTab1);
        assertFalse("Normal tab should be selected",
                MultiInstanceManagerApi31.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals("Title should be from the active normal tab", TITLE1,
                MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1));
        assertEquals("URL should be from the active normal tab", URL1.getSpec(),
                MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1));

        // Update url/title as a new normal tab is selected.
        triggerSelectTab(tabModelObserver, mTab2);
        assertFalse("Normal tab should be selected",
                MultiInstanceManagerApi31.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals("Title should be from the active normal tab", TITLE2,
                MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1));
        assertEquals("URL should be from the active normal tab", URL2.getSpec(),
                MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1));

        // Incognito tab doesn't affect url/title when selected.
        triggerSelectTab(tabModelObserver, mTab3);
        assertTrue("Incognito tab should be selected",
                MultiInstanceManagerApi31.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals("Title should be from the active normal tab", TITLE2,
                MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1));
        assertEquals("URL should be from the active normal tab", URL2.getSpec(),
                MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1));

        // Nulled-tab doesn't affect url/title either.
        triggerSelectTab(tabModelObserver, null);
        assertTrue("Incognito tab should be selected",
                MultiInstanceManagerApi31.readIncognitoSelected(INSTANCE_ID_1));
        assertEquals("Null tab should not affect the title", TITLE2,
                MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1));
        assertEquals("Null tab should not affect the URL", URL2.getSpec(),
                MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1));
    }

    @Test
    @SmallTest
    @UiThreadTest
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
                new MultiInstanceManagerApi31(mCurrentActivity, mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher, mActivityLifecycleDispatcher,
                        mModalDialogManagerSupplier, mMenuOrKeyboardActionController);
        multiInstanceManager.initialize(INSTANCE_ID_1, TASK_ID_57);
        TabModelObserver tabModelObserver = multiInstanceManager.getTabModelObserverForTesting();

        when(mTab1.isIncognito()).thenReturn(false);
        when(mTab2.isIncognito()).thenReturn(false);
        when(mTab3.isIncognito()).thenReturn(true);

        final String normalTabMessage = "Normal tab count does not match";
        final String incognitoTabMessage = "Normal tab count does not match";
        triggerAddTab(tabModelObserver, mTab1); // normal tab added
        assertEquals(normalTabMessage, 1, MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertEquals(incognitoTabMessage, 0,
                MultiInstanceManagerApi31.readIncognitoTabCount(INSTANCE_ID_1));

        triggerAddTab(tabModelObserver, mTab2); // normal tab added
        assertEquals(normalTabMessage, 2, MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertEquals(incognitoTabMessage, 0,
                MultiInstanceManagerApi31.readIncognitoTabCount(INSTANCE_ID_1));

        triggerAddTab(tabModelObserver, mTab3); // incognito tab added
        assertEquals(normalTabMessage, 2, MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertEquals(incognitoTabMessage, 1,
                MultiInstanceManagerApi31.readIncognitoTabCount(INSTANCE_ID_1));

        triggerOnFinishingTabClosure(tabModelObserver, mTab1);
        assertEquals(normalTabMessage, 1, MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertEquals(incognitoTabMessage, 1,
                MultiInstanceManagerApi31.readIncognitoTabCount(INSTANCE_ID_1));

        triggerTabRemoved(tabModelObserver, mTab3);
        assertEquals(normalTabMessage, 1, MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertEquals(incognitoTabMessage, 0,
                MultiInstanceManagerApi31.readIncognitoTabCount(INSTANCE_ID_1));

        triggerTabRemoved(tabModelObserver, mTab2);
        assertEquals(normalTabMessage, 0, MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertEquals(incognitoTabMessage, 0,
                MultiInstanceManagerApi31.readIncognitoTabCount(INSTANCE_ID_1));
    }

    @Test
    @SmallTest
    @UiThreadTest
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
                new MultiInstanceManagerApi31(mCurrentActivity, mTabModelOrchestratorSupplier,
                        mMultiWindowModeStateDispatcher, mActivityLifecycleDispatcher,
                        mModalDialogManagerSupplier, mMenuOrKeyboardActionController);
        multiInstanceManager.initialize(INSTANCE_ID_1, TASK_ID_57);
        TabModelObserver tabModelObserver = multiInstanceManager.getTabModelObserverForTesting();

        triggerAddTab(tabModelObserver, mTab1);
        triggerSelectTab(tabModelObserver, mTab1);
        assertEquals("Title should be from the active normal tab", TITLE1,
                MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1));
        assertEquals("URL should be from the active normal tab", URL1.getSpec(),
                MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1));

        triggerAddTab(tabModelObserver, mTab2);
        triggerSelectTab(tabModelObserver, mTab2);
        assertEquals("Title should be from the active normal tab", TITLE2,
                MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1));
        assertEquals("URL should be from the active normal tab", URL2.getSpec(),
                MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1));

        triggerOnFinishingTabClosure(tabModelObserver, mTab1);
        triggerTabRemoved(tabModelObserver, mTab2);
        assertEquals("Tab count should be zero", 0,
                MultiInstanceManagerApi31.readTabCount(INSTANCE_ID_1));
        assertTrue("Title was not cleared",
                TextUtils.isEmpty(MultiInstanceManagerApi31.readTitle(INSTANCE_ID_1)));
        assertTrue("URL was not cleared",
                TextUtils.isEmpty(MultiInstanceManagerApi31.readUrl(INSTANCE_ID_1)));
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
        tabModelObserver.didAddTab(tab, 0, 0);
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
        return allocInstanceIndex(passedId, activity, /*preferNew=*/false);
    }

    private int allocInstanceIndex(int passedId, Activity activity, boolean preferNew) {
        int index =
                mMultiInstanceManager.allocInstanceId(passedId, activity.getTaskId(), preferNew);

        // Does what TabModelOrchestrator.createTabModels() would do to simulate production code.
        Pair<Integer, TabModelSelector> pair =
                TabWindowManagerSingleton.getInstance().requestSelector(
                        activity, null, null, index);
        if (pair == null) return INVALID_INSTANCE_ID;

        int instanceId = pair.first;
        mMultiInstanceManager.createInstance(instanceId, activity);
        mMultiInstanceManager.initialize(instanceId, activity.getTaskId());

        // Store minimal data to get the instance recognized.
        MultiInstanceManagerApi31.writeUrl(instanceId, "url" + instanceId);
        MultiInstanceManagerApi31.writeLastAccessedTime(index);
        SharedPreferencesManager.getInstance().writeInt(
                MultiInstanceManagerApi31.tabCountKey(index), 1);
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
