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
import android.os.Bundle;

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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabGroupTask;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTabsTask;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.WebContents;

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
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabPersistentStore mTabPersistentStore;
    @Mock private ReparentingTabsTask mReparentingTabsTask;
    @Mock private ReparentingTabGroupTask mReparentingTabGroupTask;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Profile mProfile;
    @Mock private WebContents mWebContents;

    @Captor private ArgumentCaptor<Runnable> mOnSaveTabListRunnableCaptor;

    private TabReparentingDelegate mDelegate;

    @Before
    public void setup() {
        MultiWindowTestUtils.enableMultiInstance();
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(any())).thenReturn(true);
        TabBuilder.setTabForTesting(mTab1);

        mDelegate = new TabReparentingDelegate();
        TabReparentingDelegate.setPersistentStoreForTesting(mTabPersistentStore);

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
                        mMismatchedIndicesHandler,
                        SOURCE_WINDOW_ID);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);

        ReparentingTabsTask.setReparentingTabsTaskForTesting(mReparentingTabsTask);
        when(mReparentingTabsTask.begin(any(), any(), any(), any())).thenReturn(true);
        ReparentingTabGroupTask.setReparentingTabGroupTaskForTesting(mReparentingTabGroupTask);
        doNothing().when(mReparentingTabGroupTask).begin(any(), any());

        MultiWindowUtils.setActivityByWindowIdForTesting(SOURCE_WINDOW_ID, mCurrentActivity);
        when(mTab1.getContext()).thenReturn(mCurrentActivity);
        when(mTab2.getContext()).thenReturn(mCurrentActivity);
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
    public void testCreateNewWindowFromWebContents_success() {
        // Act.
        boolean result =
                mDelegate.createNewWindowFromWebContents(
                        mCurrentActivity,
                        mProfile,
                        mWebContents,
                        /* additionalIntentExtras= */ null,
                        /* startActivityOptions= */ null,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        // Verify.
        assertTrue(result);
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mReparentingTabsTask)
                .begin(eq(mCurrentActivity), intentCaptor.capture(), eq(null), eq(null));
        Intent intent = intentCaptor.getValue();
        org.junit.Assert.assertNotNull(intent);
        org.junit.Assert.assertFalse(
                intent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, /* defaultValue= */ true));
    }

    @Test
    public void testCreateNewWindowFromWebContents_success_withExtrasAndOptions() {
        // Setup.
        when(mProfile.isIncognitoBranded()).thenReturn(true);

        Bundle extras = new Bundle();
        extras.putInt("extra", 1);
        Bundle options = mock(Bundle.class);

        // Act.
        boolean result =
                mDelegate.createNewWindowFromWebContents(
                        mCurrentActivity,
                        mProfile,
                        mWebContents,
                        extras,
                        options,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        // Verify.
        assertTrue(result);
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mReparentingTabsTask)
                .begin(eq(mCurrentActivity), intentCaptor.capture(), eq(options), eq(null));
        Intent intent = intentCaptor.getValue();
        org.junit.Assert.assertNotNull(intent);
        org.junit.Assert.assertTrue(
                intent.getBooleanExtra(
                        IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, /* defaultValue= */ false));
        org.junit.Assert.assertEquals(1, intent.getIntExtra("extra", 0));
    }

    @Test
    public void testCreateNewWindowFromWebContents_reparentingTaskFailed() {
        // Setup.
        when(mReparentingTabsTask.begin(any(), any(), any(), any())).thenReturn(false);

        // Act.
        boolean result =
                mDelegate.createNewWindowFromWebContents(
                        mCurrentActivity,
                        mProfile,
                        mWebContents,
                        /* additionalIntentExtras= */ null,
                        /* startActivityOptions= */ null,
                        NewWindowAppSource.BROWSER_WINDOW_CREATOR);

        // Verify.
        assertFalse(result);
        verify(mReparentingTabsTask).begin(eq(mCurrentActivity), any(), eq(null), eq(null));
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
                /* destGroupTabId= */ TabList.INVALID_TAB_INDEX,
                /* bringToFront= */ true);

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
                /* destGroupTabId= */ 3,
                /* bringToFront= */ true);

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
        List<Tab> tabs = List.of(mTab1, mTab2);
        assertThrows(
                AssertionError.class,
                () ->
                        mDelegate.reparentTabsToExistingWindow(
                                mDestActivity,
                                tabs,
                                /* destTabIndex= */ 2,
                                /* destGroupTabId= */ 3,
                                /* bringToFront= */ true));
    }

    @Test
    public void testReparentTabGroupToNewWindow() {
        doTestReparentTabGroupToNewWindow(/* pauseResumeTabGroupSyncService= */ true);
    }

    @Test
    public void testReparentTabGroupToNewWindow_noSourceTabPersistentStore() {
        doTestReparentTabGroupToNewWindow(/* pauseResumeTabGroupSyncService= */ false);
    }

    @Test
    public void testReparentTabGroupToExistingWindow() {
        doTestReparentTabGroupToExistingWindow(
                /* isGroupShared= */ false, /* pauseResumeTabGroupSyncService= */ true);
    }

    @Test
    public void testReparentTabGroupToExistingWindow_sharedTabGroup() {
        doTestReparentTabGroupToExistingWindow(
                /* isGroupShared= */ true, /* pauseResumeTabGroupSyncService= */ true);
    }

    @Test
    public void testReparentTabGroupToExistingWindow_noSourceTabPersistentStore() {
        doTestReparentTabGroupToExistingWindow(
                /* isGroupShared= */ false, /* pauseResumeTabGroupSyncService= */ false);
    }

    private void doTestReparentTabGroupToNewWindow(boolean pauseResumeTabGroupSyncService) {
        // Setup.
        TabGroupMetadata tabGroupMetadata = getTestTabGroupMetadata(/* isGroupShared= */ false);
        boolean openAdjacently = true;
        if (!pauseResumeTabGroupSyncService) {
            MultiWindowUtils.setActivityByWindowIdForTesting(
                    SOURCE_WINDOW_ID, /* activity= */ null);
        }

        // Act.
        mDelegate.reparentTabGroupToNewWindow(
                tabGroupMetadata, DEST_WINDOW_ID, openAdjacently, NewWindowAppSource.MENU);

        if (pauseResumeTabGroupSyncService) {
            // Verify that we pause the TabGroupSyncService to stop observing local changes.
            verify(mTabGroupSyncService).setLocalObservationMode(/* observeLocalChanges= */ false);

            // Verify that we pause the TabPersistentStore.
            verify(mTabPersistentStore).pauseSaveTabList();
            verify(mTabPersistentStore).resumeSaveTabList(mOnSaveTabListRunnableCaptor.capture());
        }

        var setupIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        var beginIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mReparentingTabGroupTask).setupIntent(setupIntentCaptor.capture(), eq(null));
        if (pauseResumeTabGroupSyncService) {
            // Verify that we only send the reparent intent after the Runnable runs.
            verify(mReparentingTabGroupTask, never()).begin(any(), any());
            mOnSaveTabListRunnableCaptor.getValue().run();
            verify(mReparentingTabGroupTask)
                    .begin(eq(mCurrentActivity), beginIntentCaptor.capture());
        } else {
            verify(mReparentingTabGroupTask).begin(any(), beginIntentCaptor.capture());
        }

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

        if (pauseResumeTabGroupSyncService) {
            // Verify that we resume the TabGroupSyncService to begin observing local changes.
            verify(mTabGroupSyncService).setLocalObservationMode(/* observeLocalChanges= */ true);
        }
    }

    private void doTestReparentTabGroupToExistingWindow(
            boolean isGroupShared, boolean pauseResumeTabGroupSyncService) {
        // Setup.
        TabGroupMetadata tabGroupMetadata = getTestTabGroupMetadata(isGroupShared);
        if (!pauseResumeTabGroupSyncService) {
            MultiWindowUtils.setActivityByWindowIdForTesting(
                    SOURCE_WINDOW_ID, /* activity= */ null);
        }

        // Act.
        mDelegate.reparentTabGroupToExistingWindow(
                mDestActivity, tabGroupMetadata, /* destTabIndex= */ 3, /* bringToFront= */ true);

        if (pauseResumeTabGroupSyncService) {
            // Verify that we pause the TabGroupSyncService to stop observing local changes.
            verify(mTabGroupSyncService).setLocalObservationMode(/* observeLocalChanges= */ false);

            // Verify that we pause the TabPersistentStore.
            verify(mTabPersistentStore).pauseSaveTabList();
            verify(mTabPersistentStore).resumeSaveTabList(mOnSaveTabListRunnableCaptor.capture());
        }

        var setupIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        var onNewIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mReparentingTabGroupTask).setupIntent(setupIntentCaptor.capture(), eq(null));
        if (pauseResumeTabGroupSyncService) {
            // Verify that we only send the reparent intent after the Runnable runs.
            verify(mDestActivity, never()).onNewIntent(any());
            mOnSaveTabListRunnableCaptor.getValue().run();
        }
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

        if (pauseResumeTabGroupSyncService) {
            // Verify that we resume the TabGroupSyncService to begin observing local changes.
            verify(mTabGroupSyncService).setLocalObservationMode(/* observeLocalChanges= */ true);
        }
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
