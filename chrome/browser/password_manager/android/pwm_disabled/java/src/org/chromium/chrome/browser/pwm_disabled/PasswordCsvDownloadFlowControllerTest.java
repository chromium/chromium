// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import static android.app.Activity.RESULT_OK;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Dialog;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.FragmentActivity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowContentResolver;
import org.robolectric.shadows.ShadowDialog;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridge;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwm_disabled.PwmDeprecationDialogsMetricsRecorder.DownloadCsvFlowStep;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.test.BrowserUiTestFragmentActivity;
import org.chromium.ui.widget.ToastManager;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

/** Tests for {@link PasswordsCsvDownloadDialogController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowToast.class})
@DoNotBatch(reason = "The ReauthenticationManager setup should not leak between tests.")
public class PasswordCsvDownloadFlowControllerTest {
    private static final String TEST_FILE_DATA =
            "name,url,username,password,note\n"
                    + "example.com,https://example.com/,Someone,Secret,\"Note Line 1\n"
                    + "\"Note Line 2\"";

    private static final String NO_GMS_DOWNLOAD_FLOW_LAST_STEP_HISTOGRAM =
            "PasswordManager.UPM.NoGms.DownloadCsvFlowLastStep";
    private static final String OLD_GMS_DOWNLOAD_FLOW_LAST_STEP_HISTOGRAM =
            "PasswordManager.UPM.OldGms.DownloadCsvFlowLastStep";
    private static final String FULL_SUPPORT_GMS_DOWNLOAD_FLOW_LAST_STEP_HISTOGRAM =
            "PasswordManager.UPM.FullUpmSupportGms.DownloadCsvFlowLastStep";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private PasswordCsvDownloadFlowController mController;
    private FragmentActivity mActivity;
    @Mock Runnable mEndOfFlowCallback;
    @Mock Profile mProfile;
    @Mock ReauthenticatorBridge mReauthenticatorBridge;
    @Mock LoginDbDeprecationUtilBridge.Natives mLoginDbDeprecationUtilBridge;
    @Mock SettingsCustomTabLauncher mSettingsCustomTabLauncher;

    @Before
    public void setUp() {
        mActivity =
                Robolectric.buildActivity(BrowserUiTestFragmentActivity.class)
                        .create()
                        .start()
                        .resume()
                        .get();
        mActivity.setTheme(
                org.chromium.components.browser_ui.test.R.style.Theme_BrowserUI_DayNight);

        LoginDbDeprecationUtilBridgeJni.setInstanceForTesting(mLoginDbDeprecationUtilBridge);
    }

    @After
    public void tearDown() {
        ToastManager.resetForTesting();
        ShadowToast.reset();
    }

    @Test
    public void testScreenLockNotAvailable() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FULL_SUPPORT_GMS_DOWNLOAD_FLOW_LAST_STEP_HISTOGRAM,
                        DownloadCsvFlowStep.NO_SCREEN_LOCK);
        mController = new PasswordCsvDownloadFlowController(mEndOfFlowCallback);
        mController.showDialogAndStartFlow(
                mActivity, mProfile, true, true, mSettingsCustomTabLauncher);
        mActivity.getSupportFragmentManager().executePendingTransactions();

        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorBridge);
        when(mReauthenticatorBridge.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.UNAVAILABLE);

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.positive_button).performClick();

        ShadowLooper.idleMainLooper();

        Resources resources = RuntimeEnvironment.getApplication().getResources();

        assertTrue(
                "Wrong/no toast shown",
                ShadowToast.showedCustomToast(
                        resources.getString(R.string.password_export_set_lock_screen),
                        R.id.toast_text));
        mActivity.getSupportFragmentManager().executePendingTransactions();
        assertFalse(dialog.isShowing());
        verify(mEndOfFlowCallback).run();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testScreenLockAvailableAuthFailed() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FULL_SUPPORT_GMS_DOWNLOAD_FLOW_LAST_STEP_HISTOGRAM,
                        DownloadCsvFlowStep.REAUTH_FAILED);
        mController = new PasswordCsvDownloadFlowController(mEndOfFlowCallback);
        mController.showDialogAndStartFlow(
                mActivity, mProfile, true, true, mSettingsCustomTabLauncher);
        mActivity.getSupportFragmentManager().executePendingTransactions();

        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorBridge);
        when(mReauthenticatorBridge.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.positive_button).performClick();

        ArgumentCaptor<Callback<Boolean>> resultCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mReauthenticatorBridge).reauthenticate(resultCallbackCaptor.capture());
        resultCallbackCaptor.getValue().onResult(false);

        assertFalse(dialog.isShowing());
        verify(mEndOfFlowCallback).run();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDestinationNotSet() throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FULL_SUPPORT_GMS_DOWNLOAD_FLOW_LAST_STEP_HISTOGRAM,
                        DownloadCsvFlowStep.CANCELLED_FILE_SELECTION);
        // Make sure the auto-exported csv is set up.
        File sourceFile = setUpTempAutoExportedCsv(TEST_FILE_DATA);

        mController = new PasswordCsvDownloadFlowController(mEndOfFlowCallback);
        mController.showDialogAndStartFlow(
                mActivity, mProfile, true, true, mSettingsCustomTabLauncher);
        mActivity.getSupportFragmentManager().executePendingTransactions();

        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorBridge);
        when(mReauthenticatorBridge.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);

        Dialog exportDialog = ShadowDialog.getLatestDialog();
        exportDialog.findViewById(R.id.positive_button).performClick();

        ArgumentCaptor<Callback<Boolean>> resultCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mReauthenticatorBridge).reauthenticate(resultCallbackCaptor.capture());
        resultCallbackCaptor.getValue().onResult(true);

        ShadowActivity shadowActivity = shadowOf(mActivity);
        Intent startedIntent = shadowActivity.getNextStartedActivityForResult().intent;
        // Verify that the create document intent was triggered (creating file in Downloads for
        // exported passwords).
        assertEquals(Intent.ACTION_CREATE_DOCUMENT, startedIntent.getAction());

        // Simulate the user cancelling the activity and not setting a destination file
        shadowActivity.receiveResult(startedIntent, RESULT_OK, new Intent().setData(null));
        ShadowLooper.idleMainLooper();

        assertFalse(exportDialog.isShowing());

        // The source file should not have been deleted, because the write to the destination
        // file didn't complete.
        assertTrue(sourceFile.exists());
        verify(mEndOfFlowCallback).run();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testErrorDialog() throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FULL_SUPPORT_GMS_DOWNLOAD_FLOW_LAST_STEP_HISTOGRAM,
                        DownloadCsvFlowStep.CSV_WRITE_FAILED);
        // Make sure the auto-exported csv is set up.
        File sourceFile = setUpTempAutoExportedCsv(TEST_FILE_DATA);

        mController = new PasswordCsvDownloadFlowController(mEndOfFlowCallback);
        mController.showDialogAndStartFlow(
                mActivity, mProfile, true, true, mSettingsCustomTabLauncher);
        mActivity.getSupportFragmentManager().executePendingTransactions();

        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorBridge);
        when(mReauthenticatorBridge.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);

        Dialog exportDialog = ShadowDialog.getLatestDialog();
        exportDialog.findViewById(R.id.positive_button).performClick();

        ArgumentCaptor<Callback<Boolean>> resultCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mReauthenticatorBridge).reauthenticate(resultCallbackCaptor.capture());
        resultCallbackCaptor.getValue().onResult(true);

        ShadowActivity shadowActivity = shadowOf(mActivity);
        Intent startedIntent = shadowActivity.getNextStartedActivityForResult().intent;
        // Verify that the create document intent was triggered (creating file in Downloads for
        // exported passwords).
        assertEquals(Intent.ACTION_CREATE_DOCUMENT, startedIntent.getAction());

        // Create the destination file.
        File destinationFile = File.createTempFile("exportedpasswords", "csv", null);
        destinationFile.deleteOnExit();

        // Mock the input stream to simulate an error
        InputStream inputStream = mock(InputStream.class);
        ShadowContentResolver shadowContentResolver =
                shadowOf(ContextUtils.getApplicationContext().getContentResolver());
        shadowContentResolver.registerInputStream(Uri.fromFile(sourceFile), inputStream);
        when(inputStream.read(any())).thenThrow(new IOException());

        // Return the result of the create document intent (the file name).
        shadowActivity.receiveResult(
                startedIntent, RESULT_OK, new Intent().setData(Uri.fromFile(destinationFile)));
        BaseRobolectricTestRule.runAllBackgroundAndUi();

        assertFalse(exportDialog.isShowing());

        Dialog errorDialog = ShadowDialog.getLatestDialog();
        assertTrue(errorDialog instanceof AlertDialog);
        AlertDialog errorAlertDialog = (AlertDialog) errorDialog;
        TextView description =
                errorAlertDialog.findViewById(
                        org.chromium.chrome.browser.password_manager.R.id
                                .passwords_error_main_description);
        assertNotNull(description);
        assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(R.string.password_settings_export_tips),
                description.getText());
        errorAlertDialog
                .getButton(androidx.appcompat.app.AlertDialog.BUTTON_NEGATIVE)
                .performClick();
        BaseRobolectricTestRule.runAllBackgroundAndUi();

        // The source file should not have been deleted, because the write to the destination
        // file didn't complete.
        assertTrue(sourceFile.exists());
        verify(mEndOfFlowCallback).run();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDownloadFlow() throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        FULL_SUPPORT_GMS_DOWNLOAD_FLOW_LAST_STEP_HISTOGRAM,
                        DownloadCsvFlowStep.SUCCESS);
        // Make sure the auto-exported csv is set up.
        File sourceFile = setUpTempAutoExportedCsv(TEST_FILE_DATA);

        mController = new PasswordCsvDownloadFlowController(mEndOfFlowCallback);
        mController.showDialogAndStartFlow(
                mActivity, mProfile, true, true, mSettingsCustomTabLauncher);
        mActivity.getSupportFragmentManager().executePendingTransactions();

        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorBridge);
        when(mReauthenticatorBridge.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.positive_button).performClick();

        ArgumentCaptor<Callback<Boolean>> resultCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mReauthenticatorBridge).reauthenticate(resultCallbackCaptor.capture());
        resultCallbackCaptor.getValue().onResult(true);

        ShadowActivity shadowActivity = shadowOf(mActivity);
        Intent startedIntent = shadowActivity.getNextStartedActivityForResult().intent;
        // Verify that the create document intent was triggered (creating file in Downloads for
        // exported passwords).
        assertEquals(Intent.ACTION_CREATE_DOCUMENT, startedIntent.getAction());

        // Create the destination file.
        File destinationFile = File.createTempFile("exportedpasswords", "csv", null);
        destinationFile.deleteOnExit();

        // Return the result of the create document intent (the file name).
        shadowActivity.receiveResult(
                startedIntent, RESULT_OK, new Intent().setData(Uri.fromFile(destinationFile)));
        BaseRobolectricTestRule.runAllBackgroundAndUi();

        assertFalse(dialog.isShowing());

        verifyFakeDataWasCopiedOver(destinationFile);

        // The source file should be deleted.
        assertFalse(sourceFile.exists());

        verify(mEndOfFlowCallback).run();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testDownloadFlowForRecreatedActivity() throws IOException {
        mController = new PasswordCsvDownloadFlowController(mEndOfFlowCallback);
        PasswordCsvDownloadFlowControllerFactory.setControllerForTesting(mController);
        mController.showDialogAndStartFlow(
                mActivity, mProfile, true, true, mSettingsCustomTabLauncher);
        mActivity.getSupportFragmentManager().executePendingTransactions();

        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorBridge);
        when(mReauthenticatorBridge.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.positive_button).performClick();

        ArgumentCaptor<Callback<Boolean>> resultCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mReauthenticatorBridge).reauthenticate(resultCallbackCaptor.capture());
        resultCallbackCaptor.getValue().onResult(true);

        ShadowActivity shadowActivity = shadowOf(mActivity);
        Intent startedIntent = shadowActivity.peekNextStartedActivityForResult().intent;
        // Verify that the create document intent was triggered (creating file in Downloads for
        // exported passwords).
        assertEquals(Intent.ACTION_CREATE_DOCUMENT, startedIntent.getAction());

        // Recreate the activity to simulate its previous destruction and check that no
        // crashes occur. This scenario could happen in case of system resource pressure while the
        // file chooser activity is shown.
        mActivity.recreate();
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testRecordsCorrectDialogTypeOldGms() throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        OLD_GMS_DOWNLOAD_FLOW_LAST_STEP_HISTOGRAM,
                        DownloadCsvFlowStep.DISMISSED_DIALOG);

        mController = new PasswordCsvDownloadFlowController(mEndOfFlowCallback);
        mController.showDialogAndStartFlow(
                mActivity, mProfile, true, false, mSettingsCustomTabLauncher);
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.negative_button).performClick();
        verify(mEndOfFlowCallback).run();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordsCorrectDialogTypeNoGms() throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        NO_GMS_DOWNLOAD_FLOW_LAST_STEP_HISTOGRAM,
                        DownloadCsvFlowStep.DISMISSED_DIALOG);

        mController = new PasswordCsvDownloadFlowController(mEndOfFlowCallback);
        mController.showDialogAndStartFlow(
                mActivity, mProfile, false, false, mSettingsCustomTabLauncher);
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.negative_button).performClick();
        verify(mEndOfFlowCallback).run();
        histogramWatcher.assertExpected();
    }

    private File setUpTempAutoExportedCsv(String data) throws IOException {
        File sourceFile = File.createTempFile("ChromePasswords", "csv", null);
        sourceFile.deleteOnExit();

        FileWriter writer = new FileWriter(sourceFile);
        writer.write(TEST_FILE_DATA);
        writer.close();

        when(mLoginDbDeprecationUtilBridge.getAutoExportCsvFilePath(eq(mProfile)))
                .thenReturn(sourceFile.getAbsolutePath());
        return sourceFile;
    }

    private void verifyFakeDataWasCopiedOver(File destinationFile) throws IOException {
        BufferedReader input =
                new BufferedReader(new FileReader(destinationFile.getAbsolutePath()));
        List<String> lines = new ArrayList<>();
        String line;
        while ((line = input.readLine()) != null) {
            lines.add(line);
        }
        String data = String.join("\n", lines);
        input.close();
        assertEquals(TEST_FILE_DATA, data);
    }
}
