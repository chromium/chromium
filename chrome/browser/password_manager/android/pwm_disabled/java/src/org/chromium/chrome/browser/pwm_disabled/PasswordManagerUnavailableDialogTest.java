// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Tests for {@link PasswordManagerUnavailableDialogCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordManagerUnavailableDialogTest {

    private final PasswordManagerUnavailableDialogCoordinator mCoordinator =
            new PasswordManagerUnavailableDialogCoordinator();
    private final FakeModalDialogManager mModalDialogManager =
            new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
    private Activity mActivity;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Callback<Context> mLaunchGmsCoreUpdate;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().start().resume().get();
        mActivity.setTheme(
                org.chromium.components.browser_ui.test.R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void showsAndHidesDialog() {
        mCoordinator.showDialog(mActivity, mModalDialogManager, mLaunchGmsCoreUpdate);
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        assertNotNull(dialogModel);

        mModalDialogManager.clickNegativeButton();
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void launchesGmsCoreUpdateIfUpdateDialog() {
        mCoordinator.showDialog(mActivity, mModalDialogManager, mLaunchGmsCoreUpdate);
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        assertNotNull(dialogModel);

        mModalDialogManager.clickPositiveButton();
        verify(mLaunchGmsCoreUpdate).onResult(any());
        assertNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    public void setsCorrectContentsForUpdateDialog() {
        mCoordinator.showDialog(mActivity, mModalDialogManager, mLaunchGmsCoreUpdate);
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        assertNotNull(dialogModel);

        assertEquals(
                mActivity.getResources().getString(R.string.access_loss_update_gms_title),
                dialogModel.get(ModalDialogProperties.TITLE));
        assertEquals(
                mActivity.getResources().getString(R.string.pwm_disabled_update_dialog_description),
                dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS).get(0));
        assertEquals(1, dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS).size());
        assertEquals(
                mActivity
                        .getResources()
                        .getString(R.string.pwd_access_loss_warning_update_gms_core_button_text),
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                mActivity.getResources().getString(R.string.pwm_disabled_update_dialog_cancel),
                dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        assertEquals(
                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE,
                dialogModel.get(ModalDialogProperties.BUTTON_STYLES));
    }

    @Test
    public void setsCorrectContentsForNonUpdateDialog() {
        mCoordinator.showDialog(mActivity, mModalDialogManager, null);
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        assertNotNull(dialogModel);

        assertEquals(
                mActivity.getResources().getString(R.string.pwm_disabled_no_gms_dialog_title),
                dialogModel.get(ModalDialogProperties.TITLE));
        assertEquals(
                mActivity
                        .getResources()
                        .getString(R.string.pwm_disabled_no_gms_dialog_description_paragraph1),
                dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS).get(0));
        assertEquals(
                mActivity
                        .getResources()
                        .getString(R.string.pwm_disabled_no_gms_dialog_description_paragraph2),
                dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS).get(1));
        assertEquals(2, dialogModel.get(ModalDialogProperties.MESSAGE_PARAGRAPHS).size());
        assertEquals(
                mActivity.getResources().getString(R.string.pwm_disabled_no_gms_dialog_button_text),
                dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertNull(dialogModel.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        assertEquals(
                ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE,
                dialogModel.get(ModalDialogProperties.BUTTON_STYLES));
    }

    @Test
    public void recordsNoGmsDialogShownHistogram() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PwmDeprecationDialogsMetricsRecorder
                                .NO_GMS_NO_PASSWORDS_DIALOG_SHOWN_HISTOGRAM,
                        true);
        mCoordinator.showDialog(mActivity, mModalDialogManager, null);
        histogramWatcher.assertExpected();
    }

    @Test
    public void recordsOldGmsDialogAccepted() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PwmDeprecationDialogsMetricsRecorder
                                .OLD_GMS_NO_PASSWORDS_DIALOG_DISMISSAL_REASON_HISTOGRAM,
                        true);
        mCoordinator.showDialog(mActivity, mModalDialogManager, mLaunchGmsCoreUpdate);
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        assertNotNull(dialogModel);

        mModalDialogManager.clickPositiveButton();
        histogramWatcher.assertExpected();
    }

    @Test
    public void recordsOldGmsDialogRejectedIfNegativeButtonClicked() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PwmDeprecationDialogsMetricsRecorder
                                .OLD_GMS_NO_PASSWORDS_DIALOG_DISMISSAL_REASON_HISTOGRAM,
                        false);
        mCoordinator.showDialog(mActivity, mModalDialogManager, mLaunchGmsCoreUpdate);
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        assertNotNull(dialogModel);

        mModalDialogManager.clickNegativeButton();
        histogramWatcher.assertExpected();
    }

    @Test
    public void recordsOldGmsDialogRejectedIfDialogDismissedWithNoButtonClick() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PwmDeprecationDialogsMetricsRecorder
                                .OLD_GMS_NO_PASSWORDS_DIALOG_DISMISSAL_REASON_HISTOGRAM,
                        false);
        mCoordinator.showDialog(mActivity, mModalDialogManager, mLaunchGmsCoreUpdate);
        PropertyModel dialogModel = mModalDialogManager.getShownDialogModel();
        assertNotNull(dialogModel);

        mModalDialogManager.dismissDialog(
                dialogModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        histogramWatcher.assertExpected();
    }
}
