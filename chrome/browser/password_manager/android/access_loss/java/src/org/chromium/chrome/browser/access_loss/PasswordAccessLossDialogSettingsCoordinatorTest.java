// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Tests for {@link PasswordAccessLossDialogSettingsCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordAccessLossDialogSettingsCoordinatorTest {
    private PasswordAccessLossDialogSettingsCoordinator mCoordinator =
            new PasswordAccessLossDialogSettingsCoordinator();
    private FakeModalDialogManager mModalDialogManager =
            new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);

    @Mock private Callback<Context> mLaunchGmsCoreUpdate;
    @Mock private Callback<Context> mLaunchExportFlow;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
    }

    @Test
    public void showsAndHidesAccessLossDialog() {
        mCoordinator.showPasswordAccessLossDialog(
                ContextUtils.getApplicationContext(),
                mModalDialogManager,
                PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED,
                mLaunchGmsCoreUpdate,
                mLaunchExportFlow);
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(mDialogModel);

        mModalDialogManager.clickNegativeButton();
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void launchesGmsCoreUpdateWhenNoUpm() {
        mCoordinator.showPasswordAccessLossDialog(
                ContextUtils.getApplicationContext(),
                mModalDialogManager,
                PasswordAccessLossWarningType.NO_UPM,
                mLaunchGmsCoreUpdate,
                mLaunchExportFlow);
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(mDialogModel);

        mModalDialogManager.clickPositiveButton();
        verify(mLaunchGmsCoreUpdate).onResult(any());
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void launchesGmsCoreUpdateWhenOnlyAccountUpm() {
        mCoordinator.showPasswordAccessLossDialog(
                ContextUtils.getApplicationContext(),
                mModalDialogManager,
                PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM,
                mLaunchGmsCoreUpdate,
                mLaunchExportFlow);
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(mDialogModel);

        mModalDialogManager.clickPositiveButton();
        verify(mLaunchGmsCoreUpdate).onResult(any());
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void launchesExportFlowWhenNoGmsCore() {
        mCoordinator.showPasswordAccessLossDialog(
                ContextUtils.getApplicationContext(),
                mModalDialogManager,
                PasswordAccessLossWarningType.NO_GMS_CORE,
                mLaunchGmsCoreUpdate,
                mLaunchExportFlow);
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(mDialogModel);

        mModalDialogManager.clickPositiveButton();
        verify(mLaunchExportFlow).onResult(any());
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void launchesExportFlowWhenMigrationFailed() {
        mCoordinator.showPasswordAccessLossDialog(
                ContextUtils.getApplicationContext(),
                mModalDialogManager,
                PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED,
                mLaunchGmsCoreUpdate,
                mLaunchExportFlow);
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(mDialogModel);

        mModalDialogManager.clickPositiveButton();
        verify(mLaunchExportFlow).onResult(any());
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
    }
}
