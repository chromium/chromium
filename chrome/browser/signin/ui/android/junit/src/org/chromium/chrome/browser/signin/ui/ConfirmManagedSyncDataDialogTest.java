// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.widget.Button;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowAlertDialog;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for {@link ConfirmManagedSyncDataDialog}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ConfirmManagedSyncDataDialogTest {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    @Mock
    private ConfirmManagedSyncDataDialog.Listener mMockListener;

    private FragmentActivity mActivity;

    private ConfirmSyncDataStateMachineDelegate mStateMachineDelegate;

    @Before
    public void setUp() {
        initMocks(this);
        mActivity = Robolectric.setupActivity(FragmentActivity.class);
        mStateMachineDelegate =
                new ConfirmSyncDataStateMachineDelegate(mActivity.getSupportFragmentManager());
    }

    @Test
    public void testListenerOnConfirmWhenPositiveButtonClicked() {
        AlertDialog dialog = getSignInToManagedAccountDialog();
        Button positiveButton = dialog.getButton(AlertDialog.BUTTON_POSITIVE);
        Assert.assertEquals(
                mActivity.getString(R.string.policy_dialog_proceed), positiveButton.getText());
        positiveButton.performClick();
        verify(mMockListener).onConfirm();
    }

    @Test
    public void testListenerOnCancelWhenNegativeButtonClicked() {
        AlertDialog dialog = getSignInToManagedAccountDialog();
        Button negativeButton = dialog.getButton(AlertDialog.BUTTON_NEGATIVE);
        Assert.assertEquals(mActivity.getString(R.string.cancel), negativeButton.getText());
        negativeButton.performClick();
        verify(mMockListener).onCancel();
    }

    @Test
    public void testListenerOnCancelNotCalledWhenDialogDismissed() {
        getSignInToManagedAccountDialog();
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        Assert.assertEquals(1, fragmentManager.getFragments().size());
        mStateMachineDelegate.dismissAllDialogs();
        Assert.assertEquals(0, fragmentManager.getFragments().size());
        verify(mMockListener, never()).onCancel();
    }

    @Test
    public void testListenerOnCancelNotCalledOnDismissWhenButtonClicked() {
        AlertDialog dialog = getSignInToManagedAccountDialog();
        dialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        dialog.dismiss();
        verify(mMockListener, never()).onCancel();
    }

    private AlertDialog getSignInToManagedAccountDialog() {
        mStateMachineDelegate.showSignInToManagedAccountDialog(mMockListener, TEST_DOMAIN);
        return (AlertDialog) ShadowAlertDialog.getLatestDialog();
    }
}
