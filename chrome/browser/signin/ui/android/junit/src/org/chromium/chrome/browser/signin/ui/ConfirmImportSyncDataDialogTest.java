// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

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
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;

/**
 * Tests for {@link ConfirmImportSyncDataDialog}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ConfirmImportSyncDataDialogTest {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    @Mock
    private ConfirmImportSyncDataDialog.Listener mMockListener;

    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private Profile mProfile;

    private FragmentManager mFragmentManager;

    private ConfirmSyncDataStateMachineDelegate mStateMachineDelegate;

    @Before
    public void setUp() {
        initMocks(this);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        Profile.setLastUsedProfileForTesting(mProfile);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        mFragmentManager =
                Robolectric.setupActivity(FragmentActivity.class).getSupportFragmentManager();
        mStateMachineDelegate = new ConfirmSyncDataStateMachineDelegate(mFragmentManager);
    }

    @Test
    public void testPositiveButtonWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        AlertDialog alertDialog = getConfirmImportSyncDataDialog();
        alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        verify(mMockListener).onConfirm(true);
    }

    @Test
    public void testPositiveButtonWhenAccountIsNotManaged() {
        AlertDialog alertDialog = getConfirmImportSyncDataDialog();
        alertDialog.findViewById(R.id.sync_confirm_import_choice).performClick();
        alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        verify(mMockListener).onConfirm(false);
    }

    @Test
    public void testNegativeButton() {
        AlertDialog alertDialog = getConfirmImportSyncDataDialog();
        alertDialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
        verify(mMockListener).onCancel();
    }

    @Test
    public void testListenerOnCancelNotCalledWhenDialogDismissed() {
        getConfirmImportSyncDataDialog();
        Assert.assertEquals(1, mFragmentManager.getFragments().size());
        mStateMachineDelegate.dismissAllDialogs();
        Assert.assertEquals(0, mFragmentManager.getFragments().size());
        verify(mMockListener, never()).onCancel();
    }

    @Test
    public void testListenerOnCancelNotCalledOnDismissWhenButtonClicked() {
        AlertDialog dialog = getConfirmImportSyncDataDialog();
        dialog.findViewById(R.id.sync_confirm_import_choice).performClick();
        dialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        dialog.dismiss();
        verify(mMockListener, never()).onCancel();
    }

    @Test
    public void testToastOfConfirmImportOptionForManagedAccount() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        AlertDialog dialog = getConfirmImportSyncDataDialog();
        dialog.findViewById(R.id.sync_confirm_import_choice).performClick();
        Assert.assertTrue(ShadowToast.showedCustomToast(
                dialog.getContext().getString(R.string.managed_by_your_organization),
                R.id.toast_text));
    }

    private AlertDialog getConfirmImportSyncDataDialog() {
        mStateMachineDelegate.showConfirmImportSyncDataDialog(
                mMockListener, "old.testaccount@gmail.com", "new.testaccount@gmail.com");
        return (AlertDialog) ShadowAlertDialog.getLatestDialog();
    }
}
