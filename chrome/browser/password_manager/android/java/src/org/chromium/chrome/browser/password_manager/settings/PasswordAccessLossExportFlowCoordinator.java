// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.FragmentActivity;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
import org.chromium.chrome.browser.lifetime.ApplicationLifetime;
import org.chromium.chrome.browser.password_manager.PasswordAccessLossDialogHelper;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Coordinates the export flow for the password access loss warning. It can be triggered only if the
 * user doesn't have GMS Core installed or if their local passwords migration failed. The flow has
 * the following steps: 1) Displays the dialog offering to export passwords to the file on disk and
 * clear the profile store (see {@link PasswordAccessLossExportDialogCoordinator}). 2) Displays the
 * dialog instructing the user to import the passwords in GMS Core (see {@link
 * PasswordAccessLossImportDialogCoordinator}). This step is executed only if GMS Core is installed
 * and up to date.
 */
public class PasswordAccessLossExportFlowCoordinator
        implements PasswordAccessLossExportDialogCoordinator.Observer {
    private final FragmentActivity mActivity;
    private final Profile mProfile;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Runnable mChromeShutDownRunnable;
    private final PasswordAccessLossExportDialogCoordinator mExportDialogCoordinator;

    private final @PasswordAccessLossWarningType int mWarningType;

    public PasswordAccessLossExportFlowCoordinator(
            FragmentActivity activity,
            Profile profile,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mActivity = activity;
        mProfile = profile;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mWarningType = getAccessLossWarningType();
        // In case the warning is prompted for the NEW_GMS_CORE_MIGRATION_FAILED case, the user will
        // be redirected to the import flow in GMS Core. Therefore, Chrome should be restarted
        // instead of shut down so that it doesn't interfere with GMS Core opening.
        boolean shouldRestartChrome = mWarningType == PasswordAccessLossWarningType.NO_GMS_CORE;
        mChromeShutDownRunnable = () -> ApplicationLifetime.terminate(shouldRestartChrome);
        mExportDialogCoordinator =
                new PasswordAccessLossExportDialogCoordinator(mActivity, mProfile, this);
    }

    @VisibleForTesting
    public PasswordAccessLossExportFlowCoordinator(
            FragmentActivity activity,
            Profile profile,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            PasswordAccessLossExportDialogCoordinator exportDialogCoordinator,
            Runnable chromeShutDownRunnable) {
        mActivity = activity;
        mProfile = profile;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mWarningType = getAccessLossWarningType();
        mChromeShutDownRunnable = chromeShutDownRunnable;
        mExportDialogCoordinator = exportDialogCoordinator;
    }

    public void startExportFlow() {
        mExportDialogCoordinator.showExportDialog();
    }

    private void showImportInstructionDialog() {
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        PasswordManagerHelper passwordManagerHelper = PasswordManagerHelper.getForProfile(mProfile);
        new PasswordAccessLossImportDialogCoordinator(
                        mActivity.getApplicationContext(),
                        syncService,
                        mModalDialogManagerSupplier,
                        passwordManagerHelper,
                        mChromeShutDownRunnable)
                .showImportInstructionDialog();
    }

    private @PasswordAccessLossWarningType int getAccessLossWarningType() {
        PrefService prefService = UserPrefs.get(mProfile);
        return PasswordAccessLossDialogHelper.getAccessLossWarningType(prefService);
    }

    @Override
    public void onPasswordsDeletionFinished() {
        if (mWarningType != PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED) {
            mChromeShutDownRunnable.run();
            return;
        }
        showImportInstructionDialog();
    }
}
