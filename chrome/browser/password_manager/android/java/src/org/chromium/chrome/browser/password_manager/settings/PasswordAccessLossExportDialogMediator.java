// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
import org.chromium.chrome.browser.password_manager.PasswordAccessLossDialogHelper;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge.PasswordStoreObserver;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.password_manager.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * The mediator for the password access loss warning export flow. It implements the {@link
 * ExportFlowInterface.Delegate} and contains the dialog buttons callbacks.
 */
class PasswordAccessLossExportDialogMediator
        implements ExportFlowInterface.Delegate,
                PasswordAccessLossExportDialogFragment.Delegate,
                PasswordListObserver,
                PasswordStoreObserver {
    /** The delay after which the progress bar will be displayed. */
    private static final int PROGRESS_BAR_DELAY_MS = 500;

    private final FragmentActivity mActivity;
    private final Profile mProfile;
    private final int mDialogViewId;
    private final PasswordAccessLossExportDialogFragment mExportDialogFragment;
    private ExportFlow mExportFlow;
    private PasswordStoreBridge mPasswordStoreBridge;
    private DialogManager mProgressBarManager;
    private final PasswordAccessLossExportDialogCoordinator.Observer mExportDialogObserver;

    public PasswordAccessLossExportDialogMediator(
            FragmentActivity activity,
            Profile profile,
            int dialogViewId,
            PasswordAccessLossExportDialogFragment exportDialogFragment,
            PasswordStoreBridge passwordStoreBridge,
            PasswordAccessLossExportDialogCoordinator.Observer exportDialogObserver) {
        mActivity = activity;
        mProfile = profile;
        mDialogViewId = dialogViewId;
        mExportDialogFragment = exportDialogFragment;
        mPasswordStoreBridge = passwordStoreBridge;
        mExportDialogObserver = exportDialogObserver;
    }

    public String getDialogTitle() {
        PrefService prefService = UserPrefs.get(mProfile);
        if (PasswordAccessLossDialogHelper.getAccessLossWarningType(prefService)
                == PasswordAccessLossWarningType.NO_GMS_CORE) {
            return mActivity.getString(R.string.access_loss_export_dialog_title_no_gms);
        }
        return mActivity.getString(R.string.access_loss_export_dialog_title);
    }

    public void handlePositiveButtonClicked() {
        PasswordManagerHandlerProvider.getForProfile(mProfile).addObserver(this);
        mExportFlow = new ExportFlow();
        // TODO (crbug.com/354876446): Handle metrics in separate CL.
        mExportFlow.onCreate(new Bundle(), this, "");
        mExportFlow.startExporting();
    }

    // Implementation of ExportFlowInterface.Delegate.
    @Override
    public Activity getActivity() {
        return mActivity;
    }

    @Override
    public FragmentManager getFragmentManager() {
        return mActivity.getSupportFragmentManager();
    }

    @Override
    public int getViewId() {
        return mDialogViewId;
    }

    @Override
    public void runCreateFileOnDiskIntent(Intent intent) {
        assert mExportDialogFragment != null
                : "Password access loss export dialog was already dismissed!";
        mExportDialogFragment.runCreateFileOnDiskIntent();
    }

    @Override
    public void onExportFlowSucceeded() {
        deletePasswords();
    }

    @Override
    public Profile getProfile() {
        return mProfile;
    }

    // Implementation of PasswordListObserver.
    @Override
    public void passwordListAvailable(int count) {
        if (mExportFlow == null) return;
        mExportFlow.passwordsAvailable();
    }

    @Override
    public void passwordExceptionListAvailable(int count) {
        // Not used here.
    }

    // Implementation of PasswordAccessLossExportDialogFragment.Delegate.
    @Override
    public void onDocumentCreated(Uri uri) {
        if (mExportFlow != null) {
            mExportFlow.savePasswordsToDownloads(uri);
        }
        mExportDialogFragment.dismiss();
    }

    @Override
    public void onResume() {
        if (mExportFlow == null) return;
        mExportFlow.onResume();
    }

    @Override
    public void onExportFlowFailed() {
        mExportDialogFragment.dismiss();
    }

    @Override
    public void onExportFlowCanceled() {
        destroy();
    }

    // Implementation of PasswordStoreObserver.
    @Override
    public void onSavedPasswordsChanged(int count) {
        if (count == 0) {
            onPasswordDeletionCompleted();
        }
    }

    @Override
    public void onEdit(PasswordStoreCredential credential) {
        // Won't be used. It's overridden to implement {@link PasswordStoreObserver}.
    }

    private void deletePasswords() {
        // This additional check protects from the case when migration succeeds while export flow
        // was executing. In this case the `UseUpmLocalAndSeparateStoresState` preference would have
        // been changed to `kOn`;
        // TODO (crbug.com/354876446): Introduce passwords deleted metrics in a separate CL.
        if (!shouldDeleteAllPasswords()) {
            destroy();
            return;
        }

        mProgressBarManager = new DialogManager(null);
        NonCancelableProgressBar progressBarDialogFragment =
                new NonCancelableProgressBar(
                        R.string.exported_passwords_deletion_in_progress_title);
        mProgressBarManager.showWithDelay(
                progressBarDialogFragment,
                mActivity.getSupportFragmentManager(),
                PROGRESS_BAR_DELAY_MS);
        mPasswordStoreBridge.addObserver(this, true);
        mPasswordStoreBridge.clearAllPasswordsFromProfileStore();
    }

    private boolean shouldDeleteAllPasswords() {
        PrefService prefService = UserPrefs.get(mProfile);
        if (PasswordAccessLossDialogHelper.getAccessLossWarningType(prefService)
                == PasswordAccessLossWarningType.NO_GMS_CORE) return true;
        if (prefService.getInteger(Pref.PASSWORDS_USE_UPM_LOCAL_AND_SEPARATE_STORES)
                == /* UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending */ 1) return true;
        return false;
    }

    private void onPasswordDeletionCompleted() {
        mProgressBarManager.hide(
                () -> {
                    destroy();
                    mPasswordStoreBridge.removeObserver(this);
                    mPasswordStoreBridge.destroy();
                    mExportDialogObserver.onPasswordsDeletionFinished();
                });
    }

    private void destroy() {
        if (mExportDialogFragment.getDialog() != null) {
            mExportDialogFragment.dismiss();
        }
        mExportFlow = null;
        if (PasswordManagerHandlerProvider.getForProfile(mProfile).getPasswordManagerHandler()
                != null) {
            PasswordManagerHandlerProvider.getForProfile(mProfile).removeObserver(this);
        }
    }
}
