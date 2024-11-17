// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.password_manager.CustomTabIntentHelper;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.ui.widget.ChromeImageButton;

/** Tests for {@link PasswordAccessLossPostExportDialogController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasswordAccessLossPostExportDialogControllerTest {
    private Activity mActivity;
    private FakeModalDialogManager mModalDialogManager;
    private CustomTabIntentHelper mCustomTabIntentHelper;
    PasswordAccessLossPostExportDialogController mController;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().start().resume().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        mCustomTabIntentHelper = (Context context, Intent intent) -> intent;
    }

    @Test
    public void testDialogDismissed() {
        mController =
                new PasswordAccessLossPostExportDialogController(
                        mActivity, mModalDialogManager, mCustomTabIntentHelper);
        mController.showPostExportDialog();
        assertNotNull(mModalDialogManager.getShownDialogModel());

        mModalDialogManager.clickNegativeButton();
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void testOpensGmsCoreSupportedDevicesHelpArticle() {
        Activity spyActivity = spy(mActivity);
        mController =
                new PasswordAccessLossPostExportDialogController(
                        spyActivity, mModalDialogManager, mCustomTabIntentHelper);
        mController.showPostExportDialog();

        getHelpButton().performClick();
        ArgumentCaptor<Intent> intentArgumentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentArgumentCaptor.capture(), any());
        assertEquals(
                Uri.parse(HelpUrlLauncher.GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL),
                intentArgumentCaptor.getValue().getData());
    }

    private ChromeImageButton getHelpButton() {
        final PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        final View customView = mDialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        assertNotNull(customView);
        return customView.findViewById(R.id.help_button);
    }
}
