// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static android.app.Activity.RESULT_CANCELED;
import static android.app.Activity.RESULT_OK;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.getExportFlowFinalStepHistogramName;

import android.app.Dialog;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.widget.Button;
import android.widget.TextView;

import androidx.fragment.app.FragmentActivity;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowContentResolver;
import org.robolectric.shadows.ShadowDialog;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileProviderUtils;
import org.chromium.base.FileProviderUtils.FileProviderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.PasswordAccessLossWarningExportStep;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.FakePasswordManagerHandler;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridgeJni;
import org.chromium.chrome.browser.password_manager.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.components.browser_ui.test.BrowserUiDummyFragmentActivity;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/** Tests for {@link PasswordAccessLossExportDialogCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DoNotBatch(reason = "The ReauthenticationManager setup should not leak between tests.")
@EnableFeatures(
        ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
@DisableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING)
public class PasswordAccessLossExportDialogCoordinatorTest {
    private static final Uri TEMP_EXPORT_FILE_URI = Uri.parse("tmp/fake/test/path/file.ext");
    private static final Uri SAVED_EXPORT_FILE_URI = Uri.parse("fake/test/path/file.ext");
    // 10 minutes in milliseconds
    private static final long TIME_PASSED_SINCE_LAST_AUTH_MINS = 1000L * 60 * 10;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();
    private PasswordAccessLossExportDialogCoordinator mCoordinator;
    private FragmentActivity mActivity;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private FileProviderUtil mFileProviderUtil;
    @Mock private InputStream mInputStream;
    @Mock private OutputStream mOutputStream;
    @Mock private PasswordStoreBridge.Natives mPasswordStoreBridgeJniMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;
    @Mock private PasswordAccessLossExportDialogCoordinator.Observer mPasswordsDeletionFinished;
    private FakePasswordManagerHandler mPasswordManagerHandler;

    public void setUp(@PasswordAccessLossWarningType int type) {
        mJniMocker.mock(PasswordStoreBridgeJni.TEST_HOOKS, mPasswordStoreBridgeJniMock);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        mJniMocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeJniMock);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        setUpAccessLossWarningType(type);

        mActivity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class)
                        .create()
                        .start()
                        .resume()
                        .get();
        mCoordinator =
                new PasswordAccessLossExportDialogCoordinator(
                        mActivity, mProfile, mPasswordsDeletionFinished);
    }

    private void setUpPasswordManagerHandler() {
        // Fakes password manager provider needed for passwords serialization.
        mPasswordManagerHandler =
                new FakePasswordManagerHandler(mCoordinator.getMediatorForTesting());
        PasswordManagerHandlerProvider provider =
                PasswordManagerHandlerProvider.getForProfile(mProfile);
        provider.setPasswordManagerHandlerForTest(mPasswordManagerHandler);
    }

    private void setUpPasswordStoreBridge() {
        doAnswer(
                        invocation -> {
                            mCoordinator.getMediatorForTesting().onSavedPasswordsChanged(0);
                            return null;
                        })
                .when(mPasswordStoreBridgeJniMock)
                .clearAllPasswordsFromProfileStore(anyLong());
    }

    private void setUpReauthenticationManager() {
        // Sets up re-authentication, which is required before exporting passwords.
        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setSkipSystemReauth(true);
    }

    private void setUpContentResolver() {
        // Mocks writing passwords to the file.
        FileProviderUtils.setFileProviderUtil(mFileProviderUtil);
        when(mFileProviderUtil.getContentUriFromFile(any())).thenReturn(TEMP_EXPORT_FILE_URI);
        ShadowContentResolver shadowContentResolver =
                shadowOf(ContextUtils.getApplicationContext().getContentResolver());
        shadowContentResolver.registerInputStream(TEMP_EXPORT_FILE_URI, mInputStream);
        shadowContentResolver.registerOutputStream(SAVED_EXPORT_FILE_URI, mOutputStream);
    }

    private void setUpAccessLossWarningType(@PasswordAccessLossWarningType int type) {
        when(mPasswordManagerUtilBridgeJniMock.getPasswordAccessLossWarningType(any()))
                .thenReturn(type);
        if (type == PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED) {
            when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
            when(mPrefService.getInteger(Pref.PASSWORDS_USE_UPM_LOCAL_AND_SEPARATE_STORES))
                    .thenReturn(/* UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending */ 1);
        }
    }

    @Test
    public void testExportDialogStringsForNewGmsCoreAndMigrationFailed() {
        setUp(PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        mCoordinator.showExportDialog();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Resources resources = RuntimeEnvironment.getApplication().getResources();
        Dialog dialog = ShadowDialog.getLatestDialog();
        assertEquals(
                resources.getString(R.string.access_loss_export_dialog_title),
                ((TextView) dialog.findViewById(R.id.title)).getText());
        assertEquals(
                resources.getString(R.string.access_loss_export_dialog_message),
                ((TextView) dialog.findViewById(R.id.message)).getText());
        assertEquals(
                resources.getString(R.string.access_loss_export_dialog_positive_button_text),
                ((Button) dialog.findViewById(R.id.positive_button)).getText());
        assertEquals(
                resources.getString(R.string.cancel),
                ((Button) dialog.findViewById(R.id.negative_button)).getText());
    }

    @Test
    public void testExportDialogStringsForNoGmsCore() {
        setUp(PasswordAccessLossWarningType.NO_GMS_CORE);
        mCoordinator.showExportDialog();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Resources resources = RuntimeEnvironment.getApplication().getResources();
        Dialog dialog = ShadowDialog.getLatestDialog();
        assertEquals(
                resources.getString(R.string.access_loss_export_dialog_title_no_gms),
                ((TextView) dialog.findViewById(R.id.title)).getText());
        assertEquals(
                resources.getString(R.string.access_loss_export_dialog_message),
                ((TextView) dialog.findViewById(R.id.message)).getText());
        assertEquals(
                resources.getString(R.string.access_loss_export_dialog_positive_button_text),
                ((Button) dialog.findViewById(R.id.positive_button)).getText());
        assertEquals(
                resources.getString(R.string.cancel),
                ((Button) dialog.findViewById(R.id.negative_button)).getText());
    }

    @Test
    public void testExportFlow() throws IOException {
        setUp(PasswordAccessLossWarningType.NO_GMS_CORE);
        mCoordinator.showExportDialog();
        setUpPasswordManagerHandler();
        setUpPasswordStoreBridge();
        setUpReauthenticationManager();
        setUpContentResolver();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.positive_button).performClick();

        // Check that passwords are serialized.
        assertEquals(1, mPasswordManagerHandler.getSerializationInvocationCount());
        // Reply with the serialized passwords count & file name.
        mPasswordManagerHandler
                .getExportSuccessCallback()
                .onResult(10, TEMP_EXPORT_FILE_URI.toString());

        // Biometric re-auth should have been triggered. Need to fake successful authentication to
        // proceed.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.BULK);
        // Simulates the `onResume` call after re-authentication.
        mCoordinator.getMediatorForTesting().onResume();
        Robolectric.flushForegroundThreadScheduler();

        ShadowActivity shadowActivity = shadowOf(mActivity);
        Intent startedIntent = shadowActivity.getNextStartedActivityForResult().intent;
        // Verify that the create document intent was triggered (creating file in Downloads for
        // exported passwords).
        assertEquals(Intent.ACTION_CREATE_DOCUMENT, startedIntent.getAction());

        // Setup input stream to pretend to have something to read when first called, and nothing on
        // the second call.
        when(mInputStream.read(any(byte[].class))).thenReturn(0).thenReturn(-1);

        // Return the result of the create document intent (the file name).
        shadowActivity.receiveResult(
                startedIntent, RESULT_OK, new Intent().setData(SAVED_EXPORT_FILE_URI));
        Robolectric.flushForegroundThreadScheduler();
        // Dialog is expected to be dismissed now.
        assertFalse(dialog.isShowing());
        // Verify that writing to the exported file was called.
        verify(mInputStream, times(2)).read(any(byte[].class));
        verify(mOutputStream).write(any(byte[].class), anyInt(), anyInt());
        verify(mPasswordStoreBridgeJniMock).clearAllPasswordsFromProfileStore(anyLong());
    }

    @Test
    public void testExportFlowWithReauthFailed() throws IOException {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getExportFlowFinalStepHistogramName(
                                        PasswordAccessLossWarningType.NO_GMS_CORE),
                                PasswordAccessLossWarningExportStep.NO_SCREEN_LOCK_SET_UP)
                        .build();

        setUp(PasswordAccessLossWarningType.NO_GMS_CORE);
        mCoordinator.showExportDialog();
        setUpPasswordManagerHandler();
        setUpPasswordStoreBridge();

        // Fake that screen lock wasn't set up.
        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.UNAVAILABLE);

        setUpContentResolver();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.positive_button).performClick();

        // Reply with the serialized passwords count & file name.
        mPasswordManagerHandler
                .getExportSuccessCallback()
                .onResult(10, TEMP_EXPORT_FILE_URI.toString());

        // Simulates the `onResume` call after re-authentication.
        mCoordinator.getMediatorForTesting().onResume();
        Robolectric.flushForegroundThreadScheduler();

        histogram.assertExpected();
    }

    @Test
    public void testExportFlowWithSavingFileFailed() throws IOException {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getExportFlowFinalStepHistogramName(
                                        PasswordAccessLossWarningType
                                                .NEW_GMS_CORE_MIGRATION_FAILED),
                                PasswordAccessLossWarningExportStep.SAVE_PWD_FILE_FAILED)
                        .build();

        setUp(PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        mCoordinator.showExportDialog();
        setUpPasswordManagerHandler();
        setUpPasswordStoreBridge();
        setUpReauthenticationManager();
        setUpContentResolver();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.positive_button).performClick();

        // Reply with the serialized passwords count & file name.
        mPasswordManagerHandler
                .getExportSuccessCallback()
                .onResult(10, TEMP_EXPORT_FILE_URI.toString());

        // Biometric re-auth should have been triggered. Need to fake successful authentication to
        // proceed.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.BULK);
        // Simulates the `onResume` call after re-authentication.
        mCoordinator.getMediatorForTesting().onResume();
        Robolectric.flushForegroundThreadScheduler();

        ShadowActivity shadowActivity = shadowOf(mActivity);
        Intent startedIntent = shadowActivity.getNextStartedActivityForResult().intent;
        // Verify that the create document intent was triggered (creating file in Downloads for
        // exported passwords).
        assertEquals(Intent.ACTION_CREATE_DOCUMENT, startedIntent.getAction());

        // Return the result of the create document intent (the file name).
        shadowActivity.receiveResult(
                startedIntent, RESULT_CANCELED, new Intent().setData(SAVED_EXPORT_FILE_URI));
        Robolectric.flushForegroundThreadScheduler();
        // Dialog is expected to be dismissed now.
        assertFalse(dialog.isShowing());
        // Verify that the exported passwords file was  not written to and the passwords were not
        // deleted.
        verify(mInputStream, times(0)).read(any(byte[].class));
        verify(mOutputStream, times(0)).write(any(byte[].class), anyInt(), anyInt());
        verify(mPasswordStoreBridgeJniMock, times(0)).clearAllPasswordsFromProfileStore(anyLong());

        histogram.assertExpected();
    }

    @Test
    public void testExportFlowWithReauthExpired() throws IOException {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getExportFlowFinalStepHistogramName(
                                        PasswordAccessLossWarningType
                                                .NEW_GMS_CORE_MIGRATION_FAILED),
                                PasswordAccessLossWarningExportStep.AUTHENTICATION_EXPIRED)
                        .build();

        setUp(PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        mCoordinator.showExportDialog();
        setUpPasswordManagerHandler();
        setUpPasswordStoreBridge();
        setUpReauthenticationManager();
        setUpContentResolver();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.positive_button).performClick();

        // Reply with the serialized passwords count & file name.
        mPasswordManagerHandler
                .getExportSuccessCallback()
                .onResult(10, TEMP_EXPORT_FILE_URI.toString());

        // Simulates that time has passed while Chrome was backgrounded for 10 minutes and the
        // authentication expired.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis() - TIME_PASSED_SINCE_LAST_AUTH_MINS,
                ReauthenticationManager.ReauthScope.BULK);
        // Simulates the `onResume` call after foregrounding Chrome.
        mCoordinator.getMediatorForTesting().onResume();
        Robolectric.flushForegroundThreadScheduler();

        histogram.assertExpected();
    }

    @Test
    public void testDialogIsDismissedWhenExportFailsDueToSerialization() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getExportFlowFinalStepHistogramName(
                                        PasswordAccessLossWarningType.NO_GMS_CORE),
                                PasswordAccessLossWarningExportStep.PWD_SERIALIZATION_FAILED)
                        .build();

        setUp(PasswordAccessLossWarningType.NO_GMS_CORE);
        mCoordinator.showExportDialog();
        setUpPasswordManagerHandler();
        setUpReauthenticationManager();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.positive_button).performClick();

        // Check that passwords are serialized.
        assertEquals(1, mPasswordManagerHandler.getSerializationInvocationCount());
        // Pretend password manager handler to encounter an error when serializing passwords.
        mPasswordManagerHandler.getExportErrorCallback().onResult("Test error");

        // Biometric re-auth should have been triggered. Need to fake successful authentication to
        // proceed.
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.BULK);
        // Simulates the `onResume` call after re-authentication.
        mCoordinator.getMediatorForTesting().onResume();
        Robolectric.flushForegroundThreadScheduler();

        // Dialog is expected to be dismissed now.
        assertFalse(dialog.isShowing());

        histogram.assertExpected();
    }

    @Test
    public void testPasswordsAreNotDeletedIfUseUpmLocalAndSeparateStoresIsOn() {
        // This test checks the edge case, when the export dialog was displayed, but the migration
        // succeeded in while it was showing.
        setUp(PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        when(mPrefService.getInteger(Pref.PASSWORDS_USE_UPM_LOCAL_AND_SEPARATE_STORES))
                .thenReturn(/* UseUpmLocalAndSeparateStoresState::kOn */ 2);
        // Notification that the export flow succeeded should trigger passwords deletion.
        mCoordinator.getMediatorForTesting().onExportFlowSucceeded();
        Robolectric.flushForegroundThreadScheduler();

        // Password deletion should not be triggered in this case (because it would remove passwords
        // from GMS Core).
        verify(mPasswordStoreBridgeJniMock, times(0)).clearAllPasswordsFromProfileStore(anyLong());
    }

    @Test
    public void testExportDialogNegativeButtonClick() {
        var histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                getExportFlowFinalStepHistogramName(
                                        PasswordAccessLossWarningType.NO_GMS_CORE),
                                PasswordAccessLossWarningExportStep.EXPORT_CANCELED)
                        .build();
        setUp(PasswordAccessLossWarningType.NO_GMS_CORE);
        mCoordinator.showExportDialog();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.negative_button).performClick();

        // Dialog is expected to be dismissed now.
        assertFalse(dialog.isShowing());
        histogram.assertExpected();
    }
}
