// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui.undo_tab_close_snackbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosureKind;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link SavedTabGroupUndoBarController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SavedTabGroupUndoBarControllerUnitTest {
    private static final @TabId int TAB_ID = 10;
    private static final @TabId int SECOND_TAB_ID = 20;
    private static final @TabId int OTHER_TAB_ID = 999;
    private static final int MODEL_INDEX = 2;
    private static final @TabRestoreEntryId int ENTRY_ID = 42;
    private static final String SYNC_ID = "sync_id";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private SnackbarManageable mSnackbarManageable;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private Destroyable mLease;
    @Mock private Tab mTab;
    @Mock private Tab mSecondTab;

    private final List<Snackbar> mShownSnackbars = new ArrayList<>();

    private Context mContext;
    private SavedTabGroupUndoBarController mController;
    private int mAcquiredLeases;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();

        when(mTabModelSelector.getModel(/* incognito= */ false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModelForTabId(TAB_ID)).thenReturn(mTabModel);
        when(mSnackbarManageable.getSnackbarManager()).thenReturn(mSnackbarManager);
        when(mSnackbarManager.canShowSnackbar()).thenReturn(true);
        doAnswer(
                        inv -> {
                            mShownSnackbars.add(inv.getArgument(0));
                            return null;
                        })
                .when(mSnackbarManager)
                .showSnackbar(any());

        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getTitle()).thenReturn("Tab 1");

        mController =
                new SavedTabGroupUndoBarController(
                        mContext,
                        mTabModelSelector,
                        () -> {
                            mAcquiredLeases++;
                            return mLease;
                        },
                        mSnackbarManageable,
                        mTabGroupSyncService);
    }

    @After
    public void tearDown() {
        mController.destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_TabOnlyClose_QueuesPendingSnapshotDirectlyAndRestoresOnAction() {
        when(mTabModel.restoreClosedTabsForUndo(any(), eq(false), isNull()))
                .thenReturn(new int[] {101});
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_UNDO);

        UndoClosureSnapshot htmoSnapshot = createSnapshot(ENTRY_ID);
        mController.onUndoableClosure(htmoSnapshot);
        mController.triggerSnackbarForTab(mTab);

        assertEquals(1, mShownSnackbars.size());
        assertEquals(1, mAcquiredLeases);
        UndoBarContent barContent = (UndoBarContent) mShownSnackbars.get(0).getActionData();
        assertSame(htmoSnapshot, barContent.snapshot);
        assertEquals(MODEL_INDEX, barContent.snapshot.tabs.get(0).modelIndex);
        assertEquals(Snackbar.UMA_TAB_CLOSE_UNDO, barContent.umaType);
        verify(mTab, never()).getTitle();
        verify(mTab, never()).getIsPinned();

        mController.onAction(barContent);

        watcher.assertExpected();
        verify(mTabModel)
                .restoreClosedTabsForUndo(
                        eq(htmoSnapshot),
                        /* reselect= */ eq(false),
                        /* tabGroupUiActionHandler= */ isNull());
        verify(mTabModel, never()).cancelTabClosure(anyInt());
        verify(mTabGroupSyncService, never()).updateArchivalStatus(any(), anyBoolean());
        verify(mLease).destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_MultiTabCloseInDifferentOrder_QueuesPendingSnapshotDirectly() {
        when(mSecondTab.getId()).thenReturn(SECOND_TAB_ID);

        UndoClosureSnapshot htmoSnapshot =
                createSnapshot(
                        List.of(
                                createClosedTab(TAB_ID, "Tab 1", /* modelIndex= */ 1),
                                createClosedTab(SECOND_TAB_ID, "Tab 2", /* modelIndex= */ 4)),
                        ENTRY_ID);
        mController.onUndoableClosure(htmoSnapshot);
        mController.queueUndoBar(List.of(mSecondTab, mTab), Collections.emptyList());

        assertEquals(1, mShownSnackbars.size());
        UndoBarContent barContent = (UndoBarContent) mShownSnackbars.get(0).getActionData();
        assertSame(htmoSnapshot, barContent.snapshot);
        assertEquals(Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO, barContent.umaType);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_OnAction_RestoresClosedTabsAndSavedGroupAndReleasesLease() {
        when(mTabModel.restoreClosedTabsForUndo(any(), eq(false), isNull()))
                .thenReturn(new int[] {101});
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_SINGLE_TAB_GROUP_DELETE_UNDO);

        UndoClosureSnapshot htmoSnapshot = createSnapshot(ENTRY_ID);
        mController.onUndoableClosure(htmoSnapshot);
        mController.queueUndoBar(List.of(mTab), List.of(SYNC_ID));
        assertEquals(1, mShownSnackbars.size());
        UndoBarContent barContent = (UndoBarContent) mShownSnackbars.get(0).getActionData();
        assertEquals(ClosureKind.ARCHIVED, barContent.snapshot.kind);
        assertEquals(htmoSnapshot.tabs, barContent.snapshot.tabs);
        assertEquals(MODEL_INDEX, barContent.snapshot.tabs.get(0).modelIndex);
        assertEquals(List.of(SYNC_ID), barContent.snapshot.savedTabGroupSyncIds);
        assertEquals(ENTRY_ID, barContent.snapshot.entryId);
        assertEquals(Snackbar.UMA_SINGLE_TAB_GROUP_DELETE_UNDO, barContent.umaType);
        verify(mTab, never()).getTitle();
        verify(mTab, never()).getIsPinned();

        mController.onAction(barContent);

        watcher.assertExpected();
        verify(mTabModel)
                .restoreClosedTabsForUndo(
                        eq(barContent.snapshot),
                        /* reselect= */ eq(false),
                        /* tabGroupUiActionHandler= */ isNull());
        verify(mTabModel, never()).cancelTabClosure(anyInt());
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_ID, /* archivalStatus= */ true);
        verify(mLease).destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_OnDismissNoAction_DoesNotCommitAndReleasesLease() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Dismissed", Snackbar.UMA_SINGLE_TAB_GROUP_DELETE_UNDO);

        mController.onUndoableClosure(createSnapshot(ENTRY_ID));
        mController.queueUndoBar(List.of(mTab), List.of(SYNC_ID));
        assertEquals(1, mShownSnackbars.size());

        mController.onDismissNoAction(mShownSnackbars.get(0).getActionData());

        watcher.assertExpected();
        verify(mTabModel, never()).commitTabClosure(anyInt());
        verify(mLease).destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_InvalidEntryIdWithoutSavedGroupsReturnsEarly_SavedGroupStillQueues() {
        // 1. Tabs with no preceding onUndoableClosure and no saved groups -> returns early.
        mController.triggerSnackbarForTab(mTab);
        assertEquals(0, mShownSnackbars.size());
        assertEquals(0, mAcquiredLeases);

        // 2. Tabs with INVALID_TAB_RESTORE_ENTRY_ID and no saved groups -> returns early.
        mController.onUndoableClosure(
                createSnapshot(TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID));
        mController.triggerSnackbarForTab(mTab);
        assertEquals(0, mShownSnackbars.size());
        assertEquals(0, mAcquiredLeases);

        // 3. Tabs with mismatched pendingSnapshot and no saved groups -> returns early.
        mController.onUndoableClosure(createSnapshot(OTHER_TAB_ID, ENTRY_ID));
        mController.triggerSnackbarForTab(mTab);
        assertEquals(0, mShownSnackbars.size());
        assertEquals(0, mAcquiredLeases);

        // 4. SavedTabGroup closure with no tabs consumes any stale pendingSnapshot and queues
        // snackbar with invalid entryId.
        mController.onUndoableClosure(createSnapshot(ENTRY_ID));
        mController.triggerSnackbarForSavedTabGroup(SYNC_ID);
        assertEquals(1, mShownSnackbars.size());
        assertEquals(1, mAcquiredLeases);
        UndoBarContent barContent = (UndoBarContent) mShownSnackbars.get(0).getActionData();
        assertTrue(barContent.snapshot.tabs.isEmpty());
        assertEquals(TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID, barContent.snapshot.entryId);

        mController.onAction(barContent);
        verify(mTabModel, never()).restoreClosedTabsForUndo(any(), anyBoolean(), any());
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_ID, /* archivalStatus= */ true);
        verify(mLease).destroy();

        // Confirm the stale pendingSnapshot was cleared and does not leak to a later tab close.
        mController.triggerSnackbarForTab(mTab);
        assertEquals(1, mShownSnackbars.size());
        assertEquals(1, mAcquiredLeases);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_InvalidEntryIdWithTabsAndSavedGroups_DropsTabsAndRestoresSavedGroup() {
        // 1. Invalid entryId with tabs + saved group drops tabs and restores saved group.
        mController.onUndoableClosure(
                createSnapshot(TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID));
        mController.queueUndoBar(List.of(mTab), List.of(SYNC_ID));
        assertEquals(1, mShownSnackbars.size());
        assertEquals(1, mAcquiredLeases);
        UndoBarContent barContent = (UndoBarContent) mShownSnackbars.get(0).getActionData();
        assertTrue(barContent.snapshot.tabs.isEmpty());
        assertEquals(List.of(SYNC_ID), barContent.snapshot.savedTabGroupSyncIds);
        assertEquals(TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID, barContent.snapshot.entryId);

        mController.onAction(barContent);
        verify(mTabModel, never()).restoreClosedTabsForUndo(any(), anyBoolean(), any());
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_ID, /* archivalStatus= */ true);
        verify(mLease).destroy();

        // 2. Mismatched pendingSnapshot with tabs + saved group also drops tabs and restores only
        // saved group.
        mController.onUndoableClosure(createSnapshot(OTHER_TAB_ID, ENTRY_ID));
        mController.queueUndoBar(List.of(mTab), List.of(SYNC_ID));
        assertEquals(2, mShownSnackbars.size());
        assertEquals(2, mAcquiredLeases);
        UndoBarContent secondBarContent = (UndoBarContent) mShownSnackbars.get(1).getActionData();
        assertTrue(secondBarContent.snapshot.tabs.isEmpty());
        assertEquals(List.of(SYNC_ID), secondBarContent.snapshot.savedTabGroupSyncIds);
        assertEquals(
                TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID, secondBarContent.snapshot.entryId);

        mController.onAction(secondBarContent);
        verify(mTabModel, never()).restoreClosedTabsForUndo(any(), anyBoolean(), any());
        verify(mTabGroupSyncService, times(2))
                .updateArchivalStatus(SYNC_ID, /* archivalStatus= */ true);
        verify(mLease, times(2)).destroy();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOff_OnActionAndOnDismissNoAction_UsesLegacyPendingClosures() {
        mController.queueUndoBar(List.of(mTab), List.of(SYNC_ID));
        assertEquals(1, mShownSnackbars.size());
        UndoBarContent combinedBarContent = (UndoBarContent) mShownSnackbars.get(0).getActionData();
        assertEquals(ClosureKind.ARCHIVED, combinedBarContent.snapshot.kind);
        assertEquals(TabList.INVALID_TAB_INDEX, combinedBarContent.snapshot.tabs.get(0).modelIndex);

        mController.onAction(combinedBarContent);
        verify(mTabModel).cancelTabClosure(TAB_ID);
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_ID, /* archivalStatus= */ true);
        verify(mLease).destroy();

        mController.triggerSnackbarForTab(mTab);
        assertEquals(2, mShownSnackbars.size());
        UndoBarContent tabOnlyBarContent = (UndoBarContent) mShownSnackbars.get(1).getActionData();
        assertEquals(ClosureKind.ARCHIVED, tabOnlyBarContent.snapshot.kind);
        assertEquals(TabList.INVALID_TAB_INDEX, tabOnlyBarContent.snapshot.tabs.get(0).modelIndex);
        assertEquals(Snackbar.UMA_TAB_CLOSE_UNDO, tabOnlyBarContent.umaType);
        verify(mTabModelSelector, never()).getModel(anyBoolean());

        mController.onDismissNoAction(tabOnlyBarContent);
        verify(mTabModel).commitTabClosure(TAB_ID);
        verify(mLease, times(2)).destroy();
    }

    private static UndoClosureSnapshot createSnapshot(@TabRestoreEntryId int entryId) {
        return createSnapshot(TAB_ID, entryId);
    }

    private static UndoClosureSnapshot createSnapshot(
            @TabId int tabId, @TabRestoreEntryId int entryId) {
        return createSnapshot(List.of(createClosedTab(tabId, "Tab 1", MODEL_INDEX)), entryId);
    }

    private static UndoClosureSnapshot createSnapshot(
            List<ClosedTab> closedTabs, @TabRestoreEntryId int entryId) {
        return UndoClosureSnapshot.forTabModel(
                closedTabs,
                Collections.emptyMap(),
                /* isTabGroupSyncEnabled= */ false,
                /* isAllTabs= */ false,
                entryId,
                TabClosingSource.UNKNOWN,
                /* undoRunnable= */ null);
    }

    private static ClosedTab createClosedTab(@TabId int tabId, String title, int modelIndex) {
        return new ClosedTab(
                tabId,
                title,
                /* groupId= */ null,
                /* indexInGroup= */ TabList.INVALID_TAB_INDEX,
                modelIndex,
                /* pinned= */ false,
                /* wasSelected= */ false);
    }
}
