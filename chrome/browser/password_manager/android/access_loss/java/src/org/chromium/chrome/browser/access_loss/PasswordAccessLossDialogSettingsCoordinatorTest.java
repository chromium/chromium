// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.PasswordAccessLossWarningUserAction.DISMISS;
import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.PasswordAccessLossWarningUserAction.HELP_CENTER;
import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.PasswordAccessLossWarningUserAction.MAIN_ACTION;
import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.getDialogShownHistogramName;
import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.getDialogUserActionHistogramName;
import static org.chromium.chrome.browser.access_loss.HelpUrlLauncher.GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL;
import static org.chromium.chrome.browser.access_loss.HelpUrlLauncher.KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.password_manager.CustomTabIntentHelper;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.ui.widget.ChromeImageButton;

/** Tests for {@link PasswordAccessLossDialogSettingsCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordAccessLossDialogSettingsCoordinatorTest {

    private PasswordAccessLossDialogSettingsCoordinator mCoordinator =
            new PasswordAccessLossDialogSettingsCoordinator();
    private FakeModalDialogManager mModalDialogManager =
            new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
    private Activity mActivity;

    @Mock private Callback<Context> mLaunchGmsCoreUpdate;
    @Mock private Runnable mLaunchExportFlow;
    private CustomTabIntentHelper mCustomTabIntentHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mCustomTabIntentHelper = (Context context, Intent intent) -> intent;
        mActivity = Robolectric.buildActivity(Activity.class).create().start().resume().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void showsAndHidesAccessLossDialog() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getDialogShownHistogramName(),
                                PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED)
                        .expectIntRecords(
                                getDialogUserActionHistogramName(
                                        PasswordAccessLossWarningType
                                                .NEW_GMS_CORE_MIGRATION_FAILED),
                                DISMISS)
                        .build();

        mCoordinator.showPasswordAccessLossDialog(
                mActivity,
                mModalDialogManager,
                PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED,
                mLaunchGmsCoreUpdate,
                mLaunchExportFlow,
                mCustomTabIntentHelper);
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(mDialogModel);

        mModalDialogManager.clickNegativeButton();
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        histogram.assertExpected();
    }

    @Test
    public void launchesGmsCoreUpdateWhenNoUpm() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getDialogUserActionHistogramName(
                                        PasswordAccessLossWarningType.NO_UPM),
                                MAIN_ACTION)
                        .build();

        mCoordinator.showPasswordAccessLossDialog(
                mActivity,
                mModalDialogManager,
                PasswordAccessLossWarningType.NO_UPM,
                mLaunchGmsCoreUpdate,
                mLaunchExportFlow,
                mCustomTabIntentHelper);
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(mDialogModel);

        mModalDialogManager.clickPositiveButton();
        verify(mLaunchGmsCoreUpdate).onResult(any());
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        histogram.assertExpected();
    }

    @Test
    public void launchesGmsCoreUpdateWhenOnlyAccountUpm() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getDialogUserActionHistogramName(
                                        PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM),
                                MAIN_ACTION)
                        .build();

        mCoordinator.showPasswordAccessLossDialog(
                mActivity,
                mModalDialogManager,
                PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM,
                mLaunchGmsCoreUpdate,
                mLaunchExportFlow,
                mCustomTabIntentHelper);
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(mDialogModel);

        mModalDialogManager.clickPositiveButton();
        verify(mLaunchGmsCoreUpdate).onResult(any());
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        histogram.assertExpected();
    }

    @Test
    public void launchesExportFlowWhenNoGmsCore() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getDialogUserActionHistogramName(
                                        PasswordAccessLossWarningType.NO_GMS_CORE),
                                MAIN_ACTION)
                        .build();

        mCoordinator.showPasswordAccessLossDialog(
                mActivity,
                mModalDialogManager,
                PasswordAccessLossWarningType.NO_GMS_CORE,
                mLaunchGmsCoreUpdate,
                mLaunchExportFlow,
                mCustomTabIntentHelper);
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(mDialogModel);

        mModalDialogManager.clickPositiveButton();
        verify(mLaunchExportFlow).run();
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        histogram.assertExpected();
    }

    @Test
    public void launchesExportFlowWhenMigrationFailed() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getDialogUserActionHistogramName(
                                        PasswordAccessLossWarningType
                                                .NEW_GMS_CORE_MIGRATION_FAILED),
                                MAIN_ACTION)
                        .build();

        mCoordinator.showPasswordAccessLossDialog(
                mActivity,
                mModalDialogManager,
                PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED,
                mLaunchGmsCoreUpdate,
                mLaunchExportFlow,
                mCustomTabIntentHelper);
        PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        Assert.assertNotNull(mDialogModel);

        mModalDialogManager.clickPositiveButton();
        verify(mLaunchExportFlow).run();
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
        histogram.assertExpected();
    }

    @Test
    public void opensGmsCoreHelpWhenHelpButtonClicked() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getDialogUserActionHistogramName(
                                        PasswordAccessLossWarningType.NO_UPM),
                                HELP_CENTER)
                        .build();

        Activity spyActivity = spy(mActivity);
        mCoordinator.showPasswordAccessLossDialog(
                spyActivity,
                mModalDialogManager,
                PasswordAccessLossWarningType.NO_UPM,
                mLaunchGmsCoreUpdate,
                mLaunchExportFlow,
                mCustomTabIntentHelper);
        findHelpButton().performClick();
        ArgumentCaptor<Intent> intentArgumentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentArgumentCaptor.capture(), any());
        Assert.assertEquals(
                Uri.parse(KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL),
                intentArgumentCaptor.getValue().getData());

        histogram.assertExpected();
    }

    @Test
    public void opensGmsCoreSupportedDevicesHelpWhenHelpButtonClicked() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getDialogUserActionHistogramName(
                                        PasswordAccessLossWarningType.NO_GMS_CORE),
                                HELP_CENTER)
                        .build();

        Activity spyActivity = spy(mActivity);
        mCoordinator.showPasswordAccessLossDialog(
                spyActivity,
                mModalDialogManager,
                PasswordAccessLossWarningType.NO_GMS_CORE,
                mLaunchGmsCoreUpdate,
                mLaunchExportFlow,
                mCustomTabIntentHelper);
        findHelpButton().performClick();
        ArgumentCaptor<Intent> intentArgumentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentArgumentCaptor.capture(), any());
        Assert.assertEquals(
                Uri.parse(GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL),
                intentArgumentCaptor.getValue().getData());

        histogram.assertExpected();
    }

    private ChromeImageButton findHelpButton() {
        final PropertyModel mDialogModel = mModalDialogManager.getShownDialogModel();
        final View customView = mDialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        Assert.assertNotNull(customView);
        return customView.findViewById(R.id.help_button);
    }
}
