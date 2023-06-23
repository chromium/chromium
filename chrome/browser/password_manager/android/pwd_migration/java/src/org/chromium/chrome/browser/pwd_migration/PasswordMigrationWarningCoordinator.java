// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.password_manager.settings.PasswordListObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningMediator.MigrationWarningOptionsHandler;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.utils.ContextUtils;

/** The coordinator of the password migration warning. */
public class PasswordMigrationWarningCoordinator implements MigrationWarningOptionsHandler {
    private final PasswordMigrationWarningMediator mMediator;
    private final SyncConsentActivityLauncher mSyncConsentActivityLauncher;
    private final SettingsLauncher mSettingsLauncher;
    private final Context mContext;
    private final Class<? extends Fragment> mSyncSettingsFragment;

    private ExportFlowInterface mExportFlow;

    public PasswordMigrationWarningCoordinator(Context context, Profile profile,
            BottomSheetController sheetController,
            SyncConsentActivityLauncher syncConsentActivityLauncher,
            SettingsLauncher settingsLauncher, Class<? extends Fragment> syncSettingsFragment,
            ExportFlowInterface exportFlow,
            Callback<PasswordListObserver> passwordListObserverCallback) {
        mContext = context;
        mSyncConsentActivityLauncher = syncConsentActivityLauncher;
        mSettingsLauncher = settingsLauncher;
        mSyncSettingsFragment = syncSettingsFragment;
        mExportFlow = exportFlow;
        mMediator = new PasswordMigrationWarningMediator(profile, this);
        PropertyModel model = PasswordMigrationWarningProperties.createDefaultModel(
                mMediator::onDismissed, mMediator);
        mMediator.initializeModel(model);
        passwordListObserverCallback.onResult(mMediator);
        setUpModelChangeProcessors(model,
                new PasswordMigrationWarningView(
                        context, sheetController, () -> { mExportFlow.onResume(); }));
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
        // TODO(crbug.com/1445065): Hide the sheet when the export is done.
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
        });
        mExportFlow.startExporting();
    }
}
