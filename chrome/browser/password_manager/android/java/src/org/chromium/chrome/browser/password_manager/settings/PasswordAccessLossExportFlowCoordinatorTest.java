// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.fragment.app.FragmentActivity;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

/** Tests for {@link PasswordAccessLossExportFlowCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordAccessLossExportFlowCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private Profile mProfile;
    @Mock private PasswordAccessLossExportDialogCoordinator mExportDialogCoordinator;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private Runnable mChromeShutDownRunnable;
    private FragmentActivity mActivity;
    private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private FakeModalDialogManager mModalDialogManager;
    private PasswordAccessLossExportFlowCoordinator mCoordinator;

    @Before
    public void setUp() {
        mJniMocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeJniMock);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mActivity =
                Robolectric.buildActivity(FragmentActivity.class).create().start().resume().get();
        mModalDialogManager = new FakeModalDialogManager(ModalDialogManager.ModalDialogType.APP);
        mModalDialogManagerSupplier = () -> mModalDialogManager;
    }

    private void setUpAccessLossWarningType(@PasswordAccessLossWarningType int type) {
        when(mPasswordManagerUtilBridgeJniMock.getPasswordAccessLossWarningType(any()))
                .thenReturn(type);
    }

    private void initializeExportFlowCoordinator() {
        // The coordinator needs to be created after the warning type is set for testing because it
        // will store the warning type in a member variable.
        mCoordinator =
                new PasswordAccessLossExportFlowCoordinator(
                        mActivity,
                        mProfile,
                        mModalDialogManagerSupplier,
                        mExportDialogCoordinator,
                        mChromeShutDownRunnable);
    }

    private void setUpSyncService() {
        SyncService syncService = Mockito.mock(SyncService.class);
        SyncServiceFactory.setInstanceForTesting(syncService);
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
    public void testShowsExportDialog() {
        setUpAccessLossWarningType(PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        initializeExportFlowCoordinator();
        mCoordinator.startExportFlow();

        verify(mExportDialogCoordinator).showExportDialog();
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
    public void testShowsImportDialogWhenDeletionFinished() {
        setUpAccessLossWarningType(PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED);
        initializeExportFlowCoordinator();
        setUpSyncService();
        mCoordinator.onPasswordsDeletionFinished();

        // Import dialog should be displayed.
        Assert.assertNotNull(mModalDialogManager.getShownDialogModel());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_LOCAL_PASSWORDS_ANDROID_ACCESS_LOSS_WARNING)
    public void testDoesNotShowImportDialogForNoGmsCoreWarningTypeAndRestartsChrome() {
        setUpAccessLossWarningType(PasswordAccessLossWarningType.NO_GMS_CORE);
        initializeExportFlowCoordinator();
        mCoordinator.onPasswordsDeletionFinished();

        // Chrome should be terminated.
        verify(mChromeShutDownRunnable).run();
        // Import dialog should not be displayed.
        Assert.assertNull(mModalDialogManager.getShownDialogModel());
    }
}
