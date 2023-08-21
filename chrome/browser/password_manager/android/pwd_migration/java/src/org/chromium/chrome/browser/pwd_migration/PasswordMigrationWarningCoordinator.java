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
import org.chromium.chrome.browser.password_manager.settings.PasswordListObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningMediator.MigrationWarningOptionsHandler;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator of the password migration warning. */
public class PasswordMigrationWarningCoordinator implements MigrationWarningOptionsHandler {
    // The prefix for the histograms, which will be used log the export flow metrics.
    public static final String EXPORT_METRICS_ID =
            "PasswordManager.PasswordMigrationWarning.Export";
    private final PasswordMigrationWarningMediator mMediator;
    private final SyncConsentActivityLauncher mSyncConsentActivityLauncher;
    private final SettingsLauncher mSettingsLauncher;
    private final Context mContext;
    private final Class<? extends Fragment> mSyncSettingsFragment;

    private ExportFlowInterface mExportFlow;
    private PasswordMigrationWarningView mView;
    private FragmentManager mFragmentManager;
    private PasswordStoreBridge mPasswordStoreBridge;

    public PasswordMigrationWarningCoordinator(Context context, Profile profile,
            BottomSheetController sheetController,
            SyncConsentActivityLauncher syncConsentActivityLauncher,
            SettingsLauncher settingsLauncher, Class<? extends Fragment> syncSettingsFragment,
            ExportFlowInterface exportFlow,
            Callback<PasswordListObserver> passwordListObserverCallback,
            PasswordStoreBridge passwordStoreBridge, @PasswordMigrationWarningTriggers int referrer,
            Callback<Throwable> exceptionReporter) {
        mContext = context;
        mSyncConsentActivityLauncher = syncConsentActivityLauncher;
        mSettingsLauncher = settingsLauncher;
        mSyncSettingsFragment = syncSettingsFragment;
        mExportFlow = exportFlow;
        mMediator = new PasswordMigrationWarningMediator(profile, this, referrer);
        mPasswordStoreBridge = passwordStoreBridge;
        PropertyModel model = PasswordMigrationWarningProperties.createDefaultModel(
                mMediator::onShown, mMediator::onDismissed, mMediator);
        mMediator.initializeModel(model);
        passwordListObserverCallback.onResult(mMediator);
        mView = new PasswordMigrationWarningView(
                context, sheetController, () -> { mExportFlow.onResume(); }, exceptionReporter);
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
        mSettingsLauncher.launchSettingsActivity(mContext, mSyncSettingsFragment);
    }

    @Override
    public void startExportFlow(FragmentManager fragmentManager) {
        mFragmentManager = fragmentManager;
        mExportFlow.onCreate(new Bundle(), new ExportFlowInterface.Delegate() {
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
                deletionDialogFragment.initialize(mFragmentManager, () -> {
                    mMediator.onDismissed(StateChangeReason.INTERACTION_COMPLETE);
                    mPasswordStoreBridge.destroy();
                }, mPasswordStoreBridge);
                deletionDialogFragment.show(mFragmentManager, null);
            }
        }, PASSWORD_MIGRATION_WARNING_EXPORT_METRICS_ID);
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
}
