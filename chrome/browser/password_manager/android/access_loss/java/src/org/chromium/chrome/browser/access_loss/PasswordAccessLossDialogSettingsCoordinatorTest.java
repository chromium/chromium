// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

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

    @Test
    public void showsAndHidesAccessLossDialog() {
        mCoordinator.showPasswordAccessLossDialog(
                ContextUtils.getApplicationContext(),
                mModalDialogManager,
                PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(mDialogModel);

        mModalDialogManager.clickNegativeButton();
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
    }
}
