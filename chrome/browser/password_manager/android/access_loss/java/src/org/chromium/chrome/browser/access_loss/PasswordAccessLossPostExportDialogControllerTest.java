// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.password_manager.HelpUrlLauncher;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.ui.widget.ChromeImageButton;

/** Tests for {@link PasswordAccessLossPostExportDialogController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasswordAccessLossPostExportDialogControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Activity mActivity;
    private FakeModalDialogManager mModalDialogManager;

    @Mock private SettingsCustomTabLauncher mSettingsCustomTabLauncher;
    PasswordAccessLossPostExportDialogController mController;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().start().resume().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mModalDialogManager = new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
    }

    @Test
    public void testDialogDismissed() {
        mController =
                new PasswordAccessLossPostExportDialogController(
                        mActivity, mModalDialogManager, mSettingsCustomTabLauncher);
        mController.showPostExportDialog();
        assertNotNull(mModalDialogManager.getShownDialogModel());

        mModalDialogManager.clickNegativeButton();
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void testOpensGmsCoreSupportedDevicesHelpArticle() {
        mController =
                new PasswordAccessLossPostExportDialogController(
                        mActivity, mModalDialogManager, mSettingsCustomTabLauncher);
        mController.showPostExportDialog();

        getHelpButton().performClick();
        verify(mSettingsCustomTabLauncher)
                .openUrlInCct(
                        eq(mActivity),
                        eq(HelpUrlLauncher.GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL));
    }

    private ChromeImageButton getHelpButton() {
        final PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        final View customView = mDialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        assertNotNull(customView);
        return customView.findViewById(R.id.help_button);
    }
}
