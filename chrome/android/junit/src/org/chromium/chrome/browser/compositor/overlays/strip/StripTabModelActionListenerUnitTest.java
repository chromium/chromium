// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.view.View;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.overlays.strip.StripTabModelActionListener.ActionType;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

/** Unit tests for {@link StripTabModelActionListener}. */
@RunWith(BaseRobolectricTestRunner.class)
public class StripTabModelActionListenerUnitTest {
    private static final Token TAB_GROUP_ID = new Token(3478329L, 3489L);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mToolbarContainerView;
    @Mock private Runnable mBeforeSyncDialogRunnable;
    @Mock private Runnable mOnSuccess;

    private final SettableNullableObservableSupplier<Token> mGroupIdToHideSupplier =
            ObservableSuppliers.createNullable();

    private StripTabModelActionListener createListener(@ActionType int actionType) {
        return new StripTabModelActionListener(
                TAB_GROUP_ID,
                actionType,
                mGroupIdToHideSupplier,
                mToolbarContainerView,
                mBeforeSyncDialogRunnable,
                mOnSuccess);
    }

    @Test
    public void testDialogTypeNone() {
        // ActionType isn't impactful here.
        StripTabModelActionListener listener = createListener(ActionType.REORDER);

        // DialogType.NONE always sends willSkipDialog = true.
        listener.willPerformActionOrShowDialog(DialogType.NONE, /* willSkipDialog= */ true);
        verify(mBeforeSyncDialogRunnable, never()).run();
        verify(mToolbarContainerView, never()).cancelDragAndDrop();
        assertNull(mGroupIdToHideSupplier.get());

        // DialogType.NONE will always send IMMEDIATE_CONTINUE.
        listener.onConfirmationDialogResult(
                DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        assertNull(mGroupIdToHideSupplier.get());
        verify(mOnSuccess).run();
    }

    @Test
    public void testCollaborationDialog() {
        // ActionType isn't impactful here.
        StripTabModelActionListener listener = createListener(ActionType.DRAG_OFF_STRIP);

        // DialogType.COLLABORATION is never skipped.
        listener.willPerformActionOrShowDialog(
                DialogType.COLLABORATION, /* willSkipDialog= */ false);
        verify(mBeforeSyncDialogRunnable, never()).run();
        verify(mToolbarContainerView, never()).cancelDragAndDrop();
        assertNull(mGroupIdToHideSupplier.get());

        // Negative confirmation (delete).
        listener.onConfirmationDialogResult(
                DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        assertNull(mGroupIdToHideSupplier.get());
        verify(mOnSuccess, never()).run();

        // Positive confirmation (keep).
        listener.onConfirmationDialogResult(
                DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        assertNull(mGroupIdToHideSupplier.get());
        verify(mOnSuccess).run();

        // DialogType.COLLABORATION is never IMMEDIATE_CONTINUE.
    }

    @Test
    public void testSyncDialog_Skip() {
        // ActionType isn't impactful here.
        StripTabModelActionListener listener = createListener(ActionType.CLOSE);

        listener.willPerformActionOrShowDialog(DialogType.SYNC, /* willSkipDialog= */ true);
        verify(mBeforeSyncDialogRunnable, never()).run();
        verify(mToolbarContainerView, never()).cancelDragAndDrop();
        assertNull(mGroupIdToHideSupplier.get());

        listener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        assertNull(mGroupIdToHideSupplier.get());
        verify(mOnSuccess).run();
    }

    @Test
    public void testSyncDialog() {
        // ActionType isn't impactful here.
        StripTabModelActionListener listener = createListener(ActionType.DRAG_OFF_STRIP);

        listener.willPerformActionOrShowDialog(DialogType.SYNC, /* willSkipDialog= */ false);
        verify(mBeforeSyncDialogRunnable).run();
        verify(mToolbarContainerView).cancelDragAndDrop();
        assertEquals(TAB_GROUP_ID, mGroupIdToHideSupplier.get());

        // Positive confirmation (delete).
        listener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        assertEquals(TAB_GROUP_ID, mGroupIdToHideSupplier.get());
        verify(mOnSuccess).run();

        // Negative confirmation (keep).
        listener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        assertNull(mGroupIdToHideSupplier.get());
        // Not run a second time or times(2) would be required.
        verify(mOnSuccess).run();

        // DialogType.SYNC with willSkipDialog = false is never IMMEDIATE_CONTINUE.
    }

    @Test(expected = AssertionError.class)
    public void testDragOffStrip_Sync_SkipDialog() {
        // ActionType.DRAG_OFF_STRIP has special handling for SYNC and should not be reachable if
        // IMMEDIATE_CONTINUE happens.
        StripTabModelActionListener listener = createListener(ActionType.DRAG_OFF_STRIP);

        listener.willPerformActionOrShowDialog(DialogType.SYNC, /* willSkipDialog= */ true);
        verify(mBeforeSyncDialogRunnable, never()).run();
        verify(mToolbarContainerView, never()).cancelDragAndDrop();
        assertNull(mGroupIdToHideSupplier.get());

        // This will assert.
        listener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.IMMEDIATE_CONTINUE);
    }
}
