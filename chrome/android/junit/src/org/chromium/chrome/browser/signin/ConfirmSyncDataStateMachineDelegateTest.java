// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowAlertDialog;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link ConfirmSyncDataStateMachineDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ConfirmSyncDataStateMachineDelegateTest {
    private FragmentManager mFragmentManager;
    private ConfirmSyncDataStateMachineDelegate mStateMachineDelegate;

    @Before
    public void setUp() {
        mFragmentManager =
                Robolectric.setupActivity(FragmentActivity.class).getSupportFragmentManager();
        mStateMachineDelegate = new ConfirmSyncDataStateMachineDelegate(mFragmentManager);
    }

    @Test
    public void testTimeoutDialogWhenPositiveButtonPressed() {
        ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener mockListener =
                mock(ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener.class);
        mStateMachineDelegate.showFetchManagementPolicyTimeoutDialog(mockListener);
        AlertDialog alertDialog = (AlertDialog) ShadowAlertDialog.getLatestDialog();
        alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        verify(mockListener).onRetry();
    }

    @Test
    public void testTimeoutDialogWhenNegativeButtonPressed() {
        ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener mockListener =
                mock(ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener.class);
        mStateMachineDelegate.showFetchManagementPolicyTimeoutDialog(mockListener);
        AlertDialog alertDialog = (AlertDialog) ShadowAlertDialog.getLatestDialog();
        alertDialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
        verify(mockListener).onCancel();
    }

    @Test
    public void testProgressDialog() {
        ConfirmSyncDataStateMachineDelegate.ProgressDialogListener mockListener =
                mock(ConfirmSyncDataStateMachineDelegate.ProgressDialogListener.class);
        mStateMachineDelegate.showFetchManagementPolicyProgressDialog(mockListener);
        AlertDialog alertDialog = (AlertDialog) ShadowAlertDialog.getLatestDialog();
        alertDialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
        verify(mockListener).onCancel();
    }

    @Test
    public void testDismissAllDialogs() {
        ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener mockListener =
                mock(ConfirmSyncDataStateMachineDelegate.TimeoutDialogListener.class);
        mStateMachineDelegate.showFetchManagementPolicyTimeoutDialog(mockListener);
        Assert.assertEquals(1, mFragmentManager.getFragments().size());
        mStateMachineDelegate.dismissAllDialogs();
        Assert.assertEquals(0, mFragmentManager.getFragments().size());
    }
}
