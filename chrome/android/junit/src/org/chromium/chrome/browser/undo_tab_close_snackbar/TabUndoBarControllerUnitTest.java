// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
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
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link TabUndoBarController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabUndoBarControllerUnitTest {
    private static final int TAB_ID = 10;
    private static final int OTHER_TAB_ID = 20;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private SnackbarManageable mSnackbarManageable;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Tab mTab;
    @Mock private Tab mOtherTab;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<UndoClosureSnapshot> mSnapshotCaptor;
    @Captor private ArgumentCaptor<Boolean> mReselectCaptor;

    private final List<Snackbar> mShownSnackbars = new ArrayList<>();

    private Context mContext;
    private UserActionTester mUserActionTester;
    private TabUndoBarController mController;
    private TabModelObserver mObserver;
    private boolean mIsDialogVisible;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mUserActionTester = new UserActionTester();

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
        when(mOtherTab.getId()).thenReturn(OTHER_TAB_ID);
        when(mOtherTab.getTitle()).thenReturn("Tab 2");

        mController =
                new TabUndoBarController(
                        mContext, mTabModelSelector, mSnackbarManageable, () -> mIsDialogVisible);
        mController.initialize();
        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        mObserver = mTabModelObserverCaptor.getValue();
    }

    @After
    public void tearDown() {
        mController.destroy();
        mUserActionTester.tearDown();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOff_OnAction_CallsCancelTabClosure() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_UNDO);

        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        assertEquals(1, mShownSnackbars.size());

        mController.onAction(mShownSnackbars.get(0).getActionData());

        watcher.assertExpected();
        verify(mTabModel).cancelTabClosure(TAB_ID);
        verify(mTabModel, never()).restoreClosedTabsForUndo(any(), anyBoolean());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOff_OnDismissNoAction_CallsCommitTabClosure() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Dismissed", Snackbar.UMA_TAB_CLOSE_UNDO);

        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        assertEquals(1, mShownSnackbars.size());

        mController.onDismissNoAction(mShownSnackbars.get(0).getActionData());

        watcher.assertExpected();
        verify(mTabModel).commitTabClosure(TAB_ID);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_OnAction_RestoresWithReselectTrueByDefault() {
        when(mTabModel.restoreClosedTabsForUndo(any(), eq(true))).thenReturn(new int[] {101});
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_UNDO);

        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.TABLET_TAB_STRIP);
        mObserver.didSelectTab(mOtherTab, TabSelectionType.FROM_CLOSE, /* lastId= */ TAB_ID);
        assertEquals(1, mShownSnackbars.size());

        mController.onAction(mShownSnackbars.get(0).getActionData());

        watcher.assertExpected();
        verify(mTabModel).restoreClosedTabsForUndo(mSnapshotCaptor.capture(), eq(true));
        assertEquals(TabClosingSource.TABLET_TAB_STRIP, mSnapshotCaptor.getValue().closingSource);
        assertEquals(1, mUserActionTester.getActionCount("TabletTabStrip.UndoCloseTab"));
        verify(mTabModel, never()).cancelTabClosure(anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_ManualTabSelectDisarmsReselect_AndNextCloseResets() {
        when(mTabModel.restoreClosedTabsForUndo(any(), anyBoolean())).thenReturn(new int[] {101});

        // 1. User manually selects a tab after close -> reselect is false.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        mObserver.didSelectTab(mOtherTab, TabSelectionType.FROM_USER, /* lastId= */ TAB_ID);
        assertEquals(1, mShownSnackbars.size());
        mController.onAction(mShownSnackbars.get(0).getActionData());

        // 2. Subsequent close resets the flag -> reselect is true.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        assertEquals(2, mShownSnackbars.size());
        mController.onAction(mShownSnackbars.get(1).getActionData());

        // 3. Omnibox selection after close -> reselect is false.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        mObserver.didSelectTab(mOtherTab, TabSelectionType.FROM_OMNIBOX, /* lastId= */ TAB_ID);
        assertEquals(3, mShownSnackbars.size());
        mController.onAction(mShownSnackbars.get(2).getActionData());

        // 4. Opening a new tab after close -> reselect is false.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        mObserver.didSelectTab(mOtherTab, TabSelectionType.FROM_NEW, /* lastId= */ TAB_ID);
        assertEquals(4, mShownSnackbars.size());
        mController.onAction(mShownSnackbars.get(3).getActionData());

        // 5. External undo selection after close -> reselect is false.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        mObserver.didSelectTab(mOtherTab, TabSelectionType.FROM_UNDO, /* lastId= */ TAB_ID);
        assertEquals(5, mShownSnackbars.size());
        mController.onAction(mShownSnackbars.get(4).getActionData());

        verify(mTabModel, times(5)).restoreClosedTabsForUndo(any(), mReselectCaptor.capture());
        assertEquals(List.of(false, true, false, false, false), mReselectCaptor.getAllValues());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_StackedClosures_OnlyMostRecentUndoReselects() {
        when(mTabModel.restoreClosedTabsForUndo(any(), anyBoolean())).thenReturn(new int[] {101});

        // 1. Two stacked closures undone in LIFO order -> only the newer closure reselects.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        mObserver.onTabClosePending(
                List.of(mOtherTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        assertEquals(2, mShownSnackbars.size());
        Snackbar bar1 = mShownSnackbars.get(0);
        Snackbar bar2 = mShownSnackbars.get(1);

        mController.onAction(bar2.getActionData());
        mController.onAction(bar1.getActionData());

        // 2. Newer closure dismissed via onDismissNoAction -> older closure does not reselect.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        mObserver.onTabClosePending(
                List.of(mOtherTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        assertEquals(4, mShownSnackbars.size());
        Snackbar bar3 = mShownSnackbars.get(2);
        Snackbar bar4 = mShownSnackbars.get(3);

        mController.onDismissNoAction(bar4.getActionData());
        mController.onAction(bar3.getActionData());

        // 3. Older closure dismissed while newer is active -> newer closure still reselects.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        mObserver.onTabClosePending(
                List.of(mOtherTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        assertEquals(6, mShownSnackbars.size());
        Snackbar bar5 = mShownSnackbars.get(4);
        Snackbar bar6 = mShownSnackbars.get(5);

        mController.onDismissNoAction(bar5.getActionData());
        mController.onAction(bar6.getActionData());

        // 4. Closure suppressed while TabGridDialog is visible -> disarms older closure.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        mIsDialogVisible = true;
        mObserver.onTabClosePending(
                List.of(mOtherTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        mIsDialogVisible = false;
        assertEquals(7, mShownSnackbars.size());
        Snackbar bar7 = mShownSnackbars.get(6);

        mController.onAction(bar7.getActionData());

        verify(mTabModel, times(5))
                .restoreClosedTabsForUndo(mSnapshotCaptor.capture(), mReselectCaptor.capture());
        assertEquals(List.of(true, false, false, true, false), mReselectCaptor.getAllValues());
        assertEquals(OTHER_TAB_ID, mSnapshotCaptor.getAllValues().get(0).tabs.get(0).id);
        assertEquals(TAB_ID, mSnapshotCaptor.getAllValues().get(1).tabs.get(0).id);
        assertEquals(TAB_ID, mSnapshotCaptor.getAllValues().get(2).tabs.get(0).id);
        assertEquals(OTHER_TAB_ID, mSnapshotCaptor.getAllValues().get(3).tabs.get(0).id);
        assertEquals(TAB_ID, mSnapshotCaptor.getAllValues().get(4).tabs.get(0).id);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_ManualSelectThenSecondCloseDismissed_OlderClosureDoesNotReselect() {
        when(mTabModel.restoreClosedTabsForUndo(any(), anyBoolean())).thenReturn(new int[] {101});

        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        mObserver.didSelectTab(mOtherTab, TabSelectionType.FROM_USER, /* lastId= */ TAB_ID);
        mObserver.onTabClosePending(
                List.of(mOtherTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);

        assertEquals(2, mShownSnackbars.size());
        Snackbar bar1 = mShownSnackbars.get(0);
        Snackbar bar2 = mShownSnackbars.get(1);

        mController.onDismissNoAction(bar2.getActionData());
        mController.onAction(bar1.getActionData());

        verify(mTabModel).restoreClosedTabsForUndo(mSnapshotCaptor.capture(), eq(false));
        assertEquals(TAB_ID, mSnapshotCaptor.getValue().tabs.get(0).id);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_ModelDeactivatedOrIncognitoSelected_DisarmsReselect() {
        when(mTabModel.restoreClosedTabsForUndo(any(), anyBoolean())).thenReturn(new int[] {101});

        // 1. Model deactivated after close -> reselect is false.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        mObserver.onDidActiveStateChange(mTabModel, /* active= */ false);
        assertEquals(1, mShownSnackbars.size());
        mController.onAction(mShownSnackbars.get(0).getActionData());

        // 2. Incognito selected at undo time -> reselect is false.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        assertEquals(2, mShownSnackbars.size());
        mController.onAction(mShownSnackbars.get(1).getActionData());

        // 3. Incognito selected at close time, then regular selected at undo -> reselect is false.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        assertEquals(3, mShownSnackbars.size());
        mController.onAction(mShownSnackbars.get(2).getActionData());

        // 4. Regular model active and non-incognito selected -> reselect remains true.
        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        mObserver.onDidActiveStateChange(mTabModel, /* active= */ true);
        assertEquals(4, mShownSnackbars.size());
        mController.onAction(mShownSnackbars.get(3).getActionData());

        verify(mTabModel, times(4)).restoreClosedTabsForUndo(any(), mReselectCaptor.capture());
        assertEquals(List.of(false, false, false, true), mReselectCaptor.getAllValues());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_OnDismissNoAction_DoesNotCommitAndRecordsDismissed() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Dismissed", Snackbar.UMA_TAB_CLOSE_UNDO);

        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        assertEquals(1, mShownSnackbars.size());

        mController.onDismissNoAction(mShownSnackbars.get(0).getActionData());

        watcher.assertExpected();
        verify(mTabModel, never()).commitTabClosure(anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_OnAction_RestoreFailureShowsFailureSnackbar() {
        when(mTabModel.restoreClosedTabsForUndo(any(), eq(true)))
                .thenReturn(new int[] {Tab.INVALID_TAB_ID});
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tab.CloseUndoBar.Undone", Snackbar.UMA_TAB_CLOSE_UNDO);

        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        assertEquals(1, mShownSnackbars.size());

        mController.onAction(mShownSnackbars.get(0).getActionData());

        watcher.assertExpected();
        assertEquals(2, mShownSnackbars.size());
        Snackbar failureSnackbar = mShownSnackbars.get(1);
        assertEquals(
                mContext.getString(R.string.undo_tab_restore_failed_message),
                failureSnackbar.getTextForTesting());
        assertEquals(Snackbar.UMA_TAB_CLOSE_UNDO, failureSnackbar.getIdentifierForTesting());
        assertNull(failureSnackbar.getActionText());
        assertNull(failureSnackbar.getController());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.UNDOABLE_TAB_CLOSURE_REWORK)
    public void testFlagOn_AllTabsClosureCommitted_DismissesSnackbarsAndDisarmsReselect() {
        when(mTabModel.restoreClosedTabsForUndo(any(), anyBoolean())).thenReturn(new int[] {101});

        mObserver.onTabClosePending(
                List.of(mTab), /* isAllTabs= */ false, TabClosingSource.UNKNOWN);
        assertEquals(1, mShownSnackbars.size());

        mObserver.allTabsClosureCommitted(/* isIncognito= */ false);
        verify(mSnackbarManager).dismissSnackbars(mController);

        mController.onAction(mShownSnackbars.get(0).getActionData());
        verify(mTabModel).restoreClosedTabsForUndo(mSnapshotCaptor.capture(), eq(false));
        assertEquals(TAB_ID, mSnapshotCaptor.getValue().tabs.get(0).id);
    }
}
