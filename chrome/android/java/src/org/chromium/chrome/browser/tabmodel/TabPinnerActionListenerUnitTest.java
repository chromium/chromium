// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

/** Unit tests for {@link TabPinnerActionListener}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabPinnerActionListenerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mRunnable;
    @Mock private TabModelActionListener mInnerListener;

    private TabPinnerActionListener mListener;

    @Before
    public void setUp() {
        mListener = new TabPinnerActionListener(mRunnable, mInnerListener);
    }

    @Test
    public void testNoDialog() {
        mListener.willPerformActionOrShowDialog(DialogType.NONE, true);
        verify(mInnerListener).willPerformActionOrShowDialog(DialogType.NONE, true);
        mListener.pinIfCollaborationDialogShown();
        verify(mRunnable, never()).run();

        mListener.onConfirmationDialogResult(
                DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mInnerListener)
                .onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mRunnable).run();
    }

    @Test
    public void testOnConfirmationDialogResult_Sync_Positive() {
        mListener.willPerformActionOrShowDialog(DialogType.SYNC, false);
        verify(mInnerListener).willPerformActionOrShowDialog(DialogType.SYNC, false);
        mListener.pinIfCollaborationDialogShown();
        verify(mRunnable, never()).run();

        mListener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mInnerListener)
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mRunnable).run();
    }

    @Test
    public void testOnConfirmationDialogResult_Sync_Immediate() {
        mListener.willPerformActionOrShowDialog(DialogType.SYNC, false);
        verify(mInnerListener).willPerformActionOrShowDialog(DialogType.SYNC, false);
        mListener.pinIfCollaborationDialogShown();
        verify(mRunnable, never()).run();

        mListener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mInnerListener)
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mRunnable).run();
    }

    @Test
    public void testOnConfirmationDialogResult_Sync_Negative() {
        mListener.willPerformActionOrShowDialog(DialogType.SYNC, false);
        verify(mInnerListener).willPerformActionOrShowDialog(DialogType.SYNC, false);
        mListener.pinIfCollaborationDialogShown();
        verify(mRunnable, never()).run();

        mListener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        verify(mInnerListener)
                .onConfirmationDialogResult(
                        DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        verify(mRunnable, never()).run();
    }

    @Test
    public void testCollaborationFlow() {
        mListener.willPerformActionOrShowDialog(DialogType.COLLABORATION, false);
        verify(mInnerListener).willPerformActionOrShowDialog(DialogType.COLLABORATION, false);
        verify(mRunnable, never()).run();

        mListener.pinIfCollaborationDialogShown();
        verify(mRunnable).run();

        mListener.onConfirmationDialogResult(
                DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mInnerListener)
                .onConfirmationDialogResult(
                        DialogType.COLLABORATION, ActionConfirmationResult.CONFIRMATION_POSITIVE);

        // Still only run once.
        verify(mRunnable).run();
    }

    @Test
    public void testNoInnerListener_ImmediateContinue() {
        mListener = new TabPinnerActionListener(mRunnable, null);
        mListener.willPerformActionOrShowDialog(DialogType.NONE, true);
        mListener.onConfirmationDialogResult(
                DialogType.NONE, ActionConfirmationResult.IMMEDIATE_CONTINUE);
        verify(mRunnable).run();
    }
}
