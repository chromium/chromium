// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.forTabModel;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link UndoClosureRestorer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UndoClosureRestorerUnitTest {
    private static final String TABLET_UNDO_USER_ACTION = "TabletTabStrip.UndoCloseTab";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Runnable mUndoRunnable;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;

    private Context mContext;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
    }

    @Test
    public void testRestoreSuccess_TabletTabStrip_RecordsUserActionAndRunsCallback() {
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(createClosedTab(10, "Tab 1")),
                        TabClosingSource.TABLET_TAB_STRIP,
                        mUndoRunnable);
        when(mTabModel.restoreClosedTabsForUndo(snapshot, /* reselect= */ true))
                .thenReturn(new int[] {101});

        assertTrue(
                UndoClosureRestorer.restoreClosure(
                        mTabModel, snapshot, /* reselect= */ true, mContext, mSnackbarManager));

        assertEquals(1, mUserActionTester.getActionCount(TABLET_UNDO_USER_ACTION));
        verify(mUndoRunnable).run();
        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testRestoreSuccess_NonTabletSource_DoesNotRecordTabletUserAction() {
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(createClosedTab(10, "Tab 1"), createClosedTab(20, "Tab 2")),
                        TabClosingSource.UNKNOWN,
                        /* undoRunnable= */ null);
        when(mTabModel.restoreClosedTabsForUndo(snapshot, /* reselect= */ false))
                .thenReturn(new int[] {Tab.INVALID_TAB_ID, 0});

        assertTrue(
                UndoClosureRestorer.restoreClosure(
                        mTabModel, snapshot, /* reselect= */ false, mContext, mSnackbarManager));

        assertEquals(0, mUserActionTester.getActionCount(TABLET_UNDO_USER_ACTION));
        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    public void testRestoreFailure_ShowsFailureNotificationSnackbar() {
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(createClosedTab(10, "Tab 1"), createClosedTab(20, "Tab 2")),
                        TabClosingSource.TABLET_TAB_STRIP,
                        mUndoRunnable);
        when(mTabModel.restoreClosedTabsForUndo(snapshot, /* reselect= */ true))
                .thenReturn(new int[] {Tab.INVALID_TAB_ID, Tab.INVALID_TAB_ID});

        assertFalse(
                UndoClosureRestorer.restoreClosure(
                        mTabModel, snapshot, /* reselect= */ true, mContext, mSnackbarManager));

        assertEquals(0, mUserActionTester.getActionCount(TABLET_UNDO_USER_ACTION));
        verify(mUndoRunnable, never()).run();
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());

        Snackbar snackbar = mSnackbarCaptor.getValue();
        String expectedMessage = mContext.getString(R.string.undo_tab_restore_failed_message);
        assertEquals(Snackbar.UMA_TAB_CLOSE_UNDO, snackbar.getIdentifierForTesting());
        assertEquals(expectedMessage, snackbar.getTextForTesting());
        assertNull(snackbar.getController());
        assertNull(snackbar.getActionText());
        assertEquals(
                Snackbar.make(
                        expectedMessage,
                        /* controller= */ null,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_TAB_CLOSE_UNDO),
                snackbar);
    }

    @Test
    public void testRestoreFailure_NullSnackbarManager_ReturnsFalseWithoutThrowing() {
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        List.of(createClosedTab(10, "Tab 1")),
                        TabClosingSource.UNKNOWN,
                        /* undoRunnable= */ null);
        when(mTabModel.restoreClosedTabsForUndo(snapshot, /* reselect= */ false))
                .thenReturn(new int[] {Tab.INVALID_TAB_ID});

        assertFalse(
                UndoClosureRestorer.restoreClosure(
                        mTabModel,
                        snapshot,
                        /* reselect= */ false,
                        mContext,
                        /* snackbarManager= */ null));
    }

    private static ClosedTab createClosedTab(int id, String title) {
        return new ClosedTab(
                id,
                title,
                /* groupId= */ null,
                /* modelIndex= */ TabList.INVALID_TAB_INDEX,
                /* pinned= */ false,
                /* wasSelected= */ false);
    }

    private static UndoClosureSnapshot createSnapshot(
            List<ClosedTab> tabs,
            @TabClosingSource int closingSource,
            @Nullable Runnable undoRunnable) {
        return forTabModel(
                tabs,
                /* groups= */ Collections.emptyMap(),
                /* isTabGroupSyncEnabled= */ false,
                /* isAllTabs= */ false,
                /* entryId= */ TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID,
                closingSource,
                undoRunnable);
    }
}
