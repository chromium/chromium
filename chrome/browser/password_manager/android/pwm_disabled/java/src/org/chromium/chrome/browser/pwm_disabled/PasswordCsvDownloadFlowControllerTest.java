// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import static android.app.Activity.RESULT_OK;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Dialog;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Looper;

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
import org.robolectric.shadows.ShadowDialog;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridge;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.test.BrowserUiDummyFragmentActivity;
import org.chromium.ui.widget.ToastManager;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/** Tests for {@link PasswordsCsvDownloadDialogController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowLooper.class, ShadowToast.class})
@DoNotBatch(reason = "The ReauthenticationManager setup should not leak between tests.")
public class PasswordCsvDownloadFlowControllerTest {
    private static final String TEST_FILE_DATA =
            "name,url,username,password,note\n"
                    + "example.com,https://example.com/,Someone,Secret,\"Note Line 1\n"
                    + "\"Note Line 2\"";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private PasswordCsvDownloadFlowController mController;
    private FragmentActivity mActivity;
    @Mock Runnable mEndOfFlowCallback;
    @Mock Profile mProfile;
    @Mock ReauthenticatorBridge mReauthenticatorBridge;
    @Mock LoginDbDeprecationUtilBridge.Natives mLoginDbDeprecationUtilBridge;

    @Before
    public void setUp() {
        mActivity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class)
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
        mController = new PasswordCsvDownloadFlowController(mEndOfFlowCallback);
        mController.showDialogAndStartFlow(mActivity, mProfile, true);
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
    }

    @Test
    public void testScreenLockAvailableAuthFailed() {
        mController = new PasswordCsvDownloadFlowController(mEndOfFlowCallback);
        mController.showDialogAndStartFlow(mActivity, mProfile, true);
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
    }

    @Test
    public void testDownloadFlow() throws IOException {
        // Make sure the auto-exported csv is set up.
        File sourceFile = setUpTempAutoExportedCsv(TEST_FILE_DATA);

        mController = new PasswordCsvDownloadFlowController(mEndOfFlowCallback);
        mController.showDialogAndStartFlow(mActivity, mProfile, true);
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
        shadowOf(Looper.getMainLooper()).idle();
        assertFalse(dialog.isShowing());

        verifyFakeDataWasCopiedOver(destinationFile);

        // The source file should be deleted.
        assertFalse(sourceFile.exists());

        verify(mEndOfFlowCallback).run();
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
