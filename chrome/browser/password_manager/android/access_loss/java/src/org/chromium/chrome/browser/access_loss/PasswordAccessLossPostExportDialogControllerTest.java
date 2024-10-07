// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Tests for {@link PasswordAccessLossPostExportDialogController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasswordAccessLossPostExportDialogControllerTest {
    private final Context mContext =
            new ContextThemeWrapper(
                    ApplicationProvider.getApplicationContext(),
                    org.chromium.chrome.browser.access_loss.R.style.Theme_BrowserUI_DayNight);
    private FakeModalDialogManager mModalDialogManager;
    PasswordAccessLossPostExportDialogController mController;

    @Before
    public void setUp() {
        mModalDialogManager = new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        mController =
                new PasswordAccessLossPostExportDialogController(mContext, mModalDialogManager);
    }

    @Test
    public void testDialogDismissed() {
        mController.showPostExportDialog();
        assertNotNull(mModalDialogManager.getShownDialogModel());

        mModalDialogManager.clickNegativeButton();
        assertNull(mModalDialogManager.getShownDialogModel());
    }
}
