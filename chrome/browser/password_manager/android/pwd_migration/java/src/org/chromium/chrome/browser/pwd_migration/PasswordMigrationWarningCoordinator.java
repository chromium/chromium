// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import android.content.Context;

import androidx.fragment.app.Fragment;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningMediator.MigrationWarningOptionsHandler;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator of the password migration warning. */
public class PasswordMigrationWarningCoordinator implements MigrationWarningOptionsHandler {
    private final PasswordMigrationWarningMediator mMediator;
    private final SettingsLauncher mSettingsLauncher;
    private final Context mContext;
    private final Class<? extends Fragment> mSyncSettingsFragment;

    public PasswordMigrationWarningCoordinator(Context context, Profile profile,
            BottomSheetController sheetController, SettingsLauncher settingsLauncher,
            Class<? extends Fragment> syncSettingsFragment) {
        mContext = context;
        mSettingsLauncher = settingsLauncher;
        mSyncSettingsFragment = syncSettingsFragment;
        mMediator = new PasswordMigrationWarningMediator(profile, this);
        PropertyModel model = PasswordMigrationWarningProperties.createDefaultModel(
                mMediator::onDismissed, mMediator);
        mMediator.initialize(model);
        setUpModelChangeProcessors(
                model, new PasswordMigrationWarningView(context, sheetController));
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
        // TODO(crbug.com/1454770): Launch the sync consent flow.
    }

    @Override
    public void openSyncSettings() {
        mSettingsLauncher.launchSettingsActivity(mContext, mSyncSettingsFragment);
    }

    @Override
    public void startExportFlow() {
        // TODO(crbug.com/1445065): Launch the password Export flow.
    }
}
