// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static android.app.Activity.RESULT_OK;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Dialog;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.fragment.app.FragmentActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;
import org.robolectric.shadows.ShadowContentResolver;
import org.robolectric.shadows.ShadowDialog;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.ContentUriUtils.FileProviderUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.FakePasswordManagerHandler;
import org.chromium.chrome.browser.password_manager.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.components.browser_ui.test.BrowserUiDummyFragmentActivity;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/** Tests for {@link PasswordAccessLossExportDialogFragment} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordAccessLossExportDialogTest {
    private static final Uri TEMP_EXPORT_FILE_URI = Uri.parse("tmp/fake/test/path/file.ext");
    private static final Uri SAVED_EXPORT_FILE_URI = Uri.parse("fake/test/path/file.ext");
    private PasswordAccessLossExportDialogCoordinator mCoordinator;
    private PasswordAccessLossExportDialogMediator mMediator;
    private PasswordAccessLossExportDialogFragment mFragment;
    private FragmentActivity mActivity;
    @Mock private Profile mProfile;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private FileProviderUtil mFileProviderUtil;
    @Mock private InputStream mInputStream;
    @Mock private OutputStream mOutputStream;
    private FakePasswordManagerHandler mPasswordManagerHandler;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        when(mProfileProvider.getOriginalProfile()).thenReturn(mProfile);

        mActivity =
                Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class)
                        .create()
                        .start()
                        .resume()
                        .get();
        View dialogView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.password_access_loss_export_dialog_view, null);
        mFragment = new PasswordAccessLossExportDialogFragment();
        mMediator =
                new PasswordAccessLossExportDialogMediator(
                        mActivity, mProfile, dialogView, mFragment);
        mCoordinator =
                new PasswordAccessLossExportDialogCoordinator(
                        mActivity, mFragment, mMediator, dialogView);
    }

    private void setUpPasswordManagerHandler() {
        // Fakes password manager provider needed for passwords serialization.
        mPasswordManagerHandler = new FakePasswordManagerHandler(mMediator);
        PasswordManagerHandlerProvider provider =
                PasswordManagerHandlerProvider.getForProfile(mProfile);
        provider.setPasswordManagerHandlerForTest(mPasswordManagerHandler);
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
        ContentUriUtils.setFileProviderUtil(mFileProviderUtil);
        when(mFileProviderUtil.getContentUriFromFile(any())).thenReturn(TEMP_EXPORT_FILE_URI);
        ShadowContentResolver shadowContentResolver =
                shadowOf(ContextUtils.getApplicationContext().getContentResolver());
        shadowContentResolver.registerInputStream(TEMP_EXPORT_FILE_URI, mInputStream);
        shadowContentResolver.registerOutputStream(SAVED_EXPORT_FILE_URI, mOutputStream);
    }

    @Test
    public void testDialogStrings() {
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
    @EnableFeatures(
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
    @DisableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING)
    public void testPositiveButtonClick() throws IOException {
        mCoordinator.showExportDialog();
        setUpPasswordManagerHandler();
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
        mFragment.onResume();
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
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
    @DisableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING)
    public void testDialogIsDismissedWhenExportFails() {
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
        mFragment.onResume();
        Robolectric.flushForegroundThreadScheduler();

        // Dialog is expected to be dismissed now.
        assertFalse(dialog.isShowing());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
    @DisableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PWD_MIGRATION_WARNING)
    public void testNegativeButtonClick() {
        mCoordinator.showExportDialog();
        mActivity.getSupportFragmentManager().executePendingTransactions();

        Dialog dialog = ShadowDialog.getLatestDialog();
        dialog.findViewById(R.id.negative_button).performClick();

        // Dialog is expected to be dismissed now.
        assertFalse(dialog.isShowing());
    }
}
