// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.chromium.chrome.browser.password_manager.PasswordMetricsUtil.PASSWORD_MIGRATION_WARNING_EXPORT_METRICS_ID;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge.PasswordStoreObserver;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.password_manager.settings.DialogManager;
import org.chromium.chrome.browser.password_manager.settings.ExportFlowInterface;
import org.chromium.chrome.browser.password_manager.settings.NonCancelableProgressBar;
import org.chromium.chrome.browser.password_manager.settings.PasswordListObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningMediator.MigrationWarningOptionsHandler;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator of the password migration warning. */
public class PasswordMigrationWarningCoordinator
        implements MigrationWarningOptionsHandler, PasswordStoreObserver {
    /** The delay after which the progress bar will be displayed. */
    private static final int PROGRESS_BAR_DELAY_MS = 500;

    // The prefix for the histograms, which will be used log the export flow metrics.
    public static final String EXPORT_METRICS_ID =
            "PasswordManager.PasswordMigrationWarning.Export";
    private final PasswordMigrationWarningMediator mMediator;
    private final SyncConsentActivityLauncher mSyncConsentActivityLauncher;
    private final Context mContext;
    private final Profile mProfile;
    private final Class<? extends Fragment> mSyncSettingsFragment;

    private ExportFlowInterface mExportFlow;
    private PasswordMigrationWarningView mView;
    private FragmentManager mFragmentManager;
    private PasswordStoreBridge mPasswordStoreBridge;
    private DialogManager mProgressBarManager;

    public PasswordMigrationWarningCoordinator(
            Context context,
            Profile profile,
            BottomSheetController sheetController,
            SyncConsentActivityLauncher syncConsentActivityLauncher,
            Class<? extends Fragment> syncSettingsFragment,
            ExportFlowInterface exportFlow,
            Callback<PasswordListObserver> passwordListObserverCallback,
            PasswordStoreBridge passwordStoreBridge,
            @PasswordMigrationWarningTriggers int referrer,
            Callback<Throwable> exceptionReporter) {
        mContext = context;
        mProfile = profile;
        mSyncConsentActivityLauncher = syncConsentActivityLauncher;
        mSyncSettingsFragment = syncSettingsFragment;
        mExportFlow = exportFlow;
        mMediator = new PasswordMigrationWarningMediator(profile, this, referrer);
        mPasswordStoreBridge = passwordStoreBridge;
        PropertyModel model =
                PasswordMigrationWarningProperties.createDefaultModel(
                        mMediator::onShown, mMediator::onDismissed, mMediator);
        mMediator.initializeModel(model);
        passwordListObserverCallback.onResult(mMediator);
        mView =
                new PasswordMigrationWarningView(
                        context,
                        sheetController,
                        () -> {
                            mExportFlow.onResume();
                        },
                        exceptionReporter,
                        (reason, setFragmentWasCalled) -> {
                            mMediator.onSheetClosed(reason, setFragmentWasCalled);
                        });
        setUpModelChangeProcessors(model, mView);
    }

    public void showWarning() {
        mMediator.showWarning(ScreenType.INTRO_SCREEN);
    }

    static void setUpModelChangeProcessors(PropertyModel model, PasswordMigrationWarningView view) {
        PropertyModelChangeProcessor.create(
                model, view, PasswordMigrationWarningViewBinder::bindPasswordMigrationWarningView);
    }

    @Override
    public void startSyncConsentFlow() {
        mSyncConsentActivityLauncher.launchActivityIfAllowed(
                mContext, SigninAccessPoint.PASSWORD_MIGRATION_WARNING_ANDROID);
    }

    @Override
    public void openSyncSettings() {
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(mContext, mSyncSettingsFragment);
    }

    @Override
    public void startExportFlow(FragmentManager fragmentManager) {
        mFragmentManager = fragmentManager;
        mExportFlow.onCreate(
                new Bundle(),
                new ExportFlowInterface.Delegate() {
                    @Override
                    public Activity getActivity() {
                        Activity activity = ContextUtils.activityFromContext(mContext);
                        assert activity != null;
                        return activity;
                    }

                    @Override
                    public FragmentManager getFragmentManager() {
                        return fragmentManager;
                    }

                    @Override
                    public int getViewId() {
                        return R.id.fragment_container_view;
                    }

                    @Override
                    public void runCreateFileOnDiskIntent(Intent intent) {
                        mView.runCreateFileOnDiskIntent(intent);
                    }

                    @Override
                    public void onExportFlowSucceeded() {
                        ExportDeletionDialogFragment deletionDialogFragment =
                                new ExportDeletionDialogFragment();
                        deletionDialogFragment.initialize(
                                () -> {
                                    startPasswordsDeletion();
                                });
                        deletionDialogFragment.show(mFragmentManager, null);
                    }

                    @Override
                    public Profile getProfile() {
                        return mProfile;
                    }
                },
                PASSWORD_MIGRATION_WARNING_EXPORT_METRICS_ID);
        mExportFlow.startExporting();
    }

    @Override
    public void savePasswordsToDownloads(Uri passwordsFile) {
        mExportFlow.savePasswordsToDownloads(passwordsFile);
    }

    @Override
    public void resumeExportFlow() {
        mExportFlow.onResume();
    }

    @Override
    public void passwordsAvailable() {
        mExportFlow.passwordsAvailable();
    }

    public PasswordMigrationWarningMediator getMediatorForTesting() {
        return mMediator;
    }

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

    private void startPasswordsDeletion() {
        mProgressBarManager = new DialogManager(null);
        NonCancelableProgressBar progressBarDialogFragment =
                new NonCancelableProgressBar(
                        R.string.exported_passwords_deletion_in_progress_title);
        mProgressBarManager.showWithDelay(
                progressBarDialogFragment, mFragmentManager, PROGRESS_BAR_DELAY_MS);
        mPasswordStoreBridge.addObserver(this, true);
        mPasswordStoreBridge.clearAllPasswords();
    }

    private void onPasswordDeletionCompleted() {
        mProgressBarManager.hide(
                () -> {
                    mMediator.onDismissed(StateChangeReason.INTERACTION_COMPLETE);
                    mPasswordStoreBridge.removeObserver(this);
                    mPasswordStoreBridge.destroy();
                });
    }
}
