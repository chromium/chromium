// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.multiwindow.MultiInstanceManager.INVALID_WINDOW_ID;

import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabGroupTask;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabsTask;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/** Unit tests for {@link TabReparentingDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 31)
public class TabReparentingDelegateUnitTest {
    private static final int SOURCE_WINDOW_ID = 1;
    private static final int DEST_WINDOW_ID = 2;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ChromeTabbedActivity mCurrentActivity;
    @Mock private ChromeTabbedActivity mDestActivity;
    @Mock private MismatchedIndicesHandler mMismatchedIndicesHandler;
    @Mock private TabModelOrchestrator mTabModelOrchestrator;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabPersistentStore mTabPersistentStore;
    @Mock private ReparentingTabsTask mReparentingTabsTask;
    @Mock private ReparentingTabGroupTask mReparentingTabGroupTask;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    @Captor private ArgumentCaptor<Runnable> mOnSaveTabListRunnableCaptor;

    private TabReparentingDelegate mDelegate;
    private final SettableMonotonicObservableSupplier<TabModelOrchestrator>
            mTabModelOrchestratorSupplier = ObservableSuppliers.createMonotonic();

    @Before
    public void setup() {
        MultiWindowTestUtils.enableMultiInstance();
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(any())).thenReturn(true);
        mTabModelOrchestratorSupplier.set(mTabModelOrchestrator);

        mDelegate = new TabReparentingDelegate(mCurrentActivity, mTabModelOrchestratorSupplier);

        when(mCurrentActivity.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());

        MultiWindowTestUtils.setupTabModelSelectorFactory(mock(Profile.class), mock(Profile.class));
        TabWindowManagerSingleton.getInstance()
                .requestSelector(
                        mCurrentActivity,
                        /* modalDialogManager= */ null,
                        /* profileProviderSupplier= */ null,
                        /* tabCreatorManager= */ null,
                        /* nextTabPolicySupplier= */ null,
                        /* multiInstanceManager= */ null,
                        mMismatchedIndicesHandler,
                        SOURCE_WINDOW_ID);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabModelOrchestrator.getTabPersistentStore()).thenReturn(mTabPersistentStore);

        ReparentingTabsTask.setReparentingTabsTaskForTesting(mReparentingTabsTask);
        doNothing().when(mReparentingTabsTask).begin(any(), any(), any(), any());
        ReparentingTabGroupTask.setReparentingTabGroupTaskForTesting(mReparentingTabGroupTask);
        doNothing().when(mReparentingTabGroupTask).begin(any(), any());
    }

    @After
    public void tearDown() {
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        ApplicationStatus.destroyForJUnitTests();
    }

    @Test
    public void testReparentTabsToNewWindow() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);
        boolean openAdjacently = true;

        // Act.
        mDelegate.reparentTabsToNewWindow(
                tabs,
                INVALID_WINDOW_ID,
                openAdjacently,
                /* finalizeCallback= */ null,
                NewWindowAppSource.MENU);

        // Verify that the reparenting task is initiated.
        var intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mReparentingTabsTask)
                .begin(eq(mCurrentActivity), intentCaptor.capture(), eq(null), eq(null));

        // Verify the intent used in the reparenting task.
        assertEquals(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT is not set as expected.",
                openAdjacently,
                (intentCaptor.getValue().getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT) != 0);
        assertTrue(
                "EXTRA_PREFER_NEW is not set as expected.",
                intentCaptor.getValue().getBooleanExtra(IntentHandler.EXTRA_PREFER_NEW, false));
        assertEquals(
                "EXTRA_WINDOW_ID is not set as expected.",
                INVALID_WINDOW_ID,
                intentCaptor
                        .getValue()
                        .getIntExtra(IntentHandler.EXTRA_WINDOW_ID, INVALID_WINDOW_ID));
        assertEquals(
                "New window source extra is incorrect.",
                NewWindowAppSource.MENU,
                intentCaptor
                        .getValue()
                        .getIntExtra(
                                IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE,
                                NewWindowAppSource.UNKNOWN));
    }

    @Test
    public void testReparentTabsToExistingWindow_validDestTabIndex() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);

        // Act.
        mDelegate.reparentTabsToExistingWindow(
                mDestActivity,
                tabs,
                /* destTabIndex= */ 2,
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX);

        // Verify that the reparenting task is initiated.
        var setupIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        var onNewIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mReparentingTabsTask).setupIntent(setupIntentCaptor.capture(), eq(null));
        verify(mDestActivity).onNewIntent(onNewIntentCaptor.capture());

        // Verify the intent used in the reparenting task.
        assertEquals(
                "Intents passed to setup reparenting task and sent to the activity should be the"
                        + " same.",
                setupIntentCaptor.getValue(),
                onNewIntentCaptor.getValue());
        assertTrue(
                "EXTRA_TAB_INDEX is not set.",
                setupIntentCaptor.getValue().hasExtra(IntentHandler.EXTRA_TAB_INDEX));
    }

    @Test
    public void testReparentTabsToExistingWindow_validDestGroupTabId() {
        // Setup.
        List<Tab> tabs = List.of(mTab1, mTab2);

        // Act.
        mDelegate.reparentTabsToExistingWindow(
                mDestActivity,
                tabs,
                /* destTabIndex= */ TabList.INVALID_TAB_INDEX,
                /* destGroupTabId= */ 3);

        // Verify that the reparenting task is initiated.
        var setupIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        var onNewIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mReparentingTabsTask).setupIntent(setupIntentCaptor.capture(), eq(null));
        verify(mDestActivity).onNewIntent(onNewIntentCaptor.capture());

        // Verify the intent used in the reparenting task.
        assertEquals(
                "Intents passed to setup reparenting task and sent to the activity should be the"
                        + " same.",
                setupIntentCaptor.getValue(),
                onNewIntentCaptor.getValue());
        assertTrue(
                "EXTRA_DEST_TAB_ID is not set.",
                setupIntentCaptor.getValue().hasExtra(IntentHandler.EXTRA_DEST_TAB_ID));
    }

    @Test
    public void testReparentTabsToExistingWindow_invalidDestTabIds() {
        assertThrows(
                AssertionError.class,
                () ->
                        mDelegate.reparentTabsToExistingWindow(
                                mDestActivity,
                                List.of(mTab1, mTab2),
                                /* destTabIndex= */ 2,
                                /* destGroupTabId= */ 3));
    }

    @Test
    public void testReparentTabGroupToNewWindow() {
        // Setup.
        TabGroupMetadata tabGroupMetadata = getTestTabGroupMetadata(/* isGroupShared= */ false);
        boolean openAdjacently = true;

        // Act.
        mDelegate.reparentTabGroupToNewWindow(
                tabGroupMetadata, DEST_WINDOW_ID, openAdjacently, NewWindowAppSource.MENU);

        // Verify that we pause the TabGroupSyncService to stop observing local changes.
        verify(mTabGroupSyncService).setLocalObservationMode(/* observeLocalChanges= */ false);

        // Verify that we pause the TabPersistentStore.
        verify(mTabPersistentStore).pauseSaveTabList();
        verify(mTabPersistentStore).resumeSaveTabList(mOnSaveTabListRunnableCaptor.capture());

        // Verify that we only send the reparent intent after the Runnable runs.
        var setupIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        var beginIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mReparentingTabGroupTask).setupIntent(setupIntentCaptor.capture(), eq(null));
        verify(mReparentingTabGroupTask, never()).begin(any(), any());
        mOnSaveTabListRunnableCaptor.getValue().run();
        verify(mReparentingTabGroupTask).begin(eq(mCurrentActivity), beginIntentCaptor.capture());

        // Verify the intent used in the reparenting task.
        assertEquals(
                "Intents passed to setup and begin reparenting task should be the same.",
                setupIntentCaptor.getValue(),
                beginIntentCaptor.getValue());
        assertEquals(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT is not set as expected.",
                openAdjacently,
                (setupIntentCaptor.getValue().getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT)
                        != 0);
        assertFalse(
                "EXTRA_PREFER_NEW is not set as expected.",
                setupIntentCaptor
                        .getValue()
                        .getBooleanExtra(IntentHandler.EXTRA_PREFER_NEW, false));
        assertEquals(
                "EXTRA_WINDOW_ID is not set as expected.",
                DEST_WINDOW_ID,
                setupIntentCaptor
                        .getValue()
                        .getIntExtra(
                                IntentHandler.EXTRA_WINDOW_ID, TabWindowManager.INVALID_WINDOW_ID));
        assertTrue(
                "EXTRA_REPARENT_START_TIME is not set.",
                setupIntentCaptor.getValue().hasExtra(IntentHandler.EXTRA_REPARENT_START_TIME));
        assertEquals(
                "New window source extra is incorrect.",
                NewWindowAppSource.MENU,
                setupIntentCaptor
                        .getValue()
                        .getIntExtra(
                                IntentHandler.EXTRA_NEW_WINDOW_APP_SOURCE,
                                NewWindowAppSource.UNKNOWN));

        // Verify that we resume the TabGroupSyncService to begin observing local changes.
        verify(mTabGroupSyncService).setLocalObservationMode(/* observeLocalChanges= */ true);
    }

    @Test
    public void testReparentTabGroupToExistingWindow() {
        doTestReparentTabGroupToExistingWindow(/* isGroupShared= */ false);
    }

    @Test
    public void testReparentTabGroupToExistingWindow_sharedTabGroup() {
        doTestReparentTabGroupToExistingWindow(/* isGroupShared= */ true);
    }

    private void doTestReparentTabGroupToExistingWindow(boolean isGroupShared) {
        // Setup.
        TabGroupMetadata tabGroupMetadata = getTestTabGroupMetadata(isGroupShared);

        // Act.
        mDelegate.reparentTabGroupToExistingWindow(
                mDestActivity, tabGroupMetadata, /* destTabIndex= */ 3);

        // Verify that we pause the TabGroupSyncService to stop observing local changes.
        verify(mTabGroupSyncService).setLocalObservationMode(/* observeLocalChanges= */ false);

        // Verify that we pause the TabPersistentStore.
        verify(mTabPersistentStore).pauseSaveTabList();
        verify(mTabPersistentStore).resumeSaveTabList(mOnSaveTabListRunnableCaptor.capture());

        // Verify that we only send the reparent intent after the Runnable runs.
        var setupIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        var onNewIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mReparentingTabGroupTask).setupIntent(setupIntentCaptor.capture(), eq(null));
        verify(mDestActivity, never()).onNewIntent(any());
        mOnSaveTabListRunnableCaptor.getValue().run();
        verify(mDestActivity).onNewIntent(onNewIntentCaptor.capture());

        assertEquals(
                "Intents passed to setup reparenting task and sent to the activity should be the"
                        + " same.",
                setupIntentCaptor.getValue(),
                onNewIntentCaptor.getValue());
        assertTrue(
                "FLAG_ACTIVITY_LAUNCH_ADJACENT is not set.",
                (setupIntentCaptor.getValue().getFlags() & Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT)
                        != 0);
        assertTrue(
                "EXTRA_TAB_INDEX is not set.",
                setupIntentCaptor.getValue().hasExtra(IntentHandler.EXTRA_TAB_INDEX));
        assertTrue(
                "EXTRA_REPARENT_START_TIME is not set.",
                setupIntentCaptor.getValue().hasExtra(IntentHandler.EXTRA_REPARENT_START_TIME));

        // Verify that we resume the TabGroupSyncService to begin observing local changes.
        verify(mTabGroupSyncService).setLocalObservationMode(/* observeLocalChanges= */ true);
    }

    private static TabGroupMetadata getTestTabGroupMetadata(boolean isGroupShared) {
        ArrayList<Entry<Integer, String>> tabIdsToUrls =
                new ArrayList<>(
                        List.of(
                                Map.entry(1, "https://www.amazon.com"),
                                Map.entry(2, "https://www.youtube.com"),
                                Map.entry(3, "https://www.facebook.com")));
        return new TabGroupMetadata(
                /* selectedTabId= */ -1,
                SOURCE_WINDOW_ID,
                /* tabGroupId= */ null,
                tabIdsToUrls,
                /* tabGroupColor= */ 0,
                /* tabGroupTitle= */ null,
                /* mhtmlTabTitle= */ null,
                /* tabGroupCollapsed= */ false,
                isGroupShared,
                /* isIncognito= */ false);
    }
}
