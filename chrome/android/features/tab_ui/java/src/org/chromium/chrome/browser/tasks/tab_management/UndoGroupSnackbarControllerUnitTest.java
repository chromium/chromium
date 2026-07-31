// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

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

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.UndoGroupMetadata;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.util.TokenHolder;

import java.util.List;

/** Unit tests for {@link UndoGroupSnackbarController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
@NullMarked
public class UndoGroupSnackbarControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private TabModel mTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private UndoGroupMetadata mUndoGroupMetadata;
    @Mock private UndoGroupMetadata mSecondUndoGroupMetadata;
    @Mock private Tab mTab;

    @Captor private ArgumentCaptor<TabGroupObserver> mTabGroupObserverCaptor;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<Snackbar> mSnackbarCaptor;

    private final SettableMonotonicObservableSupplier<TabModel> mCurrentTabModelSupplier =
            ObservableSuppliers.createMonotonic();

    private Context mContext;
    private UndoGroupSnackbarController mController;
    private TabGroupObserver mTabGroupObserver;
    private TabModelObserver mTabModelObserver;
    private Token mTabGroupId;

    @Before
    public void setUp() {
        mContext = spy(ApplicationProvider.getApplicationContext());
        mTabGroupId = new Token(1L, 2L);

        when(mTabModelSelector.getModel(/* incognito= */ false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(/* incognito= */ true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mCurrentTabModelSupplier);
        when(mTabModelSelector.getModels()).thenReturn(List.of(mTabModel, mIncognitoTabModel));
        when(mUndoGroupMetadata.isIncognito()).thenReturn(false);
        when(mUndoGroupMetadata.getTabGroupId()).thenReturn(mTabGroupId);
        when(mSecondUndoGroupMetadata.isIncognito()).thenReturn(false);
        when(mSecondUndoGroupMetadata.getTabGroupId()).thenReturn(mTabGroupId);
        when(mTabModel.getTabCountForGroup(mTabGroupId)).thenReturn(2);
        mCurrentTabModelSupplier.set(mTabModel);

        mController =
                new UndoGroupSnackbarController(mContext, mTabModelSelector, mSnackbarManager);

        verify(mTabModel).addTabGroupObserver(mTabGroupObserverCaptor.capture());
        mTabGroupObserver = mTabGroupObserverCaptor.getValue();

        verify(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        mTabModelObserver = mTabModelObserverCaptor.getValue();
    }

    @Test
    @SmallTest
    public void testShowUndoGroupSnackbar_SingleTabGroup_ShowsSingleTabTemplateText() {
        when(mTabModel.getTabCountForGroup(mTabGroupId)).thenReturn(1);

        mTabGroupObserver.showUndoGroupSnackbar(mUndoGroupMetadata);

        verify(mContext).getString(R.string.undo_bar_group_tab_message);
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    @SmallTest
    public void testShowUndoGroupSnackbar_MultipleTabsGroup_ShowsMultipleTabsTemplateText() {
        when(mTabModel.getTabCountForGroup(mTabGroupId)).thenReturn(3);

        mTabGroupObserver.showUndoGroupSnackbar(mUndoGroupMetadata);

        verify(mContext).getString(R.string.undo_bar_group_tabs_message);
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    @SmallTest
    public void testShowUndoGroupSnackbar_WhenNotThrottled_ShowsImmediately() {
        mTabGroupObserver.showUndoGroupSnackbar(mUndoGroupMetadata);

        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    @SmallTest
    public void testShowUndoGroupSnackbar_WhenThrottled_DelaysUntilStopThrottling() {
        int token = mController.startThrottling();
        assertNotEquals(TokenHolder.INVALID_TOKEN, token);

        mTabGroupObserver.showUndoGroupSnackbar(mUndoGroupMetadata);

        // Verify snackbar is NOT shown while throttled.
        verify(mSnackbarManager, never()).showSnackbar(any());

        // Stop throttling and verify snackbar is shown.
        mController.stopThrottling(token);
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    @SmallTest
    public void testShowUndoGroupSnackbar_WhenThrottledMultipleTimes_ShowsOnlyLastSnackbar() {
        int token = mController.startThrottling();

        mTabGroupObserver.showUndoGroupSnackbar(mUndoGroupMetadata);
        mTabGroupObserver.showUndoGroupSnackbar(mSecondUndoGroupMetadata);

        // Replacing pending metadata while throttled must expire the previous operation.
        verify(mTabModel).undoGroupOperationExpired(mUndoGroupMetadata);
        verify(mSnackbarManager, never()).showSnackbar(any());

        mController.stopThrottling(token);
        verify(mSnackbarManager).showSnackbar(mSnackbarCaptor.capture());
        assertEquals(mSecondUndoGroupMetadata, mSnackbarCaptor.getValue().getActionData());
    }

    @Test
    @SmallTest
    public void testShowUndoGroupSnackbar_WhenThrottledAndMovedOutOfGroup_CancelsSnackbar() {
        int token = mController.startThrottling();

        mTabGroupObserver.showUndoGroupSnackbar(mUndoGroupMetadata);
        verify(mSnackbarManager, never()).showSnackbar(any());

        // Drag out of group triggers willMoveTabOutOfGroup.
        mTabGroupObserver.willMoveTabOutOfGroup(mTab, null);

        // Dismissing snackbars while throttled must expire the pending operation.
        verify(mTabModel).undoGroupOperationExpired(mUndoGroupMetadata);

        // Stop throttling; snackbar should NOT be shown because pending metadata was cleared.
        mController.stopThrottling(token);
        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @SmallTest
    public void testOnAction_CallsPerformUndoGroupOperation() {
        mController.onAction(mUndoGroupMetadata);

        verify(mTabModel).performUndoGroupOperation(mUndoGroupMetadata);
    }

    @Test
    @SmallTest
    public void testOnDismissNoAction_CallsUndoGroupOperationExpired() {
        mController.onDismissNoAction(mUndoGroupMetadata);

        verify(mTabModel).undoGroupOperationExpired(mUndoGroupMetadata);
    }

    @Test
    @SmallTest
    public void testTabModelSelectorTabModelObserver_DidAddTab_DismissesSnackbar() {
        mTabModelObserver.didAddTab(
                mTab,
                TabLaunchType.FROM_LINK,
                TabCreationState.LIVE_IN_FOREGROUND,
                /* markedForSelection= */ true);

        verify(mSnackbarManager).dismissSnackbars(mController);
    }

    @Test
    @SmallTest
    public void testTabModelSelectorTabModelObserver_WillCloseTab_DismissesSnackbar() {
        mTabModelObserver.willCloseTab(mTab, /* didCloseAlone= */ true);

        verify(mSnackbarManager).dismissSnackbars(mController);
    }

    @Test
    @SmallTest
    public void testTabModelSelectorTabModelObserver_OnFinishingTabClosure_DismissesSnackbar() {
        mTabModelObserver.onFinishingTabClosure(mTab, TabClosingSource.UNKNOWN);

        verify(mSnackbarManager).dismissSnackbars(mController);
    }

    @Test
    @SmallTest
    public void testShowUndoGroupSnackbar_Incognito_UsesIncognitoTabModel() {
        when(mUndoGroupMetadata.isIncognito()).thenReturn(true);
        when(mIncognitoTabModel.getTabCountForGroup(mTabGroupId)).thenReturn(2);

        mTabGroupObserver.showUndoGroupSnackbar(mUndoGroupMetadata);

        verify(mIncognitoTabModel).getTabCountForGroup(mTabGroupId);
        verify(mSnackbarManager).showSnackbar(any());
    }

    @Test
    @SmallTest
    public void testWillMoveTabOutOfGroup_DismissesSnackbar() {
        mTabGroupObserver.willMoveTabOutOfGroup(mTab, null);

        verify(mSnackbarManager).dismissSnackbars(mController);
    }

    @Test
    @SmallTest
    public void testCurrentTabModelObserver_OnTabModelChanged_DismissesSnackbar() {
        mCurrentTabModelSupplier.set(mIncognitoTabModel);

        verify(mSnackbarManager).dismissSnackbars(mController);
    }

    @Test
    @SmallTest
    public void testDestroy_RemovesObserversAndCleanUp() {
        mController.destroy();

        assertFalse(mCurrentTabModelSupplier.hasObservers());
        verify(mTabModel).removeTabGroupObserver(mTabGroupObserver);
        verify(mIncognitoTabModel).removeTabGroupObserver(mTabGroupObserver);
        verify(mTabModel).removeObserver(mTabModelObserver);
    }

    @Test
    @SmallTest
    public void testDestroy_WhenThrottled_DiscardsPendingMetadata() {
        int token = mController.startThrottling();
        mTabGroupObserver.showUndoGroupSnackbar(mUndoGroupMetadata);

        mController.destroy();
        mController.stopThrottling(token);

        // Destroying the controller while a snackbar is pending must expire the operation.
        verify(mTabModel).undoGroupOperationExpired(mUndoGroupMetadata);
        verify(mSnackbarManager, never()).showSnackbar(any());
    }

    @Test
    @SmallTest
    public void testDismissSnackbars_WhenThrottled_ExpiresPendingMetadata() {
        mController.startThrottling();
        mTabGroupObserver.showUndoGroupSnackbar(mUndoGroupMetadata);

        // Any action calling dismissSnackbars (e.g. didAddTab) should expire pending metadata.
        mTabModelObserver.didAddTab(
                mTab,
                TabLaunchType.FROM_LINK,
                TabCreationState.LIVE_IN_FOREGROUND,
                /* markedForSelection= */ true);

        verify(mTabModel).undoGroupOperationExpired(mUndoGroupMetadata);
    }
}
