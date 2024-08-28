// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.FragmentActivity;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
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
    private final PasswordAccessLossExportDialogCoordinator mExportDialogCoordinator;

    public PasswordAccessLossExportFlowCoordinator(
            FragmentActivity activity,
            Profile profile,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        mActivity = activity;
        mProfile = profile;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mExportDialogCoordinator =
                new PasswordAccessLossExportDialogCoordinator(mActivity, mProfile, this);
    }

    @VisibleForTesting
    public PasswordAccessLossExportFlowCoordinator(
            FragmentActivity activity,
            Profile profile,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            PasswordAccessLossExportDialogCoordinator exportDialogCoordinator) {
        mActivity = activity;
        mProfile = profile;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
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
                        passwordManagerHelper)
                .showImportInstructionDialog();
    }

    @Override
    public void onPasswordsDeletionFinished() {
        PrefService prefService = UserPrefs.get(mProfile);
        if (PasswordManagerHelper.getAccessLossWarningType(prefService)
                != PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED) {
            return;
        }
        showImportInstructionDialog();
    }
}
